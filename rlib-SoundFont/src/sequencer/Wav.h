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
		struct Mono8 {
			int8_t	c = 0;
			Mono8() {}
			Mono8(decltype(c) c_) :c(c_) {}
		};
		struct Mono16 {
			int16_t	c = 0;
			Mono16() {}
			Mono16(decltype(c) c_) :c(c_) {}
		};
		struct Stereo8 {
			int8_t	l = 0;
			int8_t	r = 0;
			Stereo8() {}
			Stereo8(decltype(l) l_, decltype(r) r_) :l(l_), r(r_) {}
		};
		struct Stereo16 {
			int16_t	l = 0;
			int16_t	r = 0;
			Stereo16() {}
			Stereo16(decltype(l) l_, decltype(r) r_) :l(l_), r(r_) {}
		};
#pragma pack( pop )
	public:
		struct {
			int16_t		wFormatTag = 1;				// format type
			uint32_t	nSamplesPerSec = 44100;		// sample rate
		}m_format;

		union {
			std::vector<Mono8>		m_mono8;
			std::vector<Mono16>		m_mono16;
			std::vector<Stereo8>	m_stereo8;
			std::vector<Stereo16>	m_stereo16;
		};

		enum class Mode {
			Mono8bit,		// モノラル 8bit
			Mono16bit,		// モノラル 16bit
			Stereo8bit,		// ステレオ 8bit
			Stereo16bit,	// ステレオ 16bit
		};
	private:
		Mode	m_mode;

		void clearData() {
			switch (m_mode) {
			case Mode::Mono8bit: {		typedef decltype(m_mono8) t;	m_mono8.~t();		break;	}
			case Mode::Mono16bit: {		typedef decltype(m_mono16) t;	m_mono16.~t();		break;	}
			case Mode::Stereo8bit: {	typedef decltype(m_stereo8) t;	m_stereo8.~t();		break;	}
			case Mode::Stereo16bit: {	typedef decltype(m_stereo16) t;	m_stereo16.~t();	break;	}
			default:			assert(false);
			}

		}
	public:
		Wav()
			:m_mode(Mode::Stereo16bit)
			, m_stereo16()
		{
		}
		Wav(const Wav& s)
			: m_mode(s.m_mode)
			, m_format(s.m_format)
		{
			switch (s.m_mode) {
			case Mode::Mono8bit:	new(&m_mono8) decltype(m_mono8)(s.m_mono8);				break;
			case Mode::Mono16bit:	new(&m_mono16) decltype(m_mono16)(s.m_mono16);			break;
			case Mode::Stereo8bit:	new(&m_stereo8) decltype(m_stereo8)(s.m_stereo8);		break;
			case Mode::Stereo16bit:	new(&m_stereo16) decltype(m_stereo16)(s.m_stereo16);	break;
			default:			assert(false);
			}

		}
		Wav(Wav&& s)
			: m_mode(s.m_mode)
			, m_format(std::move(s.m_format))
		{
			switch (s.m_mode) {
			case Mode::Mono8bit:	new(&m_mono8) decltype(m_mono8)(std::move(s.m_mono8));			break;
			case Mode::Mono16bit:	new(&m_mono16) decltype(m_mono16)(std::move(s.m_mono16));		break;
			case Mode::Stereo8bit:	new(&m_stereo8) decltype(m_stereo8)(std::move(s.m_stereo8));	break;
			case Mode::Stereo16bit:	new(&m_stereo16) decltype(m_stereo16)(std::move(s.m_stereo16));	break;
			default:			assert(false);
			}
		}
		~Wav() {
			clearData();
		}

		void setMode(Mode mode) {
			if (m_mode != mode) {
				clearData();
				m_mode = mode;
				switch (m_mode) {
				case Mode::Mono8bit:	new(&m_mono8) decltype(m_mono8)();			break;
				case Mode::Mono16bit:	new(&m_mono16) decltype(m_mono16)();		break;
				case Mode::Stereo8bit:	new(&m_stereo8) decltype(m_stereo8)();		break;
				case Mode::Stereo16bit:	new(&m_stereo16) decltype(m_stereo16)();	break;
				default:			assert(false);
				}
			}
		}
		Mode mode() const {
			return m_mode;
		}

		template<class T> std::vector<T>& data() {
			if constexpr (std::is_same_v<T, Mono8>) {
				if (m_mode != Mode::Mono8bit) throw std::logic_error("invalid type");
				return m_mono8;
			} else if constexpr (std::is_same_v<T, Mono16>) {
				if (m_mode != Mode::Mono16bit) throw std::logic_error("invalid type");
				return m_mono16;
			} else if constexpr (std::is_same_v<T, Stereo8>) {
				if (m_mode != Mode::Stereo8bit) throw std::logic_error("invalid type");
				return m_stereo8;
			} else if constexpr (std::is_same_v<T, Stereo16>) {
				if (m_mode != Mode::Stereo16bit) throw std::logic_error("invalid type");
				return m_stereo16;
			}
		}
		template<class T> const std::vector<T>& data() const {
			return const_cast<typename std::remove_const<typename std::remove_pointer<decltype(this)>::type>::type*>(this)->data<T>();
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
				f.wFormatTag = m_format.wFormatTag;
				f.nSamplesPerSec = m_format.nSamplesPerSec;
				switch (m_mode) {
				case Mode::Mono8bit:	f.nChannels = 1; f.wBitsPerSample = 8;	break;
				case Mode::Mono16bit:	f.nChannels = 1; f.wBitsPerSample = 16;	break;
				case Mode::Stereo8bit:	f.nChannels = 2; f.wBitsPerSample = 8;	break;
				case Mode::Stereo16bit:	f.nChannels = 2; f.wBitsPerSample = 16;	break;
				default:	assert(false);
				}
				f.nBlockAlign = f.nChannels * f.wBitsPerSample / 8;
				f.nAvgBytesPerSec = f.nSamplesPerSec* f.nBlockAlign;
			}

			{// data chunk
				auto& chunkData = chunks.chunks.emplace_back();
				chunkData.setDataRef(
					[&] {
						Chunk::DataRef::Info info;
						info.id = inner::toArray<4>("data");
						info.data = m_stereo16.data();
						info.size = static_cast<decltype(info.size)>(m_stereo16.size() * sizeof(*m_stereo16.data()));
						return info;
					});
			}

			os << chunk;
		}

		static Wav fromStream(std::istream& is) {

			using namespace riff;

			const auto toArray = [](const std::string& s) {
				std::array<int8_t, 4> arr{};
				for (auto i = 0; i < arr.size() && i < s.size(); i++) {
					arr[i] = s[i];
				}
				return arr;
			};
			const auto toString = [](const std::vector<int8_t>& s) {
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
			std::optional<const std::vector<int8_t>*> data;

			{// chunk 読み込み
				const std::map<std::vector<std::array<int8_t, 4>>, std::function<void(const std::vector<int8_t>&)>> map = {
					{{toArray("fmt ")},[&](const auto& v) {
						if (v.size() < sizeof(WaveformatEx)) {
							throw std::runtime_error("iver chunk error");
						};
						waveformatEx = *reinterpret_cast<const WaveformatEx*>(v.data());

						wav.m_format.wFormatTag = waveformatEx->wFormatTag;
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
						const std::vector<int8_t>* p = &v;
						data = p;
					}},
				};
				const auto Y = [](auto func) {
					return [=](auto&&... xs) {
						return func(func, std::forward<decltype(xs)>(xs)...);
					};
				};
				const auto f = Y(
					[&map](auto f, const std::vector<std::array<int8_t, 4>>& key, const Chunk::Chunks& chunks) -> void {
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
				f(std::vector<std::array<int8_t, 4>>{}, chunks);
			}

			if (!waveformatEx) throw std::runtime_error("not found fmt chunk");
			if (!data) throw std::runtime_error("not found data chunk");

			[&wav, &w = *waveformatEx, &data = **data] {
				switch (w.nChannels) {
				case 1:
					switch (w.wBitsPerSample) {
					case 8:
					{
						wav.setMode(Mode::Mono8bit);
						auto& v = wav.data<Mono8>();
						v.resize(data.size());
						std::memcpy(v.data(), data.data(), data.size());
						return;
					}
					case 16:
					{
						wav.setMode(Mode::Mono16bit);
						auto& v = wav.data<Mono16>();
						v.resize((data.size() + sizeof(Mono16) - 1) / sizeof(Mono16));
						std::memcpy(v.data(), data.data(), data.size());
						return;
					}
					}
				case 2:
					switch (w.wBitsPerSample) {
					case 8:
					{
						wav.setMode(Mode::Stereo8bit);
						auto& v = wav.data<Stereo8>();
						v.resize((data.size() + sizeof(Stereo8) - 1) / sizeof(Stereo8));
						std::memcpy(v.data(), data.data(), data.size());
						return;
					}
					case 16:
					{
						wav.setMode(Mode::Stereo16bit);
						auto& v = wav.data<Stereo16>();
						v.resize((data.size() + sizeof(Stereo16) - 1) / sizeof(Stereo16));
						std::memcpy(v.data(), data.data(), data.size());
						return;
					}
					}
				}
				throw std::runtime_error("unsupported format");
			}();

			return wav;
		}

	};

}
