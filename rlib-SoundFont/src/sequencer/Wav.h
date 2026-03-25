#pragma once

#include "../base/Riff.h"

namespace rlib {

	class Wav {
#pragma pack( push )
#pragma pack( 1 )
		struct WaveformatEx {
			int16_t		wFormatTag;			// format type
			int16_t		nChannels;			// number of channels (i.e. mono, stereo...)
			uint32_t	nSamplesPerSec;		// sample rate
			uint32_t	nAvgBytesPerSec;	// for buffer estimation
			int16_t		nBlockAlign;		// block size of data
			int16_t		wBitsPerSample;		// Number of bits per sample of mono data
			// int16_t		cbSize;				// The count in bytes of the size of extra information (after cbSize)
		};
	public:
		template<typename T> struct Mono {
			using value_type = T;
			T c{};
			Mono() = default;
			explicit Mono(T v) : c(v) {}
			// template<typename U> Mono(const U& s) : c(static_cast<T>(s.c)) {}
			static constexpr int channels = 1;
		};
		template<typename T> struct Stereo {
			using value_type = T;
			T l{}, r{};
			Stereo() = default;
			explicit Stereo(T l_, T r_) : l(l_), r(r_) {}
			template<typename U> Stereo(const U& s) : l(static_cast<T>(s.l)), r(static_cast<T>(s.r)) {}	// 型違いもOKとする。コンパイルエラーにならないので余計な変換が入らないよう注意すべし。
			static constexpr int channels = 2;
		};
#pragma pack( pop )
	public:
		struct {
			uint32_t	nSamplesPerSec = 44100;		// sample rate
		}m_format;
		std::variant<
			std::vector<Mono<uint8_t>>,
			std::vector<Stereo<uint8_t>>,
			std::vector<Mono<int16_t>>,
			std::vector<Stereo<int16_t>>,
			std::vector<Mono<float>>,
			std::vector<Stereo<float>>,
			std::vector<Mono<double>>,
			std::vector<Stereo<double>>
		> m_audioData;

		template<typename Sample> struct SampleTraits {
			using value_type = typename Sample::value_type;
			static constexpr int channels = Sample::channels;
			static constexpr int bits = sizeof(value_type) * 8;
			static constexpr int formatTag = std::is_floating_point_v<value_type> ? 3 : 1;	// WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM
		};

	public:
		Wav()
			: m_audioData(std::in_place_type<std::vector<Stereo<int16_t>>>)
		{}
		Wav(const Wav& s)
			:m_format(s.m_format)
			, m_audioData(s.m_audioData)
		{}
		Wav(Wav&& s)
			:m_format(std::move(s.m_format))
			, m_audioData(std::move(s.m_audioData))
		{}
		~Wav() {}

		template <class T> void setMode() {
			m_audioData = std::vector<T>();
		}

		template<class T> std::vector<T>& data() {
			static_assert(
				std::disjunction_v<
				std::is_same<T, Mono<uint8_t>>,
				std::is_same<T, Stereo<uint8_t>>,
				std::is_same<T, Mono<int16_t>>,
				std::is_same<T, Stereo<int16_t>>,
				std::is_same<T, Mono<float>>,
				std::is_same<T, Stereo<float>>,
				std::is_same<T, Mono<double>>,
				std::is_same<T, Stereo<double>>
				>,
				"Unsupported audio sample type"
				);
			return std::get<std::vector<T>>(m_audioData);
		}
		template<class T> const std::vector<T>& data() const {
			return std::get<std::vector<T>>(m_audioData);
		}

		void exportFile(std::ostream& os)const {
			using namespace riff;
			Chunk chunk;
			auto& chunks = chunk.ensureChunks();
			chunks.type = inner::toArray<4>("WAVE");

			{// fmt chunk
				auto& chunkFmt = chunks.chunks.emplace_back();
				auto& data = chunkFmt.ensureData();
				data.id = inner::toArray<4>("fmt ");
				data.data.resize(sizeof(WaveformatEx));
				auto& f = *reinterpret_cast<WaveformatEx*>(data.data.data());
				f.nSamplesPerSec = m_format.nSamplesPerSec;
				std::visit([&](auto const& buffer) {
					using Sample = typename std::decay_t<decltype(buffer)>::value_type;
					f.nChannels = SampleTraits<Sample>::channels;
					f.wBitsPerSample = SampleTraits<Sample>::bits;
					f.wFormatTag = SampleTraits<Sample>::formatTag;
				}, m_audioData);
				f.nBlockAlign = f.nChannels * f.wBitsPerSample / 8;
				f.nAvgBytesPerSec = f.nSamplesPerSec * f.nBlockAlign;
			}

			{// data chunk
				auto& chunkData = chunks.chunks.emplace_back();
				chunkData.setDataRef(
					[&] {
					Chunk::DataRef::Info info;
					info.id = inner::toArray<4>("data");
					std::visit([&](auto& v) {
						using Sample = typename std::decay_t<decltype(v)>::value_type;
						static_assert(std::is_trivially_copyable_v<Sample>, "Sample must be trivially copyable");
						info.data = v.data();
						info.size = static_cast<decltype(info.size)>(v.size() * sizeof(Sample));
					}, m_audioData);
					return info;
				});
			}

			os << chunk;
		}

