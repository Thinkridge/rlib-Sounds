#pragma once

#include <future>
#include <optional>
#include <set>
#include <typeindex>

#include "../json/Json.h"
#include "../ymfm/ymfm_opn.h"
#include "./MidiEvent.h"
#include "./MidiModule.h"
#include "./FmRenderer.h"

namespace rlib::fm {


	template <typename T = double> class MidiModuleT : public midi::MidiModuleBase<T> {
		using Bit14 = midi::utility::Bit14;

		struct Preset {
			std::string								name;
			typename ChipWrapper2203::FmProgramReg	reg;
		};

		std::map<uint16_t, std::map< uint8_t, typename MidiModuleT<T>::Preset>> m_presets = {
		{0,{// bank0
			{0, {"piano",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{28,  8,  0,  8,  3, 31, 2, 1, 2},
				{26,  3,  1,  6, 10,  0, 0, 2, 7},
				{27, 20,  0,  9,  2, 44, 0, 5, 2},
				{28,  7,  2,  6,  6,  0, 0, 1, 5}},
				//AL   FB
				4,  7,
			}}},
			{1, {"sine",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,  0,  0,  8,  0,  0, 0, 1, 0},
				{31,  0,  0,  8,  0,  0, 0, 1, 0},
				{31,  0,  0,  8,  0,  0, 0, 1, 0},
				{31,  0,  0,  8,  0,  0, 0, 1, 0}},
				//AL   FB
				7,  0,
			}}},
			{63, {"Bass Drum (o3c)",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,  27,  31,  15,  15,  18,   0,   1,   0},
				{31,  16,  31,  15,  11,   0,   0,   1,   3},
				{31,  16,  31,  15,  10,   0,   0,   1,   7},
				{31,  17,  31,  15,  10,   0,   0,   0,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				5,   6,//   0,   0,   0,   0,
			}}},
			{62, {"Snare Drum (o2d)",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   0,   0,  15,   0,   8,   0,   3,   0},
				{31,  16,  13,  15,  14,   1,   0,   0,   0},
				{31,  22,  31,  15,   6,   0,   0,   2,   0},
				{31,  11,  13,  15,   7,   0,   0,   1,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				4,   7,//   1,  30,-127,   0}
			}
			}},
			{61, {"HiHat Close (o5e)",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   0,   0,  15,   0,   0,   0,  15,   7},
				{31,  16,  16,  15,  11,   8,   0,   3,   1},
				{31,  25,   9,  15,   6,  10,   0,   1,   7},
				{31,  22,  15,  15,  13,   0,   2,   7,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				4,   5,//   0,   0,   0,   0}
			}}},
			{60, {"HiHat Open (o5e)",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   0,   0,  15,   0,   1,   0,  15,   7},
				{31,  13,  15,  15,   3,   8,   0,   3,   1},
				{31,  25,   6,  15,   3,   0,   0,   1,   7},
				{31,  18,  10,  15,   7,   0,   2,   7,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				4,   5,//,   0,   0,   0,   0}
			}}},
			{59, {"Cymbal + Snare (o2d)",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   0,   1,  15,   0,   4,   0,   9,   7},
				{31,  15,  14,  15,   4,   0,   0,   6,   1},
				{31,  22,  31,  15,   6,   0,   0,   0,   0},
				{31,  11,  13,  15,   7,   0,   0,   1,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				4,   6,//,   0,   0,   0,   0}
			}}},
			{58, {"Tom (o4c - o2a)",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   0,   0,   6,   0,   7,   0,  15,   0},
				{31,  16,  15,  15,   3,  12,   1,   2,   0},
				{31,  26,   9,   6,  12,   4,   0,   2,   7},
				{31,  14,   5,  15,   7,   0,   0,   1,   3}},
				// AL   FB  DLY  CLC  DPS  RIS
				4,   7,//,   1,  30,-127,   0}
			}}},
			{10, {"SlapBass",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   1,   0,   5,   4,  25,   0,   1,   0},
				{31,   2,   0,   5,   3,  20,   0,   5,   3},
				{31,  14,   0,   6,  11,  43,   0,   1,   3},
				{31,   5,   1,   8,   4,   0,   2,   1,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				2,   2,//,   0,   0,   0,   0}
			}}},
			{11, {"SyBass",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   9,   1,   2,  10,  28,   2,   6,   0},
				{31,  11,   0,   3,  11,  53,   3,   5,   0},
				{31,   7,   2,   3,   1,  23,   0,   0,   0},
				{31,   7,   4,   8,   5,   0,   0,   1,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				0,   4,//,   0,   0,   0,   0}
			}}},
			{12, {"Bell",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   1,   1,   6,   1,  20,   2,  10,   7},
				{31,  11,  11,   8,  11,   5,   0,   4,   7},
				{31,  11,   8,   7,  12,   7,   0,  12,   3},
				{31,  10,   8,  11,   6,   0,   0,   1,   3}},
				// AL   FB  DLY  CLC  DPS  RIS
				6,   2,//,   0,   0,   0,   0}
			}}},
			{13, {"Trumpet",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{18,   8,   0,   5,   1,  25,   0,   1,   0},
				{22,   8,   0,   7,   1,   0,   1,   1,   0},
				{23,  14,   0,   8,   1,   0,   0,   1,   0},
				{17,   3,   0,   9,   1,   6,   1,   2,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				5,   7,//,  55,   4,  12,  90}
			}}},
			{14, {"Horn",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{13,   9,   0,   9,   3,  34,   0,   1,   4},
				{31,  17,   0,  12,  12,  45,   1,   5,   4},
				{12,  11,   0,   8,   1,  50,   0,   1,   4},
				{14,  31,   0,  10,   0,   0,   0,   1,   4}},
				// AL   FB  DLY  CLC  DPS  RIS
				2,   7,//   0,   0,   0,   0}
			}}},
			{15, {"Flute",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{19,   4,   1,   2,   3,  46,   0,  10,   7},
				{21,   2,   3,   7,   2,   0,   0,   2,   7},
				{18,   4,   1,   2,   3,  35,   0,   2,   3},
				{21,   2,   3,   7,   2,   0,   0,   2,   3}},
				// AL   FB  DLY  CLC  DPS  RIS
				4,   7,//,   0,   0,   0,   0}
			}}},
			{16, {"Oboe",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{25,  11,   0,   1,   1,  36,   3,   1,   3},
				{28,  12,   0,   2,   2,  41,   3,   9,   3},
				{25,  10,   0,   2,   1,  46,   1,   2,   3},
				{15,  10,   0,   6,   1,   0,   1,   4,   3}},
				// AL   FB  DLY  CLC  DPS  RIS
				2,   7,//,  64,   4,  11, 100}
			}}},
			{17, {"Harmonica",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{20,   5,   0,   2,   1,  30,   2,   4,   6},
				{16,   2,   0,   2,   0,  26,   0,   2,   5},
				{20,   8,   0,   2,   0,  45,   3,  11,   4},
				{12,   8,   0,   6,   1,   0,   0,   2,   6}},
				// AL   FB  DLY  CLC  DPS  RIS
				0,   7,//,  50,   4,  10,   0}
			}}},
			{18, {"Strings",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{25,  10,   0,   4,   1,  29,   1,   1,   0},
				{25,  11,   0,   6,   5,  15,   1,   5,   7},
				{28,  13,   0,   5,   2,  48,   1,   1,   0},
				{15,   4,   0,   5,   0,   0,   1,   1,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
				2,   7,//,  55,   4,   7, 127 }
			}}},
			{19, {"A.Piano",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{27,   3,   1,   4,   8,  36,   0,   2,   7},
				{27,  10,   7,   9,   6,   6,   1,   4,   7},
				{27,   3,   1,   5,   9,  39,   3,   6,   3},
				{27,  10,   7,   9,   6,   0,   1,   2,   3}},
				// AL   FB  DLY  CLC  DPS  RIS
				4,   3,//,   0,   0,   0,   0 }
			}}},
			{20, {"E.Guiter",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   2,   1,   0,   1,   7,   0,   3,   0},
				{25,   1,   1,   8,   1,  43,   0,  15,   0},
				{31,   2,   1,   0,   3,  27,   0,   1,   0},
				{31,  14,   1,   9,   1,   0,   0,   3,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
					0,   5,//,  65,   4,   6,  50 }
			}}},
			{21, {"Bass",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   2,   0,   0,   0,  31,   2,   2,   2},
				{31,   2,   0,   7,   2,   0,   1,   2,   1},
				{31,   2,   0,   0,   0,  13,   1,   1,   6},
				{31,   2,   0,   7,   2,   0,   1,   2,   5}},
				// AL   FB  DLY  CLC  DPS  RIS
					4,   7,//,   0,   0,   0,   0 }
			}}},
			{22, {"Syth.Strings",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{21,   0,   1,   9,   0,  22,   0,   4,   7},
				{11,  10,   0,  11,   2,   5,   0,   4,   7},
				{21,   0,   1,   9,   0,  24,   0,   4,   3},
				{11,  10,   0,  11,   2,   0,   0,   4,   3}},
				// AL   FB  DLY  CLC  DPS  RIS
					4,   7,//,  48,   4,  10,   1 }
			}}},
			{23, {"OrchestraHit",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   7,   0,   2,   1,  25,   0,   2,   3},
				{31,  31,   0,   7,   0,   0,   0,   2,   3},
				{31,   5,   0,   3,   1,  18,   0,   1,   5},
				{31,  31,   0,   7,   0,   0,   0,   2,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
					4,   7,//,   0,   0,   0,   0  }
			}}},
			{24, {"E.Piano",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,  20,   0,   5,   2,  33,   2,   1,   3},
				{31,   6,   2,   5,   5,   0,   2,   1,   3},
				{31,  13,   1,   5,   2,  57,   0,  10,   7},
				{31,   8,   2,   5,   4,   0,   2,   1,   7}},
				// AL   FB  DLY  CLC  DPS  RIS
					4,   7,//,   0,   0,   0,   0  }
			}}},
			{25, {"Pizzcate",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{29,  20,   1,   8,   2,  35,   0,   2,   0},
				{28,  14,   2,   7,   9,   0,   0,   1,   3},
				{30,  16,   3,   7,   5,   7,   0,   4,   7},
				{28,  15,   2,   8,   2,   0,   0,   2,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
					5,   6,//,   0,   0,   0,   0 }
			}}},
			{26, {"Synth.Bass",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,  14,   0,   4,   2,  23,   3,   2,   2},
				{31,   5,   0,   8,   2,   0,   1,   2,   7},
				{31,   6,   0,   4,   2,  31,   0,   2,   0},
				{31,   7,   0,   8,   2,   0,   1,   2,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
					4,   6,//,   0,   0,   0,   0 }
			}}},
			{27, {"A.Piano + 5th",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   3,   1,   3,   4,  39,   0,   9,   7},
				{31,  10,   7,   6,   6,   0,   1,   3,   7},
				{31,   3,   1,   3,   5,  39,   3,   6,   3},
				{31,  10,   7,   6,   6,   0,   1,   2,   3}},
				// AL   FB  DLY  CLC  DPS  RIS
					4,   0,//,   0,   0,   0,   0 }
			}}},
			{28, {"SynthLead",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{31,   0,   0,   0,   0,  28,   0,   6,   3},
				{31,   9,   1,   2,   3,  38,   0,   7,   3},
				{19,   2,   1,   6,   1,  38,   0,   1,   7},
				{26,   0,   0,   9,   0,   0,   0,   2,   7}},
				// AL   FB  DLY  CLC  DPS  RIS
					0,   7,//  48,   4,  15,   1 }
			}}},
			{29, {"SynthLead2",
			{{ //AR  DR  SR  RR  SL  TL KS ML DT
				{20,   3,   1,   4,   2,  30,   0,   1,   2},
				{31,  12,   2,   4,   3,  25,   0,   9,   6},
				{28,   2,   3,   4,   2,  42,   0,   2,   0},
				{20,   6,   0,   8,   3,   0,   0,   2,   0}},
				// AL   FB  DLY  CLC  DPS  RIS
					2,   7,//,  48,   4,  15,   1 }
			}}},
		}},

		#if __has_include("fmbank98_.inc")
		#include "fmbank98_.inc"
		#else
				// fmbank98 is intentionally omitted in the public repository.
		#endif

		};

		RendererT<T>	m_renderer;

		struct Channel{
			const uint8_t	m_channel;
			uint16_t		m_bank = 0;
			uint8_t			m_programNo = 0;
			uint8_t			m_volume = 100;			// 0～127
			uint8_t			m_expression = 127;		// 0～127
			uint8_t			m_pan = 64;				// 0～127
			
			std::optional<std::pair<T, T>> m_gain;	// volume,expression,pan,masterVolume を掛け合わせたl,r振幅値 (0.0～1.0)

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
			const auto itBank = m_presets.find(channel.m_bank);
			if (itBank == m_presets.end()) return;
			const auto it = itBank->second.find(channel.m_programNo);
			if (it == itBank->second.end()) return;
			const auto& program = it->second;

			typename RendererT<T>::PresetKey key;
			key.note = ev.note + channel.m_coarseTune;
			key.velocity = ev.velocity;
			key.fineTune = channel.m_fineTune;

			const auto spNote = m_renderer.createNote(key, program.reg, channel.m_pitch.get().result);
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

			switch(ev.type){
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
					const auto& map = json["rlib-MML"]["opnfm"].get<Json::Map>();
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
						if (data.size() != sizeof(decltype(pg.reg))) continue;	// 38

						int count = 0;
						const auto val = [&] {
							return data[count++].get<int>();
						};
						for (int op = 0; op < std::size(pg.reg.ope); op++) {
							auto& ope = pg.reg.ope[op];
							ope.ar = val();
							ope.dr = val();
							ope.sr = val();
							ope.rr = val();
							ope.sl = val();
							ope.tl = val();
							ope.ks = val();
							ope.ml = val();
							ope.dt = val();
						}
						pg.reg.al = val();
						pg.reg.fb = val();

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

#if 0
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
					if (sample > threshold) {
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

#if 0
		// レンダリング(波形データ出力（結果配列がsize未満なら完了=無音）
		std::vector<Sample> readSamples(size_t size) {
			std::vector<Sample> result;
#if 0
			const int clock = 3993600;
			// const int clock = 4000000;
			uint64_t output_step = 0x100000000ull / m_sampleRate;

			auto& chip = m_chip.m_chip;
			const auto step = 0x100000000ull / chip.sample_rate(clock);

			for (size_t i = 0; i < size; i++) {

				decltype(m_chip)::ChipType::output_data output;
				for (size_t n = 0; n < output_step; n += step) {
					chip.generate(&output, 1);
				}

				int32_t out0 = output.data[0];	// FM
				int32_t out1 = output.data[1 % ymfm::ym2203r::OUTPUTS];	// SSG1
				int32_t out2 = output.data[2 % ymfm::ym2203r::OUTPUTS];	// SSG2
				int32_t out3 = output.data[3 % ymfm::ym2203r::OUTPUTS];	// SSG2
				auto out = static_cast<intmax_t>(out0) + out1 + out2 + out3;

				// s.l = s.r = static_cast<T>(n) / 32767;
				const T n = out * (static_cast<T>(1.0) / 32767);

				{// 音量を反映
					


				}

				Sample s;
				s.l = s.r = n;
				result.push_back(s);
			}
#endif
			return result;
		}
#endif

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
			m["FileInfo"]["ICOP"] = u8"FM sound engine use ymfm\n"
									u8"https://github.com/aaronsgiles/ymfm\n"
									u8"BSD 3 - Clause License\n"
									u8"Copyright(c) 2021, Aaron Giles\n"
									u8"All rights reserved\n"
									u8"\n"
									u8"Reference list of FM sound data\n"
									u8"\nPC-9800シリーズ FM音源スーパーサウンド (bank:98)\n"
									u8"";

			auto& v = m["Presets"].ensureArray();
			for (auto& bank : m_presets) {
				for (auto& pr : bank.second) {
					auto &t = v.emplace_back();
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
