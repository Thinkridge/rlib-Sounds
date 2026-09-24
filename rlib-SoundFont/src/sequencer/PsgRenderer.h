#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "../ymfm/ymfm_opn.h"
#include "./MidiModule.h"

namespace rlib::fm::psg {

	// ymfm:ym2203 をPSGに特化させたクラス
	class ym2203psg
	{
	public:
		using output_data = ymfm::ym2203::output_data;

		explicit ym2203psg(ymfm::ymfm_interface& intf)
			: m_address(0)
			, m_ssg(intf)
		{
			m_resampled.last.clear();
		}
		ym2203psg(const ym2203psg&) = delete;
		ym2203psg& operator=(const ym2203psg&) = delete;

		// リセット
		void reset()
		{
			m_ssg.reset();
		}

		// save/restore
		void save_restore(ymfm::ymfm_saved_state& state)
		{
			state.save_restore(m_address);
			m_ssg.save_restore(state);
			state.save_restore(m_resampled.last.data);
		}

		// 読み込み （PSG以外は割愛）
		uint8_t read_data()
		{
			uint8_t result = 0;
			if (m_address < 0x10)
			{
				// 00-0F: Read from SSG
				result = m_ssg.read(m_address & 0x0f);
			}
			return result;
		}
		uint8_t read(uint32_t offset)
		{
			uint8_t result = 0xff;
			switch (offset & 1)
			{
			case 0: // status port
				result = 0xff;	// read_status(); status port は非対応(0xffを返す)
				break;

			case 1: // data port (only SSG)
				result = read_data();
				break;
			}
			return result;
		}

		// 書き込み（PSG以外は割愛）
		void write_address(uint8_t data)
		{
			m_address = data;
		}
		void write_data(uint8_t data)
		{
			if (m_address < 0x10) {
				m_ssg.write(m_address & 0x0f, data);
			}
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

		void generate_resampled_one(output_data* output, uint32_t clock, uint32_t target_rate)
		{
			constexpr uint32_t kOutputRateDivider = 4;	// ymfm::ym2203::sample_rate()(OPN_FIDELITY_MAX)と同じ、clock→チップ内部レートの分周比
			const uint32_t src_rate = clock / kOutputRateDivider;

			m_resampled.resampleAcc += src_rate;
			const uint32_t skip = (std::max)(m_resampled.resampleAcc / target_rate, 1u);	// 今回進める内部クロック数
			m_resampled.resampleAcc %= target_rate;

			// 内部クロック4回に1回SSGを進める(ym2203::update_prescale(uint8_t prescale=6) の ssg_resampler::configure(4,1) = resample_n_1<4> と同じ)。
			// resampler を1クロックずつ呼ぶ代わりに、ssg_engine を直接クロックし、出力値は最後に1回だけ算出する
			constexpr uint32_t kSsgSamplesPerOutput = 4;
			const uint32_t phase = m_resampled.phase;	// 次のクロックの位置(0～3。0ならそのクロックでSSGを進める)
			uint32_t count = (phase + skip + kSsgSamplesPerOutput - 1) / kSsgSamplesPerOutput - (phase + kSsgSamplesPerOutput - 1) / kSsgSamplesPerOutput;
			m_resampled.phase = (phase + skip) % kSsgSamplesPerOutput;
			if (count != 0) {
				while (count--) m_ssg.clock();
				m_ssg.output(m_resampled.last);
			}
			output->data[1] = m_resampled.last.data[0];	// [1]～[3] が PSG ch0～2
			// output->data[2] = m_last.data[1];
			// output->data[3] = m_last.data[2];
		}

	private:
		uint8_t								m_address;			// address register
		ymfm::ssg_engine					m_ssg;				// SSG engine
		struct {
			ymfm::ssg_engine::output_data	last;				// last SSG output (ch A～C)  サンプリングレートが極端に高い場合に前回値を返す必要があるためメンバ変数
			uint32_t						phase = 0;			// 次の内部クロックの位置(0～3)
			uint32_t						resampleAcc = 0;
		}m_resampled;
	};


