#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <vector>

#include "../ymfm/ymfm_opn.h"	// ymfm::opn_registers
#include "./MidiModule.h"		// midi::volumeGainTable

namespace rlib::fm {

	// ymfm:ym2203 をFMに特化させたクラス
	class ym2203fm
	{
	public:
		using fm_engine = ymfm::fm_engine_base<ymfm::opn_registers>;
		using output_data = fm_engine::output_data;

		explicit ym2203fm(ymfm::ymfm_interface& intf)
			: m_address(0)
			, m_fm(intf)
		{
			m_last_fm.clear();
		}
		ym2203fm(const ym2203fm&) = delete;
		ym2203fm& operator=(const ym2203fm&) = delete;

		// リセット
		void reset()
		{
			m_fm.reset();
		}

		void save_restore(ymfm::ymfm_saved_state& state)
		{
			state.save_restore(m_address);
			state.save_restore(m_last_fm.data);
			m_fm.save_restore(state);
		}

		// 読み込み（FM以外は割愛）
		uint8_t read_status()
		{
			uint8_t result = m_fm.status();
			if (m_fm.intf().ymfm_is_busy())
				result |= fm_engine::STATUS_BUSY;
			return result;
		}
		uint8_t read(uint32_t offset)
		{
			uint8_t result = 0xff;
			switch (offset & 1)
			{
			case 0: // status port
				result = read_status();
				break;

			case 1: // data port (only SSG)
				result = 0; // read_data(); PSGはナシ
				break;
			}
			return result;
		}

		// 書き込み （FM以外は割愛）
		void write_address(uint8_t data)
		{
			// just set the address
			m_address = data;
		}
		void write_data(uint8_t data)
		{
			// 10-FF: write to FM
			m_fm.write(m_address, data);

			// mark busy for a bit
			m_fm.intf().ymfm_set_busy_end(32 * m_fm.clock_prescale());
		}
		void write(uint32_t offset, uint8_t data)
		{
			switch (offset & 1)
			{
			case 0: // address port
				write_address(data);
				break;

			case 1: // data port
				write_data(data);
				break;
			}
		}

		// 指定サンプリングレート(target_rate)で1サンプルを直接生成する。
		//   clock       : マスタークロック(Hz。例: 3,993,600)
		//   target_rate : 欲しい出力サンプリングレート(Hz。例: 44,100)
		void generate_resampled_one(output_data* output, uint32_t clock, uint32_t target_rate)
		{
			constexpr uint32_t kOutputRateDivider = 4;
			const uint32_t src_rate = clock / kOutputRateDivider;

			m_resampled.resampleAcc += src_rate;
			const uint32_t skip = (std::max)(m_resampled.resampleAcc / target_rate, 1u);	// 今回進める内部クロック数
			m_resampled.resampleAcc %= target_rate;

			constexpr uint32_t kFmSamplesPerOutput = 18;	// 内部クロックの18回(ym2203::update_prescale(uint8_t prescale=6))に1回FMを進める
			const uint32_t phase = m_resampled.phase;		// 次のクロックの位置(0～17)
			uint32_t count = (phase + skip + kFmSamplesPerOutput - 1) / kFmSamplesPerOutput - (phase + kFmSamplesPerOutput - 1) / kFmSamplesPerOutput;
			m_resampled.phase = (phase + skip) % kFmSamplesPerOutput;
			while (count--) clock_fm_ch0();
			output->clear();
			output->data[0] = m_last_fm.data[0];
		}

	protected:
		// ch0 のみを対象に FM を1クロック分進める
		// mask を ch0 のみ にすることで、未使用の ch1/2 処理を端折る
		void clock_fm_ch0()
		{
			constexpr uint32_t kChannel0Mask = 1u << 0;	// ch0 のみを表すビットマスク
			m_fm.clock(kChannel0Mask);	// .clock(fm_engine::ALL_CHANNELS);

			// update the FM content; OPN is full 14-bit with no intermediate clipping
			m_fm.output(m_last_fm.clear(), 0, 32767, kChannel0Mask);

			// convert to 10.3 floating point value for the DAC and back
			m_last_fm.roundtrip_fp();
		}

