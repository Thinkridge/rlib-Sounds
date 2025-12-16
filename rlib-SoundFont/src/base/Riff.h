#pragma once


namespace rlib {

	namespace riff {

		namespace inner {

			template <int N> constexpr std::array<int8_t, N> toArray(const char(&s)[N + 1]) {
				std::array<int8_t, N> arr{};
				for (size_t i = 0; i < N; ++i) {
					arr[i] = static_cast<int8_t>(s[i]);
				}
				return arr;
			}
		}

#pragma pack( push )
#pragma pack( 1 )
		struct ChunkHead {
			std::array<int8_t, 4> id;
			uint32_t size = 0;
		};
		struct ChunkList : ChunkHead {
			std::array<int8_t, 4> type;
		};
#pragma pack( pop )

		inline void readRiff(
			std::istream& is,
			std::function<void(const ChunkList&)> fListBegin = [](const ChunkList&) {},			// LIST 入口
			std::function<void()> fListEnd = [] {},												// LIST 出口
			std::function<void(const ChunkHead&, std::istream&)> fReadData = [](const ChunkHead& ch, std::istream& is) {
				is.seekg(ch.size, std::ios::cur);
				if (is.fail()) {
					throw std::runtime_error("seekg error");
				}
			}
		) {

			const auto readChunk = [&is]() {
				ChunkHead c;
				assert(sizeof(c) == 8);
				if (is.read(reinterpret_cast<char*>(&c), sizeof(c)).gcount() < sizeof(c)) {
					throw std::runtime_error("chunk size error");
				}
				return c;
			};

			const auto Y = [](auto func) {
				return [=](auto&&... xs) {
					return func(func, std::forward<decltype(xs)>(xs)...);
				};
			};
			const auto f = Y(
				[&](auto f, const ChunkHead& ch) -> uint64_t {
					ChunkList chunkList;
					static_cast<ChunkHead&>(chunkList) = ch;
					assert(sizeof(chunkList) == 12);
					if (is.read(reinterpret_cast<char*>(chunkList.type.data()), sizeof(chunkList.type)).gcount() < sizeof(chunkList.type)) {
						throw std::runtime_error("chunk size error");
					}
					uint64_t readed = sizeof(chunkList.type);

					fListBegin(chunkList);

					while (readed < ch.size) {
						auto ch = readChunk();
						readed += sizeof(ch);
						if (ch.id == inner::toArray<4>("LIST")) {
							readed += f(f, ch);
						} else {
							fReadData(ch, is);			// データ読み込み
							readed += ch.size;

							if (is.tellg() % 2) {		// 奇数なら偶数位置に
								is.seekg(1, std::ios::cur);
								readed++;
							}

						}
					}

					fListEnd();

					return readed;
				}
			);

			const auto riffInfo = readChunk();
			if (riffInfo.id != inner::toArray<4>("RIFF")) {
				throw std::runtime_error("riff chunk error");
			}
			f(riffInfo);

		}


		class Chunk {
		public:
			struct Chunks {			// (default)
				std::array<int8_t, 4>	type;
				std::vector<Chunk>		chunks;
			};
			struct Data {
				std::array<int8_t, 4>	id;
				std::vector<int8_t>		data;
			};
			struct DataRef {		// 実体を自前では持たない
				struct Info {
					std::array<int8_t, 4>	id;
					const void* data;
					uint32_t				size;
				};
				std::function<Info()>	funcGet;
			};

			Chunk()
			{}
			Chunk(const Chunks& s)
				:m_value(s)
			{}
			Chunk(Chunks&& s)
				:m_value(std::move(s))
			{}
			Chunk(const Data& s)
				:m_value(s)
			{}
			Chunk(Data&& s)
				:m_value(std::move(s))
			{}
			Chunk(const DataRef& s)
				:m_value(s)
			{}
			Chunk(DataRef&& s)
				:m_value(std::move(s))
			{}
			Chunk(const Chunk& s)
				:m_value(s.m_value)
			{}
			Chunk(Chunk&&) = default;

			template <typename S> Chunk& operator=(const S& s) {
				*this = Chunk(s);
				return *this;
			}

			Chunk& operator=(const Chunk& s) {
				if (this != &s) {
					m_value = s.m_value;
				}
				return *this;
			}
			Chunk& operator=(Chunk&& s)noexcept {
				if (this != &s) {
					m_value = std::move(s.m_value);
				}
				return *this;
			}

			// List 取得
			bool isChunks()const {
				return std::holds_alternative<Chunks>(m_value);
			}
			auto& ensureChunks() {
				if (!std::holds_alternative<Chunks>(m_value)) {
					m_value = Chunks();
				}
				return std::get<Chunks>(m_value);
			}
			const auto& chunks() const {
				if (!std::holds_alternative<Chunks>(m_value)) {
					throw std::logic_error("invalid type");
				}
				return std::get<Chunks>(m_value);
			}

			// Data 取得
			bool isData()const {
				return std::holds_alternative<Data>(m_value);
			}
			auto& ensureData() {
				if (!std::holds_alternative<Data>(m_value)) {
					m_value = Data();
				}
				return std::get<Data>(m_value);
			}
			const auto& data() const {
				if (!std::holds_alternative<Data>(m_value)) {
					throw std::logic_error("invalid type");
				}
				return std::get<Data>(m_value);
			}

