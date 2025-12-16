#pragma once

#include <future>
#include <typeindex>

#include "../sequencer/MidiEvent.h"
#include "../sequencer/MidiModule.h"
#include "SoundfontRenderer.h"


namespace rlib::soundfont {


	template <typename T = double> class MidiModuleT : public midi::MidiModuleBase<T> {

		// pan用 振幅値(0.0～1.0)テーブル
		static const std::array<std::pair<T, T>, 128> panAmplitudeTable;

		struct PairByte {
			uint8_t			msb = 0;
			uint8_t			lsb = 0;
			uint16_t no()const {
				return (static_cast<uint16_t>(msb) << 7) | lsb;
			}
		};

		struct Channel{
			const uint8_t	m_channel;
			uint16_t		m_bank = 0;
			uint8_t			m_programNo = 0;
			uint8_t			m_volume = 100;			// 0～127
			uint8_t			m_pan = 64;				// 0～127

			struct Pitch{
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
				void set(decltype(Info::pitchBend) value, decltype(Info::pitchBendRange) range){
					if (m_info.pitchBend == value && m_info.pitchBendRange == range) return;
					m_info.pitchBend = value;
					m_info.pitchBendRange = range;
					m_info.result = value / ((value >= 0 ? 8191 : 8192) / static_cast<double>(range));
				}
			}m_pitch;

			double			m_fineTune = 0.0;		// -1(半音下)～0～1(半音上)  
			int8_t			m_coarseTune = 0;		// -64～0～63 (半音単位)

			PairByte				m_backselect;
			std::optional<PairByte>	m_nrpn;
			std::optional<PairByte>	m_rpn;
			PairByte				m_dataEntry;

			std::map<uint8_t, std::list<std::shared_ptr<typename RendererT<T>::Note>>>	m_notes;

			Channel(uint8_t channel)
				:m_channel(channel)
			{
				if (channel == 9) {
					m_bank = 0x80;		// soundfontのリズムパートは 128(0x80)
					m_backselect.lsb = m_bank & 0x7f;
					m_backselect.msb = m_bank >> 7;
				}
			}

			struct Less {
				typedef void is_transparent;
				bool operator()(const Channel& a, const Channel& b)				const { return a.m_channel < b.m_channel; }
				bool operator()(const decltype(m_channel) a, const Channel& b)	const { return a < b.m_channel; }
				bool operator()(const Channel& a, const decltype(m_channel) b)	const { return a.m_channel < b; }
			};

		};
		std::set<Channel, typename Channel::Less>	m_channels;

		Channel& ensureChannel(uint8_t channel) {
			if (auto i = m_channels.find(channel); i != m_channels.end() ){
				return const_cast<Channel&>(*i);
			}
			auto i = m_channels.emplace(channel);
			return const_cast<Channel&>(*i.first);
		}

		void eventNoteOn(const midi::Event& event) {
			const midi::EventNoteOn &ev = static_cast<decltype(ev)>(event);
			if (ev.velocity == 0) return eventNoteOff(event);	// noteoff?

			auto& channel = ensureChannel(ev.channel);
			typename Soundfont::PresetKey key;
			key.bank = channel.m_bank;
			key.presetNo = channel.m_programNo;
			key.note = ev.note + channel.m_coarseTune;
			key.velocity = ev.velocity;

			const auto makeRenderer = [&](const typename Soundfont::PresetKey key) {
				return std::make_shared<typename RendererT<T>::Note>(std::move(m_renderer.createNote(key)));
			};
			auto sp = makeRenderer(key);
			if (sp->isFinished() && (key.bank != 0 && key.bank != 128)) {		// 対象バンクに音がないなら
				key.bank = 0;
				sp = makeRenderer(key);		// bank 0 で試行
			}
			if( !sp->isFinished() ){
				channel.m_notes[ev.note].push_back(sp);
			}
		}

		void eventNoteOff(const midi::Event& event) {
			const midi::EventNote& ev = static_cast<decltype(ev)>(event);			// NoteOn から来ることもあるので midi::EventNote に
			auto& channel = ensureChannel(ev.channel);
			if (auto i = channel.m_notes.find(ev.note); i != channel.m_notes.end()) {
				for (auto sp : i->second) {
					if (!sp) throw std::runtime_error("eventNoteOff not note.");		// failsafe
					sp->setKeyoff();
				}
			}
		}

		void eventControlChange(const midi::Event& event) {
			using namespace midi;
			const EventControlChange& ev = static_cast<decltype(ev)>(event);
			const auto channel = [&]()->Channel& {return ensureChannel(ev.channel); };

			const auto procDataEntry = [&](auto& ch) {
				if (ch.m_rpn) {
					switch (static_cast<EventControlChange::RpnType>(ch.m_rpn->no())) {
					case EventControlChange::RpnType::pitchBendRange:	// ベンドレンジ(ピッチ・ベンド・センシティビティ)
						ch.m_pitch.set(ch.m_pitch.get().pitchBend, ch.m_dataEntry.msb);
						break;
					case EventControlChange::RpnType::fineTune:
						{
							const int val = ch.m_dataEntry.no() - 8192;
							ch.m_fineTune = val / ((val >= 0 ? 8191 : 8192) / 1.0);
						}
						break;
					case EventControlChange::RpnType::coarseTune:
						ch.m_coarseTune = ch.m_dataEntry.msb - 64;
						break;
					default:
						break;
					}
				}
			};

			switch(ev.type){
			case EventControlChange::Type::volume:
				channel().m_volume = ev.value;
				break;
			case EventControlChange::Type::pan:
				channel().m_pan = ev.value;
				break;
			case EventControlChange::Type::bankSelectMSB:
				channel().m_backselect.msb = ev.value;
				break;
			case EventControlChange::Type::bankSelectLSB:
				channel().m_backselect.lsb = ev.value;
				break;
			case EventControlChange::Type::nrpnLSB:
				{
					auto& ch = channel();
					ch.m_rpn.reset();
					ch.m_nrpn = channel().m_rpn.value_or(PairByte());
					ch.m_nrpn->lsb = ev.value;
				}
				break;
			case EventControlChange::Type::nrpnMSB:
				{
					auto& ch = channel();
					ch.m_rpn.reset();
					ch.m_nrpn = channel().m_rpn.value_or(PairByte());
					ch.m_nrpn->msb = ev.value;
				}
				break;
			case EventControlChange::Type::rpnLSB:
				{
					auto &ch = channel();
					ch.m_nrpn.reset();
					ch.m_rpn = channel().m_rpn.value_or(PairByte());
					ch.m_rpn->lsb = ev.value;
				}
				break;
			case EventControlChange::Type::rpnMSB:
				{
					auto& ch = channel();
					ch.m_nrpn.reset();
					ch.m_rpn = channel().m_rpn.value_or(PairByte());
					ch.m_rpn->msb = ev.value;
				}
				break;
			case EventControlChange::Type::dataEntryMSB:
				{
					auto& ch = channel();
					ch.m_dataEntry.lsb = 0;
					ch.m_dataEntry.msb = ev.value;
					procDataEntry(ch);
				}
				break;
			case EventControlChange::Type::dataEntryLSB:
				{
					auto& ch = channel();
					ch.m_dataEntry.lsb = ev.value;
					procDataEntry(ch);
				}
			break;
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
		}

	public:
		RendererT<T>	m_renderer;

		const uint32_t		m_sampleRate;

		void setMidiEvent(const midi::Event& ev)override {
			using namespace midi;

			static const std::map<std::type_index, void (MidiModuleT::*)(const midi::Event&)> map = {
				{typeid(EventNoteOn),			&MidiModuleT::eventNoteOn		},
				{typeid(EventNoteOff),			&MidiModuleT::eventNoteOff		},
				{typeid(EventControlChange),	&MidiModuleT::eventControlChange	},
				{typeid(EventProgramChange),	&MidiModuleT::eventProgramChange	},
				{typeid(EventPitchBend),		&MidiModuleT::eventPitchBend	},
			};
			if (auto i = map.find(typeid(ev)); i != map.end()) {
				(this->*(i->second))(ev);	// 実行
			}
		}

		// レンダリング(波形データ出力（結果配列がsize未満なら完了=無音）
		std::vector<midi::StereoSample<T>> readSamples(size_t size) override {
#ifdef DISABLE_THREADS
			constexpr auto asyncLaunch = std::launch::deferred;
#else
			constexpr auto asyncLaunch = std::launch::async;
#endif
			std::vector<std::future<std::vector<typename midi::StereoSample<T>>>> futureChannels;
			for (auto& channel : m_channels) {
				futureChannels.emplace_back(std::async(asyncLaunch, [&channel = const_cast<Channel&>(channel), size, asyncLaunch] {
#if 0
					const T pitch = channel.m_pitchBend / ((channel.m_pitchBend >= 0 ? 8191 : 8192) / static_cast<T>(2.0));	// とりあえずベンドレンジは2固定
					std::vector<typename RendererT<T>::Sample> result(size);
					for (auto it = channel.m_notes.begin(); it != channel.m_notes.end();) {
						auto& list = it->second;
						for (auto t = list.begin(); t != list.end();) {
							auto sp = *t;
							if (!sp) throw std::runtime_error("not released note.");	// failsafe
							const auto samples = sp->render(size, pitch);
							for (size_t i = 0; i < samples.size(); i++) {
								result[i].l += samples[i].l;
								result[i].r += samples[i].r;
							}
							if (sp->isFinished())	t = list.erase(t);		// 終わっていればlistから破棄
							else					t++;
						}
						if (list.size() <= 0)	it = channel.m_notes.erase(it);			// 終わっていればmapから破棄
						else					it++;
					}
#else
					std::vector<std::future<decltype((*(channel.m_notes[0].begin()))->render(0, 0))>> futures;
					for (const auto& note : channel.m_notes) {
						for (const auto& sp : note.second) {
							if (sp) {		// failsafe
								const auto pitch = channel.m_fineTune + channel.m_pitch.get().result;
								futures.emplace_back(std::async(asyncLaunch, [sp, size, pitch] {
									return sp->render(size, pitch);
								}));
							}
						}
					}

					std::vector<typename midi::StereoSample<T>> result;
					if (futures.size() <= 0) return result;

					for (auto& f : futures) {
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
					for (auto n = channel.m_notes.begin(); n != channel.m_notes.end();) {
						for (auto c = n->second.begin(); c != n->second.end();) {
							c = (!(*c) || (*c)->isFinished()) ? n->second.erase(c) : ++c;	// 終わっていればlistから破棄
						}
						n = n->second.size() <= 0 ? channel.m_notes.erase(n) : ++n;	// 終わっていればmapから破棄
					}
#endif

					// volume & pan 処理
					const auto pan = panAmplitudeTable[channel.m_pan];
					const auto vol = RendererT<T>::volumeAmplitudeTable[channel.m_volume];
					const auto mulL = pan.first * vol;
					const auto mulR = pan.second * vol;
					for (auto& r : result) {
						r.l *= mulL;
						r.r *= mulR;
					}

					return result;
				}));

			}

			std::vector<typename midi::StereoSample<T>> result;
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

#if 1
#if 0
			// マスターボリューム（下げる）
			for (size_t i = 0; i < result.size(); i++) {
				result[i].l *= static_cast<T>(2.0);
				result[i].r *= static_cast<T>(2.0);
			}
#else
			{// マスターボリューム＆簡易コンプ
				static const auto comp = [](auto& sample) {
					constexpr T masterVol = static_cast<T>(1.8);
					constexpr T threshold = static_cast<T>(0.7);
					constexpr T ratio = static_cast<T>(0.3);
					sample *= masterVol;
					if (sample > threshold ){
						sample = threshold + (sample - threshold) * ratio;
					} else if (sample < -threshold) {
						sample = -threshold + (sample + threshold) * ratio;
					}
				};
				for (size_t i = 0; i < result.size(); i++) {
					comp(result[i].l);
					comp(result[i].r);
				}
			}
#endif
#endif
			return result;
		}

		// Eventはリリース音も含めて全て処理されている状態か
		bool isSilence()const override {
			for (auto& ch : m_channels) {
				if (!ch.m_notes.empty()) return false;
			}
			return true;
		}

		MidiModuleT(std::shared_ptr<const Soundfont>& sp, uint32_t sampleRate)
			:m_renderer(sp, sampleRate)
			, m_sampleRate(sampleRate)
		{}

		MidiModuleT(MidiModuleT&&) = default;
		MidiModuleT(const MidiModuleT&) = delete;
		MidiModuleT& operator=(const MidiModuleT&) = delete;
		~MidiModuleT() {}
	};

	// pan用 振幅値(0.0～1.0)テーブル
	template <typename T> const std::array<std::pair<T, T>, 128> MidiModuleT<T>::panAmplitudeTable = [] {
		// General MIDI Level2  cc#10:パン
		// Left  Channel Gain[dB] = 20 * log(cos(π / 2 * max(0, cc#10 - 1) / 126))
		// Right Channel Gain[dB] = 20 * log(sin(π / 2 * max(0, cc#10 - 1) / 126))
		// + db→振幅値  std::pow(10, db / 20.0);
		std::array<std::pair<T, T>, 128> table;
		for (int n = 0; n < table.size(); n++) {
			constexpr T pi2 = static_cast<T>(3.14159265358979323846 / 2 / 126);
			const T m = pi2 * (std::max)(0, n - 1);
			table[n] = std::pair(
				math::pow10(std::log(std::cos(m))),
				math::pow10(std::log(std::sin(m))));
		}
		return table;
	}();

	using MidiModuleF = MidiModuleT<float>;
	using MidiModule = MidiModuleT<double>;

}
