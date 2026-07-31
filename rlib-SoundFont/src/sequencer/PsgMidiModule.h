#pragma once

#include <algorithm>
#include <future>
#include <map>
#include <optional>
#include <set>
#include <typeindex>

#include "../ymfm/ymfm_opn.h"
#include "../json/Json.h"
#include "../sequencer/MidiEvent.h"
#include "../sequencer/MidiModule.h"
#include "../sequencer/PsgRenderer.h"

namespace rlib::fm::psg {

	// PSG(SSG)音源用の MidiModule。
	//
	// fm::MidiModuleT (FmMidiModule.h) とほぼ同じ構造(チャンネル管理・
	// ボリューム/エクスプレッション/パン・ピッチベンド・マスターボリューム)
	// だが、ym2203_ssg はトーン(矩形波)のみのため音色(プログラム)の概念が無く、
	// プログラムチェンジ/バンクセレクトは受信のみ行い実際の音には影響しない。
	template <typename T = double> class MidiModuleT : public midi::MidiModuleBase<T> {
		using Bit14 = midi::utility::Bit14;

		struct Preset {
			std::string							name;
			typename RendererT<T>::Envelope		envelope;
			typename RendererT<T>::Mixer		mixer;
		};

		std::map<uint16_t, std::map< uint8_t, typename MidiModuleT<T>::Preset>> m_presets = {
		{0,{// bank0
			{0, {"piano",
				// AR   HR   DR   SL  RR		noise	tone
				{ 0.0, 0.0, 1.0, 0.3f, 1.0},	{0,	true},
			}},
			{1, {"sine",
				// AR   HR   DR   SL  RR		noise	tone
				{ 0.0, 0.0, 0.0, 1.0, 0.0},		{0,	true},
			}},
			{2, {"string",
				// AR   HR   DR   SL  RR		noise	tone
				{ 0.1f, 0.2f, 3.0, 0.8f, 1.0},	{0,	true},
			}},
			{10, {"snare",
				// AR   HR   DR   SL  RR		noise	tone
				{ 0.0, 0.0, 0.15f, 0.01f, 0.001f},	{1,	false},
			}},
		}}
		};

		RendererT<T>	m_renderer;

		struct Channel {
			const uint8_t	m_channel;
			uint16_t		m_bank = 0;
			uint8_t			m_programNo = 0;
			uint8_t			m_volume = 100;			// 0～127
			uint8_t			m_expression = 127;		// 0～127
			uint8_t			m_pan = 64;				// 0～127

			std::optional<std::pair<T, T>> m_gain;	// volume,expression,pan,masterVolume を掛け合わせたl,r振幅値 (0.0～1.0)

			struct Pitch {
				struct Info {
					int16_t	pitchBend = 0;		// -8192～8191
					int8_t	pitchBendRange = 0;	// 0～127 (半音単位)
					double	result = 0.0;		// 算出値(半音単位)
				};
			private:
				Info m_info;
			public:
				Pitch() {
					set(0, 2);	// デフォルト値
				}
				const Info& get()const { return m_info; }
				void set(decltype(Info::pitchBend) value, decltype(Info::pitchBendRange) range) {
					if (m_info.pitchBend == value && m_info.pitchBendRange == range) return;
					m_info.pitchBend = value;
					m_info.pitchBendRange = range;
					m_info.result = value / ((value >= 0 ? 8191 : 8192) / static_cast<double>(range));
				}
			}m_pitch;

			double			m_fineTune = 0.0;		// -1(半音下)～0～1(半音上)
			int8_t			m_coarseTune = 0;		// -64～0～63 (半音単位)

			Bit14					m_backselect;
			std::optional<Bit14>	m_nrpn;
			std::optional<Bit14>	m_rpn;
			Bit14					m_dataEntry;

			std::map<uint8_t, std::shared_ptr<typename RendererT<T>::Note>>	m_notes;

			Channel(uint8_t channel)
				:m_channel(channel)
			{
			}

			struct Less {
				typedef void is_transparent;
				bool operator()(const Channel& a, const Channel& b)				const { return a.m_channel < b.m_channel; }
				bool operator()(const decltype(m_channel) a, const Channel& b)	const { return a < b.m_channel; }
				bool operator()(const Channel& a, const decltype(m_channel) b)	const { return a.m_channel < b; }
			};

		};
		std::set<Channel, typename Channel::Less>	m_channels;
		uint16_t	m_masterVolume = 16383;	// マスターボリューム 0-～16383 (14bit)

		Channel& ensureChannel(uint8_t channel) {
			if (auto i = m_channels.find(channel); i != m_channels.end()) {
				return const_cast<Channel&>(*i);
			}
			auto i = m_channels.emplace(channel);
			return const_cast<Channel&>(*i.first);
		}

		void eventNoteOn(const midi::Event& event) {
			const midi::EventNoteOn &ev = static_cast<decltype(ev)>(event);
			if (ev.velocity == 0) return eventNoteOff(event);	// noteoff?

			auto& channel = ensureChannel(ev.channel);

			const auto itBank = m_presets.find(channel.m_bank);
			if (itBank == m_presets.end()) return;
			const auto it = itBank->second.find(channel.m_programNo);
			if (it == itBank->second.end()) return;
			const auto& program = it->second;

			typename RendererT<T>::PresetKey key;
			key.note = ev.note + channel.m_coarseTune;
			key.velocity = ev.velocity;
			key.fineTune = channel.m_fineTune;

			auto spProgram = m_renderer.createProgram(program.envelope, program.mixer);

			const auto spNote = m_renderer.createNote(key, spProgram, channel.m_pitch.get().result);
			channel.m_notes[ev.note] = spNote;

		}

		void eventNoteOff(const midi::Event& event) {
			const midi::EventNote& ev = static_cast<decltype(ev)>(event);			// NoteOn から来ることもあるので midi::EventNote に
			auto& channel = ensureChannel(ev.channel);

			if (auto i = channel.m_notes.find(ev.note); i != channel.m_notes.end()) {
				auto sp = i->second;
				if (!sp) throw std::runtime_error("eventNoteOff not note.");			// failsafe
				sp->setKeyoff();
			}

		}

		void eventControlChange(const midi::Event& event) {
			using namespace midi;
			const EventControlChange& ev = static_cast<decltype(ev)>(event);
			const auto channel = [&]()->Channel& {return ensureChannel(ev.channel); };

			const auto procDataEntry = [&](auto& ch) {
				if (ch.m_rpn) {
					switch (static_cast<EventControlChange::RpnType>(ch.m_rpn->value)) {
					case EventControlChange::RpnType::pitchBendRange:	// ベンドレンジ(ピッチ・ベンド・センシティビティ)
						ch.m_pitch.set(ch.m_pitch.get().pitchBend, ch.m_dataEntry.msb);
						break;
					case EventControlChange::RpnType::fineTune: {
						const int val = ch.m_dataEntry.value - 8192;
						ch.m_fineTune = val / ((val >= 0 ? 8191 : 8192) / 1.0);
						break;
					}
					case EventControlChange::RpnType::coarseTune:
						ch.m_coarseTune = ch.m_dataEntry.msb - 64;
						break;
					default:
						break;
					}
				}
			};

			switch (ev.type) {
			case EventControlChange::Type::volume: {
				auto& ch = channel();
				ch.m_volume = std::min<decltype(Channel::m_volume)>(ev.value, 127);
				ch.m_gain = std::nullopt;	// 要再計算
				break;
			}
			case EventControlChange::Type::expression: {
				auto& ch = channel();
				ch.m_expression = std::min<decltype(Channel::m_expression)>(ev.value, 127);
				ch.m_gain = std::nullopt;	// 要再計算
				break;
			}
			case EventControlChange::Type::pan: {
				auto& ch = channel();
				ch.m_pan = std::min<decltype(Channel::m_pan)>(ev.value, 127);
				ch.m_gain = std::nullopt;	// 要再計算
				break;
			}
			case EventControlChange::Type::bankSelectMSB:
				channel().m_backselect.msb = ev.value;
				break;
			case EventControlChange::Type::bankSelectLSB:
				channel().m_backselect.lsb = ev.value;
				break;
			case EventControlChange::Type::nrpnLSB: {
				auto& ch = channel();
				ch.m_rpn.reset();
				ch.m_nrpn = channel().m_rpn.value_or(Bit14());
				ch.m_nrpn->lsb = ev.value;
				break;
			}
			case EventControlChange::Type::nrpnMSB: {
				auto& ch = channel();
				ch.m_rpn.reset();
				ch.m_nrpn = channel().m_rpn.value_or(Bit14());
				ch.m_nrpn->msb = ev.value;
				break;
			}
			case EventControlChange::Type::rpnLSB: {
				auto& ch = channel();
				ch.m_nrpn.reset();
				ch.m_rpn = channel().m_rpn.value_or(Bit14());
				ch.m_rpn->lsb = ev.value;
				break;
			}
			case EventControlChange::Type::rpnMSB: {
				auto& ch = channel();
				ch.m_nrpn.reset();
				ch.m_rpn = channel().m_rpn.value_or(Bit14());
				ch.m_rpn->msb = ev.value;
				break;
			}
			case EventControlChange::Type::dataEntryMSB: {
				auto& ch = channel();
				ch.m_dataEntry.lsb = 0;
				ch.m_dataEntry.msb = ev.value;
				procDataEntry(ch);
				break;
			}
			case EventControlChange::Type::dataEntryLSB: {
				auto& ch = channel();
				ch.m_dataEntry.lsb = ev.value;
				procDataEntry(ch);
				break;
			}
			default:
				break;
			}
		}

		void eventProgramChange(const midi::Event& event) {
			using namespace midi;
			const EventProgramChange& ev = static_cast<decltype(ev)>(event);
			auto& channel = ensureChannel(ev.channel);
			channel.m_programNo = ev.programNo;
			channel.m_bank = static_cast<int16_t>(channel.m_backselect.msb) * 0x80 + channel.m_backselect.lsb;
		}

		void eventPitchBend(const midi::Event& event) {
			using namespace midi;
			const auto& ev = static_cast<const EventPitchBend&>(event);
			auto& channel = ensureChannel(ev.channel);
			const auto& before = channel.m_pitch.get();
			channel.m_pitch.set(ev.pitchBend, before.pitchBendRange);

			// 発音中のNote に設定
			const auto pitch = channel.m_pitch.get().result;
			for (auto note : channel.m_notes) {
				if (note.second) {		// failsafe
					note.second->setPitchBend(pitch);
				}
			}
		}

		void eventSystemExclusive(const midi::Event& event) {
			using namespace midi;
			const EventSystemExclusive& ev = static_cast<decltype(ev)>(event);

			auto isMatch = [](const std::vector<uint8_t>& data, const std::initializer_list<int16_t>& pattern) {
				if (data.size() < pattern.size()) return false;
				auto it = data.begin();
				for (auto p : pattern) {
					if (p >= 0 && *it != p) return false;	// 0未満はワイルドカード
					it++;
				}
				return true;
			};

			// マスターボリューム
			if (isMatch(ev.data, { -1, 0x7f, -1, 0x4, 0x1, -1, -1 })) {
				Bit14 u;
				u.lsb = ev.data[5];
				u.msb = ev.data[6];
				m_masterVolume = u.value;
				for (auto& ch : m_channels) const_cast<Channel&>(ch).m_gain = std::nullopt;	// 全チャンネル要再計算
				return;
			}
		}

		void eventMeta(const midi::Event& event) {
			using namespace midi;
			const auto& ev = static_cast<const EventMeta&>(event);
			if (ev.type == EventMeta::Type::sequencerLocal) {
				try {
					const auto json = Json::parse(ev.getText());
					const auto& map = json["rlib-MML"]["opnpsg"].get<Json::Map>();
					for (const auto it : map) {
						const auto no = [&] {
							try {
								return std::stoi(it.first);
							} catch (...) {
								// std::cerr << e.what() << std::endl;
							}
							return -1;
						}();
						if (no < 0 || no>127) continue;

						auto& pg = m_presets[0][no];
						const auto& name = it.second["name"].get<std::string>();
						const auto& data = it.second["reg"].get<Json::Array>();
						if (data.size() != 7) continue;	// AR,HR,DR,SL,RR,noise,tone

						int count = 0;
						const auto valNum = [&] {			// int/doubleどちらで格納されていても取得できる(Json::get<double>の仕様)
							return data[count++].get<double>();
						};
						const auto valInt = [&] {
							return data[count++].get<std::intmax_t>();
						};

						pg.name = name;
						pg.envelope.attack = static_cast<T>(valNum());
						pg.envelope.hold = static_cast<T>(valNum());
						pg.envelope.decay = static_cast<T>(valNum());
						pg.envelope.sustain = static_cast<T>(valNum());
						pg.envelope.release = static_cast<T>(valNum());
						const auto noise = valInt();
						pg.mixer.noise = static_cast<uint8_t>(std::clamp<std::intmax_t>(noise, 0, 31));
						pg.mixer.tone = valInt() != 0;

					}

				} catch (...) {
				}
			}
		}

	public:
		const uint32_t		m_sampleRate;
		uint32_t getSampleRate()const override { return m_sampleRate; }

		void setMidiEvent(const midi::Event& ev)override {
			using namespace midi;

			static const std::map<std::type_index, void (MidiModuleT::*)(const midi::Event&)> map = {
				{typeid(EventNoteOn),			&MidiModuleT::eventNoteOn		},
				{typeid(EventNoteOff),			&MidiModuleT::eventNoteOff		},
				{typeid(EventControlChange),	&MidiModuleT::eventControlChange},
				{typeid(EventProgramChange),	&MidiModuleT::eventProgramChange},
				{typeid(EventPitchBend),		&MidiModuleT::eventPitchBend	},
				{typeid(EventMeta),				&MidiModuleT::eventMeta			},
				{typeid(EventSystemExclusive),	&MidiModuleT::eventSystemExclusive	},
			};
			if (auto i = map.find(typeid(ev)); i != map.end()) {
				(this->*(i->second))(ev);	// 実行
			}
		}


		// レンダリング(波形データ出力（結果配列がsize未満なら完了=無音）
		std::vector<typename midi::StereoSample<T>> readSamples(size_t size)override {
#ifdef DISABLE_THREADS
			constexpr auto asyncLaunch = std::launch::deferred;
#else
			constexpr auto asyncLaunch = std::launch::async;
#endif
			std::vector<std::future<std::vector<midi::StereoSample<T>>>> futureChannels;
			for (auto& channel : m_channels) {
				futureChannels.emplace_back(std::async(asyncLaunch, [self = &std::as_const(*this), &channel = const_cast<Channel&>(channel), size, asyncLaunch] {

					std::vector<T> resultMono;
					for (auto it = channel.m_notes.begin(); it != channel.m_notes.end();) {
						auto sp = it->second;
						if (!sp) throw std::runtime_error("not released note.");	// failsafe
						auto samples = sp->render(size);
						if (samples.size() < size)	it = channel.m_notes.erase(it);		// 終わっていればmapから破棄
						else						it++;
						if (resultMono.empty()) {		// 最初なら代入(加算不要)
							resultMono = std::move(samples);
						} else {
							resultMono.resize((std::max)(resultMono.size(), samples.size()));
							for (size_t i = 0; i < samples.size(); i++) {
								resultMono[i] += samples[i];
							}
						}
					}

					// 音量処理(channel.m_gain算出)
					if (!channel.m_gain) {
						const T n = midi::volumeGainTable<T>[channel.m_volume] * midi::volumeGainTable<T>[channel.m_expression] * (self->m_masterVolume * (static_cast<T>(1.0) / 16383)); // volume,expression,masterVolume
						const auto& pan = midi::panGainTable<T>[channel.m_pan];
						channel.m_gain = { n * pan.first, n * pan.second };
					}
					std::vector<midi::StereoSample<T>> result(resultMono.size());
					for (size_t i = 0; i < result.size(); i++) {
						result[i].l = resultMono[i] * channel.m_gain->first;
						result[i].r = resultMono[i] * channel.m_gain->second;
					}

					return result;
				}));

			}

			std::vector<midi::StereoSample<T>> result;
			for (auto& f : futureChannels) {
				auto samples = f.get();
				if (result.empty()) {		// 最初なら代入(加算不要)
					result = std::move(samples);
				} else {
					result.resize((std::max)(result.size(), samples.size()));
					for (size_t i = 0; i < samples.size(); i++) {
						result[i].l += samples[i].l;
						result[i].r += samples[i].r;
					}
				}
			}

			return result;
		}

		// Eventはリリース音も含めて全て処理されている状態か
		bool isSilence()const override {
			for (auto& ch : m_channels) {
				if (!ch.m_notes.empty()) return false;
			}
			return true;
		}

		MidiModuleT(uint32_t sampleRate)
			: m_renderer(sampleRate)
			, m_sampleRate(sampleRate)
		{
		}

		MidiModuleT(MidiModuleT&&) = default;
		MidiModuleT(const MidiModuleT&) = delete;
		MidiModuleT& operator=(const MidiModuleT&) = delete;
		~MidiModuleT() {}

		Json getPresetInfo()const {
			Json json;
			auto& m = json.ensureMap();
			auto& v = m["Presets"].ensureArray();
			for (auto& bank : m_presets) {
				for (auto& pr : bank.second) {
					auto& t = v.emplace_back();
					t["bank"] = bank.first;
					t["no"] = pr.first;
					t["name"] = pr.second.name;
				}
			}
			return json;
		}

	};


	using MidiModuleF = MidiModuleT<float>;
	using MidiModule = MidiModuleT<double>;

}