			// DataRef
			bool isDataRef()const {
				return std::holds_alternative<DataRef>(m_value);
			}
			auto& setDataRef(const std::function<DataRef::Info()>& f) {
				if (!std::holds_alternative<DataRef>(m_value)) {
					m_value = DataRef();
				}
				auto &dataRef = std::get<DataRef>(m_value);
				dataRef.funcGet = f;
				return dataRef;
			}
			const auto& dataRef() const {
				if (!std::holds_alternative<DataRef>(m_value)) {
					throw std::logic_error("invalid type");
				}
				return std::get<DataRef>(m_value);
			}

			static Chunk fromStream(std::istream& is) {
				using namespace inner;
				Chunk chunk;
				std::vector<Chunk*> vCurrent = { &chunk };

				readRiff(
					is,
					[&](const ChunkList& info) {
						if (info.id != toArray<4>("RIFF")) {
							auto& c = (*vCurrent.rbegin())->ensureChunks().chunks.emplace_back();
							vCurrent.push_back(&c);
						}
						auto& c = **vCurrent.rbegin();
						c.ensureChunks().type = info.type;
					},
					[&] {
						vCurrent.pop_back();
					},
					[&](const ChunkHead& info, std::istream& is) {
						auto& c = (*vCurrent.rbegin())->ensureChunks().chunks.emplace_back().ensureData();
						c.id = info.id;
						c.data.resize(info.size);
						if (is.read(reinterpret_cast<char*>(c.data.data()), c.data.size()).gcount() < static_cast<std::intmax_t>(c.data.size())) {
							throw std::runtime_error("chunk size error");
						}
					});

				return chunk;
			}

			friend std::ostream& operator<<(std::ostream& os, const Chunk& chunk) {
				using namespace inner;

				if (!chunk.isChunks()) {
					throw std::runtime_error("chunk type error");
				}

				struct F {
					static std::uintmax_t getSize(const Chunk& chunk) {
						if (chunk.isChunks()) {
							std::uintmax_t n = sizeof(ChunkList) - sizeof(ChunkHead);
							for (auto& c : std::get<Chunks>(chunk.m_value).chunks) {
								n += sizeof(ChunkHead) + getSize(c);
							}
							return n;
						}
						if (chunk.isData()) {
							auto& data = std::get<Data>(chunk.m_value);
							return data.data.size() + (data.data.size() % 2);	// 奇数バイト考慮
						}
						if (chunk.isDataRef()) {
							auto info = std::get<DataRef>(chunk.m_value).funcGet();
							return info.size + (info.size % 2);				// 奇数バイト考慮
						}
						assert(false);
						throw std::logic_error("type error");
					};

					static void write(std::ostream& os, const Chunk& chunk) {
						if (chunk.isChunks()) {
							auto& chunks = std::get<Chunks>(chunk.m_value);
							ChunkList cl;
							cl.size = static_cast<decltype(cl.size)>(getSize(chunk));
							cl.id = toArray<4>("LIST");
							cl.type = chunks.type;
							os.write(reinterpret_cast<const char*>(&cl), sizeof(cl));
							for (auto& c : chunks.chunks) {
								write(os, c);
							}
						} else if (chunk.isData()) {
							auto& data = std::get<Data>(chunk.m_value);
							ChunkHead h;
							h.id = data.id;
							h.size = static_cast<decltype(h.size)>(data.data.size());	// ココは奇数バイト考慮しない
							os.write(reinterpret_cast<const char*>(&h), sizeof(h));
							os.write(reinterpret_cast<const char*>(data.data.data()), data.data.size());
							if (data.data.size() % 2) os << '\0';		// 奇数バイトなら
						} else if (chunk.isDataRef()) {
							auto& dataRef = std::get<DataRef>(chunk.m_value);
							auto info = dataRef.funcGet();
							ChunkHead h;
							h.id = info.id;
							h.size = static_cast<decltype(h.size)>(info.size);			// ココは奇数バイト考慮しない
							os.write(reinterpret_cast<const char*>(&h), sizeof(h));
							os.write(static_cast<const char*>(info.data), info.size);
							if (info.size % 2) os << '\0';		// 奇数バイトなら
							return;
						} else {
							assert(false);
							throw std::logic_error("type error");
						}
					}
				};

				ChunkList cl;
				cl.id = toArray<4>("RIFF");
				cl.size = [&] {
					const auto n = F::getSize(chunk);
					if (n > (std::numeric_limits<uint32_t>::max)()) {
						throw std::runtime_error("chunk size overflow");
					}
					return static_cast<decltype(cl.size)>(n);
				}();
				auto& chunks = std::get<Chunks>(chunk.m_value);
				cl.type = chunks.type;
				os.write(reinterpret_cast<const char*>(&cl), sizeof(cl));
				for (auto& c : chunks.chunks) {
					F::write(os, c);
				}
				return os;
			}

		private:
			std::variant<Chunks, Data, DataRef>	m_value;
		};


	}


}
