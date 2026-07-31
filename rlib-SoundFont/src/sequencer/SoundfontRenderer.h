#pragma once

#include "Soundfont.h"
#include "MidiModule.h"

namespace rlib::soundfont {
	
	template <typename T = double> class RendererT {
	public:
		class Note;
	private:

		// 1サンプルあたり進む値を算出
		static double getAdvance(double advanceBase, double pitch, uint32_t sampleRate, uint32_t targetSampleRate) {
			const auto n = advanceBase + pitch;								// pitch考慮
			constexpr double recip12 = 1.0 / 12.0;
			const double mul = std::exp2(n * recip12);	// std::pow(2.0, n * recip12);	// 1サンプルあたり進む値
			const auto advance = mul * sampleRate / targetSampleRate;		// 波形データのサンプルレート考慮
			return advance;
		}

		// 事前処理済の中間情報
		struct InterInfo {
			std::reference_wrapper<const std::vector<T>> sample;			// 浮動小数点数に変換後の波形データ
			midi::Envelope<T>	envelope;

			uint16_t			rootKey;
			enumSampleMode		sampleModes;
			int16_t				coarseTune;
			int16_t				scaleTuning;
			int16_t				fineTune;
			T					initialAttenuationAmplitude;		// initialAttenuation を振幅値(0～1.0)にした値
			std::pair<T, T>		pan;								// pan の値から L,R の倍率の値
		};

		const InterInfo& getInterInfo(const typename Soundfont::InstrumentRefer& refer) {
			std::lock_guard<std::recursive_mutex> lock(m_interInfos.mutex);
			if (const auto it = m_interInfos.mapInterInfo.find(refer); it != m_interInfos.mapInterInfo.end()) {
				return it->second;
			}

			// 中間情報生成
			const typename Soundfont::Preset& preset = refer.preset;
			const typename Soundfont::InstrumentSample& instrumentSample = refer.instrumentSample;
			const typename Soundfont::Instrument& instrument = refer.instrument;

			auto& sample = m_interInfos.mapSample[&*instrumentSample.spSample];
			if (sample.empty()) {	// 浮動小数点数に変換後の波形データ
				sample = std::move(instrumentSample.spSample->createSample<T>(*m_soundfont));
			}

			const auto getAmount = [&](GenOperator ope) {
				return Soundfont::getGenAmount<T>(ope, *instrumentSample.genInstLocal, *instrument.genInstGlobal, *instrument.genPresetLocal, *preset.genPresetGlobal);
			};

			typename midi::Envelope<T>::Params params;
			params.delayVolEnv = static_cast<size_t>(m_sampleRate * std::get<T>(getAmount(GenOperator::delayVolEnv)));		// エンベロープのディレイ(アタックが始まるまでのサンプル数)
			params.attackVolEnv = static_cast<size_t>(m_sampleRate * std::get<T>(getAmount(GenOperator::attackVolEnv)));	// エンベロープのアタック時間(サンプル数)
			params.holdVolEnv = static_cast<size_t>(m_sampleRate * std::get<T>(getAmount(GenOperator::holdVolEnv)));		// エンベロープのホールド時間(アタックが終わってからディケイが始まるまでのサンプル数）
			params.decayVolEnv = static_cast<size_t>(m_sampleRate * std::get<T>(getAmount(GenOperator::decayVolEnv)));		// エンベロープのディケイ時間(サンプル数)
			params.sustainVolEnv = [&] {																					// サステイン量 0.0～1.0
				const auto sustainVolEnv = std::get<T>(getAmount(GenOperator::sustainVolEnv));
				return math::decibelsToAmplitude(-sustainVolEnv);			// dB値から振幅値(0～1.0)へ
			}();
			params.releaseVolEnv = static_cast<size_t>(m_sampleRate * std::get<T>(getAmount(GenOperator::releaseVolEnv)));	// エンベロープのリリース時間(サンプル数)
			InterInfo i{ sample, midi::Envelope<T>(params) };

			i.rootKey = [&] {
				auto r = getAmount(GenOperator::overridingRootKey);
				auto p = std::get_if<int16_t>(&r);
				return p ? *p : instrumentSample.spSample->originalKey;
			}();

			i.sampleModes = std::get<enumSampleMode>(getAmount(GenOperator::sampleModes));

			i.coarseTune = std::get<int16_t>(getAmount(GenOperator::coarseTune));
			i.scaleTuning = std::get<int16_t>(getAmount(GenOperator::scaleTuning));
			i.fineTune = std::get<int16_t>(getAmount(GenOperator::fineTune));

			i.initialAttenuationAmplitude = [&] {
				const auto initialAttenuation = std::get<T>(getAmount(GenOperator::initialAttenuation));
				return math::decibelsToAmplitude(-initialAttenuation);		// dB値から振幅値(0～1.0)へ
			}();
			i.pan = [&] {
				const T n = std::get<T>(getAmount(GenOperator::pan));		// -50(L) ～ 50(R)
				constexpr std::pair<T, T> minmax{ static_cast<T>(-50.0),static_cast<T>(50.0) };
				const T m = (std::min)(minmax.second, (std::max)(minmax.first, n));
				const T normalized = (m - minmax.first) / (minmax.second - minmax.first);	// 0.0～1.0の範囲に変換
				constexpr T pi2 = static_cast<T>(3.14159265358979323846 / 2.0);
				const T l = std::sin((static_cast<T>(1.0) - normalized) * pi2);		// left 0.0～1.0
				const T r = std::sin(normalized * pi2);								// right 0.0～1.0
				return std::pair(l, r);
			}();

			const auto it = m_interInfos.mapInterInfo.emplace(refer, std::move(i));
			return it.first->second;
		}

	private:

		class Instrument {
			double				m_currentPosition = 0.0;	// 現在位置(サンプルデータ)
			size_t				m_renderedSize = 0;			// レンダリング済の出力サンプル数

			struct Keyoff {
				size_t	position;		// キーオフされた位置
				T		amplitude;		// キーオフされたときの音量(0.0～1.0)
			};
			std::optional<Keyoff>	m_keyoff;		// キーオフ

		public:
			const typename Soundfont::InstrumentRefer	m_instrumentRefer;
			struct Inter {
				std::reference_wrapper<const InterInfo>	interInfo;
				double									advanceBase;	// 1サンプルあたりに、サンプルデータを読み進める土台の値
				double									advanceNormal;	// 1サンプルあたりに、サンプルデータを読み進める値(pitchが0の場合)
			};
			std::optional<Inter> m_inter;

			Instrument(const typename Soundfont::InstrumentRefer& instrumentRefer)
				:m_instrumentRefer(instrumentRefer)
			{
			}
			Instrument(const Instrument&) = delete;
			Instrument& operator=(const Instrument&) = delete;

			const Inter& ensureInter(const Note& note) {
				if (!m_inter){
					auto& renderer = note.m_renderer;
					auto& i = note.m_renderer.getInterInfo(m_instrumentRefer);

					const typename Soundfont::InstrumentSample& instrumentSample = m_instrumentRefer.instrumentSample;

					// advanceBase advanceNormal
					auto n = static_cast<double>(note.m_presetKey.note - (i.rootKey - i.coarseTune));	// オリジナルキーとの差(半音=1)
					if (instrumentSample.spSample->pitchCorrection != 0) n += instrumentSample.spSample->pitchCorrection * 0.01;	// pitchCorrection/100
					if (i.scaleTuning != 100) n *= i.scaleTuning * 0.01;	// scaleTuning/100
					if (i.fineTune != 0) n += i.fineTune * 0.01;			// fineTune/100
					double advanceBase = n;
					double advanceNormal = getAdvance(advanceBase, 0.0, instrumentSample.spSample->sampleRate, renderer.m_sampleRate);

					m_inter = Inter{ i, advanceBase, advanceNormal };
				}
				return *m_inter;
			}

			void keyoff(const Note& note) {
				auto& inter = ensureInter(note);
				if (m_keyoff) return;		// 既にkeyoff済みなら無視する(正常系でもあり得る)
				const auto gains = inter.interInfo.get().envelope.getGains(m_renderedSize, 1);	// 現在のエンベロープ値
				Keyoff k;
				k.position = m_renderedSize;
				k.amplitude = gains[0];
				m_keyoff = k;
			}

			auto render(const Note& note, size_t size, double pitch = 0.0) {
				auto& inter = ensureInter(note);

				struct Result {	// 戻り値
					std::vector<T> samples;	// 配列数が引数size未満の場合は出力完了の意味
					struct {
						T l, r;
					}amplitude;
				}result;
				result.samples.resize(size);
				const typename Soundfont::SampleBody& sampleBody = *(m_instrumentRefer.instrumentSample.get().spSample);
				const InterInfo& interInfo = inter.interInfo;

				{// 振幅値(sampleに掛ける値)
					T a = interInfo.initialAttenuationAmplitude;	// generator.initialAttenuation 反映
					a *= midi::volumeGainTable<T>[note.m_presetKey.velocity];	// ベロシティ (ベロシティには推奨式が定義されてないがvolumeの推奨式と同等とする)
					result.amplitude.l = a * interInfo.pan.first;
					result.amplitude.r = a * interInfo.pan.second;
				}

				const double multiply = [&] {				// 乗値(=1サンプルあたり進む値)
					if (pitch == 0.0) {
						return inter.advanceNormal;
					} else {
						return getAdvance(inter.advanceBase, pitch, sampleBody.sampleRate, note.m_renderer.m_sampleRate);
					}
				}();

				// エンベロープ値(0.0～1.0) 配列がsize未満なら終了の意味
				const std::vector<T> env = m_keyoff ?
					interInfo.envelope.getGainsReleaseRate(m_renderedSize - m_keyoff->position, size) :
					interInfo.envelope.getGains(m_renderedSize, size);
				if (m_keyoff) {
					result.amplitude.l *= m_keyoff->amplitude;
					result.amplitude.r *= m_keyoff->amplitude;
				}

#if 0
				{// Enverope debug log
					static std::recursive_mutex mutex;
					std::lock_guard<std::recursive_mutex> lock(mutex);
					static std::map<std::string, std::ofstream> map;
					const std::string name = m_instrument.instrumentName + "_" + m_instrument.spSample->name;
					auto& os = map[name];
					if (!os.is_open()) os = std::ofstream("c:\\tmp\\env" + name + ".txt");
					for (size_t i = 0; i < env.size(); i++) os << i << "," << env[i] << "\n";
				}
#endif

				const auto isLoop = [&] {
					if (interInfo.sampleModes == enumSampleMode::loop || interInfo.sampleModes == enumSampleMode::keyloop) {	// ループあり
						if (sampleBody.loop.second - sampleBody.loop.first >= 32) {		// ループ範囲が32サンプル以上のみ有効
							return true;
						}
					}
					return false;
				}();

				auto &smpl = interInfo.sample.get();
				size_t i = 0;
				for (; i < env.size(); i++) {

					const auto posf = m_currentPosition;
					size_t pos = static_cast<size_t>(posf);
					if (!isLoop && pos >= smpl.size()) break;		// 最後までいったら抜ける

					T sample = [&] {
						const T decimal = static_cast<T>(posf - pos);	// 小数部
						T a, b;
						if (isLoop) {
							if (pos > sampleBody.loop.second) {
								pos = sampleBody.loop.first + (pos - sampleBody.loop.first) % (sampleBody.loop.second - sampleBody.loop.first);
							}
							a = smpl[pos];
							b = smpl[pos == sampleBody.loop.second ? sampleBody.loop.first : pos + 1];
						} else {						// ループなし
							a = smpl[pos];					// 範囲チェックは不要(上でやってる)
							b = pos + 1 < smpl.size() ? smpl[pos + 1] : 0;
						}
						return a + ((b - a) * decimal);
					}();

					sample *= env[i];		// エンベロープ

					result.samples[i] = sample;

					m_currentPosition += multiply;
				}

				if (i < size) {		// size未満で抜けてきたら完了
					result.samples.resize(i);
				}

				m_renderedSize += result.samples.size();
				return result;
			}

		};

	private:
		struct LessInstrumentRefer {
			template<typename U> bool operator()(const U& a, const U& b)const { return &a.instrumentSample.get() < &b.instrumentSample.get(); }
		};
		struct {
			std::map<const typename Soundfont::SampleBody*, std::vector<T>>	mapSample;	// 浮動小数点数波形データ実体
			std::map<typename Soundfont::InstrumentRefer, InterInfo, LessInstrumentRefer>		mapInterInfo;
			std::recursive_mutex												mutex;
		}m_interInfos;
	public:
		const std::shared_ptr<const Soundfont> m_soundfont;
		const uint32_t	m_sampleRate;

		RendererT(std::shared_ptr<const Soundfont>& sp, uint32_t sampleRate)
			:m_soundfont(sp)
			, m_sampleRate(sampleRate)
		{}
		RendererT(const RendererT&) = delete;
		RendererT& operator=(const RendererT&) = delete;

		class Note {
			friend class RendererT;
		public:
			RendererT& m_renderer;
			const typename Soundfont::PresetKey	m_presetKey;
		private:
			std::list<Instrument>	m_instruments;
		private:
			Note(RendererT& renderer, const typename Soundfont::PresetKey& presetKey, std::list<Instrument>&& instruments)
				: m_renderer(renderer)
				, m_presetKey(presetKey)
				, m_instruments(std::move(instruments))
			{
			}
		public:
			Note(Note&&) = default;
			Note(const Note&) = delete;
			Note& operator=(const Note&) = delete;

			// レンダリング(波形データ出力（結果配列がsize未満なら完了）
			std::vector<midi::StereoSample<T>> render(size_t size, double pitch = 0.0) {
				std::vector<midi::StereoSample<T>> result(size);

				size_t resultSize = 0;
				for (auto it = m_instruments.begin(); it != m_instruments.end();) {
					const auto rendered = it->render(*this, size, pitch);
					if (resultSize == 0) {		// 最初なら代入処理(加算は不要)
						for (size_t i = 0; i < rendered.samples.size(); i++) {
							result[i].l = rendered.samples[i] * rendered.amplitude.l;
							result[i].r = rendered.samples[i] * rendered.amplitude.r;
						}
					} else {
						for (size_t i = 0; i < rendered.samples.size(); i++) {
							result[i].l += rendered.samples[i] * rendered.amplitude.l;
							result[i].r += rendered.samples[i] * rendered.amplitude.r;
						}
					}
					if (rendered.samples.size() < size) {	// 完了なら
						it = m_instruments.erase(it);		// 破棄
					} else {
						it++;
					}
					resultSize = (std::max)(resultSize, rendered.samples.size());
				}

				result.resize(resultSize);
				return result;
			}

			void setKeyoff() {
				for (auto& inst : m_instruments) {
					inst.keyoff(*this);
				}
			}

			bool isFinished()const {
				return m_instruments.empty();
			}
		};

		Note createNote(const typename Soundfont::PresetKey& presetKey) {
			std::list<Instrument> instruments;
			auto preset = m_soundfont->getPreset(presetKey);
			for (auto& inst : preset) {
				instruments.emplace_back(inst);
			}
			return Note(*this, presetKey, std::move(instruments));
		}
	};

	using RendererF = RendererT<float>;
	using Renderer = RendererT<double>;
}


