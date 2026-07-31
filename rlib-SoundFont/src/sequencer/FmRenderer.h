#pragma once

#include "../ymfm/ymfm_opn.h"

namespace rlib::fm {

	class ChipWrapper2203 : public ymfm::ymfm_interface {
	public:
		using ChipType = ymfm::ym2203;
		static constexpr uint32_t masterClock = 3993600;		// マスタークロック (デフォルト分周期でのOPN適正値)

		union Reg28H {
			struct {
				uint8_t	channel : 2;
				uint8_t	none : 2;
				uint8_t	slot : 4;		// op1～op4
			};
			uint8_t val = 0;
		};

		ChipWrapper2203() :
			m_chip(*this)
		{
			// reset
			m_chip.reset();

			//	m_chip.set_fidelity(ymfm::OPN_FIDELITY_MIN);
			//	m_chip.write_address(0x2f);
		}

		void regWrite(uint8_t address, uint8_t val) {
			m_chip.write(0, address);	// address
			m_chip.write(1, val);	// data
		}

		void fmSetPitch(uint8_t note, double pitch = 0.0) {
			constexpr int a4block = 4;				// a4 の block値
			static const double a4fnumber = [&] {	// a4 の f-number値
				constexpr double a4feq = 440.0;							// a4は440hzとする
				constexpr double scale = 72.0;							// 周波数スケーリング定数
				const double scaleFactor = std::pow(2.0, 21 - a4block);	// スケールファクタ
				return (scale * a4feq * scaleFactor) / masterClock;		// F-Number
			}();

			const double fnote = note + pitch;
			const int octave = static_cast<int>(fnote) / 12;			// octave(block)
			const double local = fnote - (octave * 12);					// C(0.0) ～ B(11.0) ～ 12.0未満 
			const auto mag = std::exp2((local - 9) * (1.0 / 12));		// 倍率 ( 9 は CからAへの差 )
			const auto fnumber = a4fnumber * mag;

			//static const std::vector<uint16_t> freqTable{ 0x26a, 0x28f, 0x2b6, 0x2df, 0x30b, 0x339, 0x36a, 0x39e, 0x3d5, 0x410, 0x44e, 0x48f };
			//const uint16_t fnumber = freqTable[note % freqTable.size()];
			//const int octave = note / static_cast<int>(freqTable.size());

			union BlockFNumber {
				struct {
					uint16_t	fnumber : 11;
					uint16_t	block : 3;
					uint16_t	none : 2;
				};
				uint8_t val[2] = { 0 };
			};

			BlockFNumber bf{ 0 };
			bf.fnumber = static_cast<decltype(bf.fnumber)>(std::round(fnumber));
			bf.block = octave - 1;

			uint8_t channel = 0;	// チャンネルは0のみ使用
			const uint8_t addrL = 0xa0 + channel;
			const uint8_t addrH = 0xa4 + channel;
			regWrite(addrH, bf.val[1]);
			regWrite(addrL, bf.val[0]);

		}

		void fmNoteOn() {
			uint8_t channel = 0;	// チャンネルは0のみ使用
			Reg28H r;
			r.slot = 0xf;
			r.channel = channel;
			regWrite(0x28, r.val);
		}

		void fmNoteOff() {
			uint8_t channel = 0;	// チャンネルは0のみ使用
			Reg28H r;
			r.channel = channel;
			regWrite(0x28, r.val);
		}

		struct FmProgramReg {
			struct {
				uint8_t	ar, dr, sr, rr, sl, tl, ks, ml, dt;
			}ope[4];
			uint8_t	al, fb;
		};
		struct reg {
			union DtMl {
				struct {
					uint8_t	multiple : 4;
					uint8_t	detune : 3;
					uint8_t	none : 1;
				};
				uint8_t val = 0;
			};
			union Tl {
				struct {
					uint8_t	totalLevel : 7;
					uint8_t	none : 1;
				};
				uint8_t val = 0;
			};
			union KsAr {
				struct {
					uint8_t	attackRate : 5;
					uint8_t	none : 1;
					uint8_t	keyScale : 2;
				};
				uint8_t val = 0;
			};
			union Dr {
				struct {
					uint8_t	decayRate : 5;
					uint8_t	none : 3;
				};
				uint8_t val = 0;
			};
			union Sr {
				struct {
					uint8_t	sustainRate : 5;
					uint8_t	none : 3;
				};
				uint8_t val = 0;
			};
			union SlRr {
				struct {
					uint8_t	releaseRate : 4;
					uint8_t	sustainLevel : 4;
				};
				uint8_t val = 0;
			};
			union FbAl {
				struct {
					uint8_t	algorhythm : 3;
					uint8_t	feedBack : 3;
					uint8_t	none : 2;
				};
				uint8_t val = 0;
			};
		};

