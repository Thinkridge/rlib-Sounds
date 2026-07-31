#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "../ymfm/ymfm_opn.h"
#include "./FmRenderer.h"

namespace rlib::fm::psg {

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
			ChipWrapper2203	m_chip;

			size_t	m_position = 0;			// 位置(レンダリング済の出力サンプル数)
			struct Keyoff {
				size_t	position;			// キーオフされた位置
				T		amplitude;			// キーオフされたときの音量(0.0～1.0)
			};
			std::optional<Keyoff>	m_keyoff;		// キーオフ

			const T		m_amplitude;			// PSG出力値からT型へ変換する係数(velocity込み)
			uintmax_t	m_clockCount = 0;		// 実施済クロック数
			size_t		m_silenceCount = 0;
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

			std::vector<int32_t> renderPsg(size_t size) {
				std::vector<int32_t> result(size);
				auto& chip = m_chip.m_chip;
				const auto sr = chip.sample_rate(ChipWrapper2203::masterClock);	// 1秒あたりのクロック数		3,993,600/4 = 998,400
				const T n = static_cast<T>(m_renderer.m_sampleRate) / sr;		// 1クロックあたりのサンプル数	44,100/998,400 = 0.04417
				uintmax_t before = static_cast<uintmax_t>(m_clockCount * n);	// 読み出し済の位置(サンプルあたり)
				for (size_t outCount = 0; outCount < size; ) {
					typename decltype(m_chip)::ChipType::output_data output;
					chip.generate(&output, 1);
					const uintmax_t current = static_cast<uintmax_t>((++m_clockCount) * n);	// 読み出し済の位置(サンプルあたり)
					if (before != current) {										// 出力タイミング？
						const auto sample = output.data[1];	// PSG
						result[outCount++] = sample - PsgRangeMax / 2;
						before = current;
					}
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

				T				same;		// 全体に一律に掛ける値(0.0～1.0)ス
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

			// レンダリング(波形データ出力（結果配列がsize未満なら完了）
			//std::vector<T> render(size_t size) {
			//	std::vector<T> result(size);
			//	for (size_t outCount = 0; outCount < size; outCount++) {
			//		ymfm::ym2203_ssg::output_data output;
			//		m_chip.m_chip.generate_resampled_one(&output, masterClock, m_renderer.m_sampleRate);
			//		const int32_t out = output.data[0];
			//		if (out == 0) {
			//			if (m_keyoff && ++m_silenceCount > 16) {	// 発音完了？(トーンのみのためkeyoff直後にすぐ無音化する)
			//				result.resize(outCount);
			//				return result;
			//			}
			//		} else {
			//			m_silenceCount = 0;
			//			result[outCount] = out * m_amplitude;		// -1.0～1.0 へ変換(velocity込み)
			//		}
			//	}
			//	return result;
			//}

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