		static Wav fromStream(std::istream& is) {
			using namespace riff;
			const auto toArray = [](const std::string& s) {
				std::array<std::byte, 4> arr{};
				for (auto i = 0; i < arr.size() && i < s.size(); i++) {
					arr[i] = static_cast<std::byte>(s[i]);
				}
				return arr;
			};
			const auto toString = [](const std::vector<std::byte>& s) {
				auto v(s);
				v.emplace_back();		// EOS
				return std::string(reinterpret_cast<const char*>(v.data()));
			};

			const auto chunks = Chunk::fromStream(is).chunks();
			if (chunks.type != toArray("WAVE")) {
				throw std::runtime_error("wave type error");
			}

			Wav wav;
			std::optional<WaveformatEx> waveformatEx;
			std::optional<const std::vector<std::byte>*> data;

			{// chunk 読み込み
				const std::map<std::vector<std::array<std::byte, 4>>, std::function<void(const std::vector<std::byte>&)>> map = {
					{{toArray("fmt ")},[&](const auto& v) {
						if (v.size() < sizeof(WaveformatEx)) {
							throw std::runtime_error("iver chunk error");
						};
						waveformatEx = *reinterpret_cast<const WaveformatEx*>(v.data());
						wav.m_format.nSamplesPerSec = waveformatEx->nSamplesPerSec;
					}},
					{{toArray("INFO"),toArray("INAM")},[&](const auto& v) {
					}},
					{{toArray("INFO"),toArray("IART")},[&](const auto& v) {
					}},
					{{toArray("INFO"),toArray("IGNR")},[&](const auto& v) {
					}},
					{{toArray("INFO"),toArray("IPRD")},[&](const auto& v) {
					}},
					{{toArray("JUNK")},[&](const auto& v) {
					}},
					{{toArray("data")},[&](const auto& v) {
						const std::vector<std::byte>* p = &v;
						data = p;
					}},
				};
				const auto Y = [](auto func) {
					return [=](auto&&... xs) {
						return func(func, std::forward<decltype(xs)>(xs)...);
					};
				};
				const auto f = Y(
					[&map](auto f, const std::vector<std::array<std::byte, 4>>& key, const Chunk::Chunks& chunks) -> void {
					const auto createKey = [&key](auto& type) {
						auto v(key);
						v.push_back(type);
						return v;
					};
					for (auto& chunk : chunks.chunks) {
						if (chunk.isData()) {
							if (auto j = map.find(createKey(chunk.data().id)); j != map.end()) {	// mapから検索
								j->second(chunk.data().data);
							} else {
								// throw std::runtime_error("unknown chunk error");					不明なチャンク
							}
						} else if (chunk.isChunks()) {
							f(f, createKey(chunk.chunks().type), chunk.chunks());
						} else {
							assert(false);
						}
					}
				}
				);
				f(std::vector<std::array<std::byte, 4>>{}, chunks);
			}

			if (!waveformatEx) throw std::runtime_error("not found fmt chunk");
			if (!data) throw std::runtime_error("not found data chunk");

			switch (waveformatEx->wBitsPerSample) {
			case 8:
				if (waveformatEx->nChannels == 1) {
					wav.m_audioData.emplace<std::vector<Mono<uint8_t>>>();
				} else if (waveformatEx->nChannels == 2) {
					wav.m_audioData.emplace<std::vector<Stereo<uint8_t>>>();
				} else throw std::runtime_error("unsupported channel count");
				break;
			case 16:
				if (waveformatEx->nChannels == 1) {
					wav.m_audioData.emplace<std::vector<Mono<int16_t>>>();
				} else if (waveformatEx->nChannels == 2) {
					wav.m_audioData.emplace<std::vector<Stereo<int16_t>>>();
				} else throw std::runtime_error("unsupported channel count");
				break;
			case 32:
				if (waveformatEx->wFormatTag == 3) {		// WAVE_FORMAT_IEEE_FLOAT
					if (waveformatEx->nChannels == 1) {
						wav.m_audioData.emplace<std::vector<Mono<float>>>();
					} else if (waveformatEx->nChannels == 2) {
						wav.m_audioData.emplace<std::vector<Stereo<float>>>();
					} else throw std::runtime_error("unsupported channel count");
				} else throw std::runtime_error("unsupported format");
				break;
			case 64:
				if (waveformatEx->wFormatTag == 3) {		// WAVE_FORMAT_IEEE_FLOAT
					if (waveformatEx->nChannels == 1) {
						wav.m_audioData.emplace<std::vector<Mono<double>>>();
					} else if (waveformatEx->nChannels == 2) {
						wav.m_audioData.emplace<std::vector<Stereo<double>>>();
					} else throw std::runtime_error("unsupported channel count");
				} else throw std::runtime_error("unsupported format");
				break;
			default:
				throw std::runtime_error("unsupported bit depth");
			}
			std::visit([&](auto& v) {
				const size_t count = (**data).size() / sizeof(typename std::decay_t<decltype(v)>::value_type);
				v.resize(count);
				std::memcpy(v.data(), (**data).data(), (**data).size());
			}, wav.m_audioData);

			return wav;
		}

	};

}