		void fmSetProgram(const FmProgramReg& program) {
			constexpr uint8_t channel = 0;	// チャンネルは0のみ使用
			for (size_t i = 0; i < std::size(program.ope); i++) {
				const auto reg = [&](auto addr, auto val) {
					regWrite(static_cast<uint8_t>(addr + (i * 4) + channel), val);
				};
				const auto& ope = program.ope[([i] {
					constexpr size_t a[] = { 0, 2, 1, 3 };
					return a[i];
				}())];

				reg::DtMl dtml;
				dtml.multiple = ope.ml;
				dtml.detune = ope.dt;
				reg(0x30, dtml.val);

				reg::Tl tl;
				tl.totalLevel = ope.tl;
				reg(0x40, tl.val);

				reg::KsAr ksar;
				ksar.keyScale = ope.ks;
				ksar.attackRate = ope.ar;
				reg(0x50, ksar.val);

				reg::Dr dr;
				dr.decayRate = ope.dr;
				reg(0x60, dr.val);

				reg::Sr sr;
				sr.sustainRate = ope.sr;
				reg(0x70, sr.val);

				reg::SlRr slrr;
				slrr.releaseRate = ope.rr;
				slrr.sustainLevel = ope.sl;
				reg(0x80, slrr.val);
			}

			reg::FbAl fbal;
			fbal.algorhythm = program.al;
			fbal.feedBack = program.fb;
			regWrite(static_cast<uint8_t>(0xb0 + channel), fbal.val);

		}


		// MIDIノート番号(小数点以下はセント単位のずれ)からトーン周期レジスタを算出して書き込む
		void psgSetPitch(uint8_t note, double pitch = 0.0) {
			constexpr uint8_t channel = 0;		// チャンネルは0(A)のみ使用
			
			constexpr double a4note = 69.0;		// A4のノート番号(MIDI標準)
			constexpr double a4freq = 440.0;	// A4は440Hzとする
			const double fnote = note + pitch;
			const double freq = a4freq * std::pow(2.0, (fnote - a4note) * (1.0 / 12.0));

			// freq = masterClock / (8 × 内蔵分周器の分周数(4) × period ) ⇔ period = masterClock / (32 × freq)
			const double periodF = masterClock / (32.0 * freq);
			uint32_t period = static_cast<uint32_t>(std::llround(periodF));
			period = std::clamp<uint32_t>(period, 1, 0xfff);	// 12bitレジスタ

			regWrite(channel * 2 + 0x00, static_cast<uint8_t>(period & 0xff));
			regWrite(channel * 2 + 0x01, static_cast<uint8_t>((period >> 8) & 0x0f));
		}

		void psgSetNoise(uint8_t noise) {
			regWrite(0x06, noise & 0x1f);	// ノイズ周波数(1～31。0は1と等価)
		}

		void psgSetLevel(int8_t level) {
			constexpr uint8_t channel = 0;		// チャンネルは0(A)のみ使用
			union Reg {
				struct {
					uint8_t	level : 4;		// level (bits0-3)
					uint8_t	m : 1;			// 0:固定振幅 1:可変振幅 (bit4)
					uint8_t	none : 3;
				};
				uint8_t val = 0;
			}r;
			r.level = level;
			regWrite(channel + 0x08, r.val);
		}

		void psgSetMixer(uint8_t noise, uint8_t	tone) {
			union Reg {
				struct {
					uint8_t	tone : 3;		// chA～C (0=enable,1=disable)
					uint8_t	noise : 3;		// chA～C (0=enable,1=disable)
					uint8_t	inout : 2;
				};
				uint8_t val = 0;
			}r;
			r.noise = noise;
			r.tone = tone;
			regWrite(0x07, r.val);
		}

	public:
		ChipType m_chip;
	};