	private:
		uint8_t					m_address;				// address register
		fm_engine::output_data	m_last_fm;				// last FM output
		fm_engine				m_fm;					// core FM engine
		struct {
			uint32_t			phase = 0;			// 次の内部クロックの位置(0～17)
			uint32_t			resampleAcc = 0;
		}m_resampled;
	};


	class ChipWrapper2203fm : public ymfm::ymfm_interface {
	public:
		static constexpr uint32_t masterClock = 3993600;		// マスタークロック (デフォルト分周期でのOPN適正値)
		ym2203fm m_chip;

		union Reg28H {
			struct {
				uint8_t	channel : 2;
				uint8_t	none : 2;
				uint8_t	slot : 4;		// op1～op4
			};
			uint8_t val = 0;
		};

		ChipWrapper2203fm() :
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

			// 範囲外になる音域(ノート0～11、108以上)は、f-number を 1/2(or2)倍しながら block を範囲内に収める
			int block = octave - 1;
			double fnum = fnumber;
			while (block < 0) { fnum *= 0.5; block++; }
			while (block > 7) { fnum *= 2.0; block--; }

			BlockFNumber bf{ 0 };
			bf.fnumber = std::min<decltype(bf.fnumber)>(static_cast<decltype(bf.fnumber)>(std::round(fnumber)), 0x7ff);	// f-number は11bit
			bf.block = static_cast<decltype(bf.block)>(block);

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
			ChipWrapper2203fm	m_chip;
			bool				m_keyoff = false;
			const T				m_amplitude;			// 16bitからT型へ変換する係数(velocity値から)
			size_t				m_silenceCount = 0;
		private:
			Note(RendererT& renderer, const PresetKey& presetKey, const ChipWrapper2203fm::FmProgramReg& program, double pitch)
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
			//std::vector<T> render(size_t size) {
			//	std::vector<T> result(size);
			//	auto& chip = m_chip.m_chip;
			//	const auto sr = chip.sample_rate(ChipWrapper2203::masterClock);		// 1秒あたりのクロック数		3,993,600/4 = 998,400
			//	const T n = static_cast<T>(m_renderer.m_sampleRate) / sr;		// 1クロックあたりのサンプル数	44,100/998,400 = 0.04417
			//	uintmax_t before = static_cast<uintmax_t>(m_clockCount * n);	// 読み出し済の位置(サンプルあたり)
			//	for (size_t outCount = 0; true;) {
			//		typename decltype(m_chip.m_chip)::output_data output;
			//		chip.generate(&output, 1);
			//		const uintmax_t current = static_cast<uintmax_t>((++m_clockCount) * n);	// 読み出し済の位置(サンプルあたり)
			//		if (before != current) {									// 出力タイミング？
			//			const int32_t out = output.data[0];				// FM
			//			if (out == 0) {
			//				if (m_keyoff && ++m_silenceCount > 16) {	// 発音完了？
			//					result.resize(outCount);
			//					break;
			//				}
			//			} else {
			//				m_silenceCount = 0;
			//				result[outCount] = out * m_amplitude;		// -1.0～1.0 へ変換(veloctiy込み)
			//			}
			//			if (++outCount >= size) break;
			//			before = current;
			//		}
			//	}
			//	return result;
			//}

			// レンダリング(波形データ出力（結果配列がsize未満なら完了）
			// generate_resampled_one() で、出力サンプルレートに沿ったサンプル値を返す
			std::vector<T> render(size_t size) {
				std::vector<T> result(size);
				auto& chip = m_chip.m_chip;
				for (size_t outCount = 0; outCount < size; outCount++) {
					typename decltype(m_chip.m_chip)::output_data output;
					chip.generate_resampled_one(&output, ChipWrapper2203fm::masterClock, m_renderer.m_sampleRate);
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
				}
				return result;
			}

			void setKeyoff() {
				m_keyoff = true;
				m_chip.fmNoteOff();
			}

		};

		std::shared_ptr<Note> createNote(const PresetKey& presetKey, const ChipWrapper2203fm::FmProgramReg& program, double pitch) {
			return std::shared_ptr<Note>(new Note(*this, presetKey, program, pitch));
		}

	};

	using RendererF = RendererT<float>;
	using Renderer = RendererT<double>;
}