	class ChipWrapper2203psg : public ymfm::ymfm_interface {
	public:
		static constexpr uint32_t masterClock = 3993600;		// マスタークロック (ChipWrapper2203と同じ値)
		ym2203psg m_chip;

		ChipWrapper2203psg() :
			m_chip(*this)
		{
			m_chip.reset();
		}

		void regWrite(uint8_t address, uint8_t val) {
			m_chip.write(0, address);	// address
			m_chip.write(1, val);	// data
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
	};

	// PSG(SSG)音源のレンダラー。
	template <typename T = double> class RendererT {
	public:

		struct Envelope {
			T	attack;		// アタック時間(sec)
			T	hold;		// ホールド時間(アタックが終わってからディケイが始まるまでのsec）
			T	decay;		// ディケイ時間(sec)
			T	sustain;	// サステインレベル 0.0(無音)～1.0(最大)
			T	release;	// リリース時間(sec)
		};
		struct Mixer {
			uint8_t noise = 0;  // ノイズ周波数 0:OFF, 1～31:ON (レジスタ値の0は1と等価)
			bool tone = true;	// tone ON/OFF
		};

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

		struct Program {
			const midi::Envelope<T>	m_envelope;
			const Mixer				m_mixer;
		};

		class Note {
			friend class RendererT;
		public:
			RendererT& m_renderer;
			const PresetKey m_presetKey;
		private:
			ChipWrapper2203psg	m_chip;
			size_t				m_position = 0;			// 位置(レンダリング済の出力サンプル数)
			struct Keyoff {
				size_t	position;			// キーオフされた位置
				T		amplitude;			// キーオフされたときの音量(0.0～1.0)
			};
			std::optional<Keyoff>	m_keyoff;		// キーオフ

			const T		m_amplitude;			// PSG出力値からT型へ変換する係数(velocity込み)
			std::shared_ptr<Program> m_program;
		private:
			static constexpr int PsgRangeMax = 16382;	// PSG部の出力は 0～16382
			static constexpr T GainAdjustment = 0.17f;	// 出力を一律で抑制する。他デバイスと音量を合わせるためのさじ加減（根拠がある値ではない）
			Note(RendererT& renderer, const PresetKey& presetKey, std::shared_ptr<Program> program, double pitch)
				: m_renderer(renderer)
				, m_presetKey(presetKey)
				, m_amplitude(static_cast<T>(1.0) / (PsgRangeMax / 2) * GainAdjustment * midi::volumeGainTable<T>[presetKey.velocity] )
				, m_program(program)
			{
				m_chip.psgSetPitch(presetKey.note, presetKey.fineTune + pitch);
				if (program->m_mixer.noise != 0) {
					m_chip.psgSetNoise(program->m_mixer.noise);
				}
				m_chip.psgSetLevel(15);
				m_chip.psgSetMixer(program->m_mixer.noise != 0 ? 0b110 : 0b111, program->m_mixer.tone ? 0b110 : 0b111);	// ch0(A)のみ使用 0=enable,1=disable
			}

			//// レンダリング（結果配列がsize未満なら完了）旧愚直コード
			//std::vector<int32_t> renderPsg(size_t size) {
			//	std::vector<int32_t> result(size);
			//	auto& chip = m_chip.m_chip;
			//	const auto sr = chip.sample_rate(ChipWrapper2203::masterClock);	// 1秒あたりのクロック数		3,993,600/4 = 998,400
			//	const T n = static_cast<T>(m_renderer.m_sampleRate) / sr;		// 1クロックあたりのサンプル数	44,100/998,400 = 0.04417
			//	uintmax_t before = static_cast<uintmax_t>(m_clockCount * n);	// 読み出し済の位置(サンプルあたり)
			//	for (size_t outCount = 0; outCount < size; ) {
			//		typename decltype(m_chip)::ChipType::output_data output;
			//		chip.generate(&output, 1);
			//		const uintmax_t current = static_cast<uintmax_t>((++m_clockCount) * n);	// 読み出し済の位置(サンプルあたり)
			//		if (before != current) {										// 出力タイミング？
			//			const auto sample = output.data[1];	// PSG
			//			result[outCount++] = sample - PsgRangeMax / 2;
			//			before = current;
			//		}
			//	}
			//	m_position += size;
			//	return result;
			//}

			// レンダリング（結果配列がsize未満なら完了
			// generate_resampled_one() で、出力サンプルレートに沿ったサンプル値を返す
			std::vector<int32_t> renderPsg(size_t size) {
				std::vector<int32_t> result(size);
				auto& chip = m_chip.m_chip;
				for (size_t outCount = 0; outCount < size; outCount++) {
					typename decltype(m_chip.m_chip)::output_data output;
					chip.generate_resampled_one(&output, ChipWrapper2203psg::masterClock, m_renderer.m_sampleRate);
					const auto sample = output.data[1];	// PSG
					result[outCount] = sample - PsgRangeMax / 2;
				}
				m_position += size;
				return result;
			}

		public:
			Note(Note&&) = default;
			Note(const Note&) = delete;
			Note& operator=(const Note&) = delete;

			void setPitchBend(double pitch) {
				m_chip.psgSetPitch(m_presetKey.note, m_presetKey.fineTune + pitch);
			}

			//// レンダリング（結果配列がsize未満なら完了）旧愚直コード
			std::vector<T> render(size_t size) {
				T				same;		// 全体に一律に掛ける値(0.0～1.0)
				std::vector<T>	env;		// エンベロープ値(0.0～1.0) 結果兼
				if (m_keyoff) {
					env = m_program->m_envelope.getGainsReleaseRate(m_position - m_keyoff->position, size);
					same = m_amplitude * m_keyoff->amplitude;
				} else {
					env = m_program->m_envelope.getGains(m_position, size);
					same = m_amplitude;
				}
				const auto samples = renderPsg(env.size());
				for (size_t n = 0; n < samples.size(); n++) {
					env[n] *= samples[n] * same;
				}
				return env;
			}

			void setKeyoff() {
				if (m_keyoff) return;		// 既にkeyoff済みなら無視する
				const auto gains = m_program->m_envelope.getGains(m_position, 1);	// 現在のエンベロープ値
				Keyoff k;
				k.position = m_position;
				k.amplitude = gains[0];
				m_keyoff = k;
			}
		};

		std::shared_ptr<Program> createProgram(const Envelope& envelope, const Mixer& mixer) {
			typename midi::Envelope<T>::Params params;
			params.delayVolEnv = 0;															// ディレイ(アタックが始まるまでのサンプル数)
			params.attackVolEnv = static_cast<size_t>(m_sampleRate * envelope.attack);		// アタック時間(サンプル数)
			params.holdVolEnv = static_cast<size_t>(m_sampleRate * envelope.hold);			// ホールド時間(アタックが終わってからディケイが始まるまでのサンプル数）
			params.decayVolEnv = static_cast<size_t>(m_sampleRate * envelope.decay);		// ディケイ時間(サンプル数)
			params.sustainVolEnv = envelope.sustain;										// サステインレベル 
			params.releaseVolEnv = static_cast<size_t>(m_sampleRate * envelope.release);	// リリース時間(サンプル数)
			return std::shared_ptr<Program>(new Program({ midi::Envelope<T>(params), mixer }));
		}

		std::shared_ptr<Note> createNote(const PresetKey& presetKey, std::shared_ptr<Program> program, double pitch) {
			return std::shared_ptr<Note>(new Note(*this, presetKey, program, pitch));
		}

	};

	using RendererF = RendererT<float>;
	using Renderer = RendererT<double>;
}