	template <typename T = double> class RendererT {
	public:
		struct PresetKey {
			uint8_t		note = 0;
			uint8_t		velocity = 0;
			double		fineTune = 0.0;
		};

	public:
		const uint32_t	m_sampleRate;

		RendererT(uint32_t sampleRate)
			:m_sampleRate(sampleRate)
		{
		}
		RendererT(const RendererT&) = delete;
		RendererT& operator=(const RendererT&) = delete;

		class Note {
			friend class RendererT;
		public:
			RendererT& m_renderer;
			const PresetKey m_presetKey;
		private:
			ChipWrapper2203				m_chip;
			bool						m_keyoff = false;
			const T						m_amplitude;			// 16bitからT型へ変換する係数(velocity値から)
			uintmax_t					m_clockCount = 0;		// 実施済クロック数
			size_t						m_silenceCount = 0;
		private:
			Note(RendererT& renderer, const PresetKey& presetKey, const ChipWrapper2203::FmProgramReg& program, double pitch)
				: m_renderer(renderer)
				, m_presetKey(presetKey)
				, m_amplitude((static_cast<T>(1.0) / 32767)* midi::volumeGainTable<T>[presetKey.velocity])
			{
				m_chip.fmSetProgram(program);
				m_chip.fmSetPitch(presetKey.note, presetKey.fineTune + pitch);
				m_chip.fmNoteOn();
			}
		public:
			Note(Note&&) = default;
			Note(const Note&) = delete;
			Note& operator=(const Note&) = delete;

			void setPitchBend(double pitch) {
				m_chip.fmSetPitch(m_presetKey.note, m_presetKey.fineTune + pitch);
			}

			//// レンダリング（結果配列がsize未満なら完了）旧愚直コード
			std::vector<T> render(size_t size) {
				std::vector<T> result(size);
				auto& chip = m_chip.m_chip;
				const auto sr = chip.sample_rate(ChipWrapper2203::masterClock);					// 1秒あたりのクロック数		3,993,600/4 = 998,400
				const T n = static_cast<T>(m_renderer.m_sampleRate) / sr;		// 1クロックあたりのサンプル数	44,100/998,400 = 0.04417
				uintmax_t before = static_cast<uintmax_t>(m_clockCount * n);	// 読み出し済の位置(サンプルあたり)
				for (size_t outCount = 0; true;) {
					typename decltype(m_chip)::ChipType::output_data output;
					chip.generate(&output, 1);
					const uintmax_t current = static_cast<uintmax_t>((++m_clockCount) * n);	// 読み出し済の位置(サンプルあたり)
					if (before != current) {									// 出力タイミング？
						const int32_t out = output.data[0];				// FM
						if (out == 0) {
							if (m_keyoff && ++m_silenceCount > 16) {	// 発音完了？
								result.resize(outCount);
								break;
							}
						} else {
							m_silenceCount = 0;
							result[outCount] = out * m_amplitude;		// -1.0～1.0 へ変換(veloctiy込み)
						}
						if (++outCount >= size) break;
						before = current;
					}
				}
				return result;
			}

			// レンダリング(波形データ出力（結果配列がsize未満なら完了）
			//std::vector<T> render(size_t size) {
			//	std::vector<T> result(size);
			//	auto& chip = m_chip.m_chip;
			//	for (size_t outCount = 0; outCount < size; outCount++) {
			//		typename decltype(m_chip)::ChipType::output_data output;
			//		chip.generate_resampled_one(&output, masterClock, m_renderer.m_sampleRate);
			//		const int32_t out = output.data[0];				// FM
			//		if (out == 0) {
			//			if (m_keyoff && ++m_silenceCount > 16) {	// 発音完了？
			//				result.resize(outCount);
			//				break;
			//			}
			//		} else {
			//			m_silenceCount = 0;
			//			result[outCount] = out * m_amplitude;		// -1.0～1.0 へ変換(veloctiy込み)
			//		}
			//	}
			//	return result;
			//}

			void setKeyoff() {
				m_keyoff = true;
				m_chip.fmNoteOff();
			}

		};

		std::shared_ptr<Note> createNote(const PresetKey& presetKey, const ChipWrapper2203::FmProgramReg& program, double pitch) {
			return std::shared_ptr<Note>(new Note(*this, presetKey, program, pitch));
		}

	};




	using RendererF = RendererT<float>;
	using Renderer = RendererT<double>;
}


