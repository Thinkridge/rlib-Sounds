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

			uint16_t			rootKey;
			enumSampleMode		sampleModes;
			int16_t				coarseTune;
			int16_t				scaleTuning;
			int16_t				fineTune;
			T					initialAttenuationAmplitude;		// initialAttenuation を振幅値(0～1.0)にした値
			std::pair<T, T>		pan;								// pan の値から L,R の倍率の値

			size_t				delayVolEnv;			// エンベロープのディレイ(アタックが始まるまでのサンプル数)
			size_t				attackVolEnv;			// エンベロープのアタック時間(サンプル数)
			size_t				holdVolEnv;				// エンベロープのホールド時間(アタックが終わってからディケイが始まるまでのサンプル数）
			size_t				decayVolEnv;			// エンベロープのディケイ時間(サンプル数)
			T					sustainVolEnv;			// サステイン量 0.0～1.0
			size_t				releaseVolEnv;			// エンベロープのリリース時間(サンプル数)
			T					divAttack;				// 計算値 1.0 / attackVolEnv
			T					divSustainDecay;		// 計算値 (1.0 - sustainVolEnv) / decayVolEnv
			T					divRelease;				// 計算値 1.0 / releaseVolEnvv
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
			InterInfo i{ sample };

			const auto getAmount = [&](GenOperator ope) {
				return Soundfont::getGenAmount<T>(ope, *instrumentSample.genInstLocal, *instrument.genInstGlobal, *instrument.genPresetLocal, *preset.genPresetGlobal);
			};

			i.rootKey = [&] {
				auto r = getAmount(GenOperator::overridingRootKey);
				return std::holds_alternative<int16_t>(r) ? std::get<int16_t>(r) : instrumentSample.spSample->originalKey;
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

			auto delayVolEnv = std::get<T>(getAmount(GenOperator::delayVolEnv));
			i.delayVolEnv = static_cast<size_t>(m_sampleRate * delayVolEnv);
			auto attackVolEnv = std::get<T>(getAmount(GenOperator::attackVolEnv));
			i.attackVolEnv = static_cast<size_t>(m_sampleRate * attackVolEnv);
			auto holdVolEnv = std::get<T>(getAmount(GenOperator::holdVolEnv));
			i.holdVolEnv = static_cast<size_t>(m_sampleRate * holdVolEnv);
			auto decayVolEnv = std::get<T>(getAmount(GenOperator::decayVolEnv));
			i.decayVolEnv = static_cast<size_t>(m_sampleRate * decayVolEnv);
			i.sustainVolEnv = [&] {
				const auto sustainVolEnv = std::get<T>(getAmount(GenOperator::sustainVolEnv));
				return math::decibelsToAmplitude(-sustainVolEnv);			// dB値から振幅値(0～1.0)へ
			}();
			auto releaseVolEnv = std::get<T>(getAmount(GenOperator::releaseVolEnv));
			i.releaseVolEnv = std::max<size_t>(static_cast<size_t>(m_sampleRate * releaseVolEnv), 1);		// 除算チェックを端折るために1以上を担保

			i.divAttack = static_cast<T>(1.0) / i.attackVolEnv;
			i.divSustainDecay = (static_cast<T>(1.0) - i.sustainVolEnv) / i.decayVolEnv;
			i.divRelease = static_cast<T>(1.0) / i.releaseVolEnv;

			const auto it = m_interInfos.mapInterInfo.emplace(refer, std::move(i));
			return it.first->second;
		}

	public:

		// volume用 振幅値(0.0～1.0)テーブル
		static const std::array<T, 128> volumeAmplitudeTable;

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

			// 音量(0.0～1.0)取得 size未満の配列なら終了の意味
			auto getAmplitudes(const InterInfo &inter,size_t position, size_t size)const {
				struct Result {
					std::optional<T>	same;		// 全体に一律に掛ける値(0.0～1.0) ReleaseRate のケース
					std::vector<T>		env;		// エンベロープ値(0.0～1.0) 配列がsize未満なら終了の意味
				};

				if (m_keyoff) {		// ReleaseRate なら
					assert(position >= m_keyoff->position);
					const size_t remain = m_keyoff->position + inter.releaseVolEnv - position;	// 終了(無音)までの残りサンプル数

					Result result = {
						m_keyoff->amplitude,								// 全体に一律に掛ける値
						decltype(Result::env)((std::min)(size, remain)),	// 今回出力するサンプル数
					};

					const auto amp = inter.divRelease;	// static_cast<T>(1.0) / m_releaseVolEnv;		// 1サンプルあたりのamp値
					for (size_t i = 0; i < result.env.size(); i++) {
						const auto linear = amp * (remain - i);	// キーオフから終了(無音)までの位置を 1.0～0.0 で表した値
#if 0
						// 0～1 の入力値を曲線で返す exponent:調整値 1.0=線形 1未満:立ち上がりが速い 1以上:遅い
						static const auto curve = [](double n, double exponent) {
							return std::pow(n, exponent);
						};
#else
						// べき指数(y)が自然数の場合の高速版のpow = std::pow(x, y);
						static const auto curve = [](T x, unsigned int y) {
							T result = 1.0;
							for (; y > 0; y /= 2) {
								if (y % 2 == 1) result *= x;
								x *= x;
							}
							return result;
						};
#endif
						result.env[i] = curve(linear, 8);				// 8:さじ加減
					}
					return result;
				}

				Result result = {
					std::nullopt,					// 全体に一律に掛ける値
					decltype(Result::env)(size),	// 今回出力するサンプル数
				};
				auto& env = result.env;
				size_t i = 0;

				// エンベロープのディレイ(アタックが始まるまで)
				if (position + i < inter.delayVolEnv) {
					size_t diff = inter.delayVolEnv - (position + i);
					i += diff;
					if (i >= env.size()) {
						return result;
					}
				}

				// AttackRate
				const size_t beginAttack = inter.delayVolEnv;						// AttackRate開始位置
				if (position + i < beginAttack + inter.attackVolEnv) {				// AttackRate
					const auto amp = inter.divAttack;								// 1サンプルあたりのamp値 = 1.0 / m_attackVolEnv
					size_t pos = (position + i) - beginAttack;					// AttackRate開始からの位置
					const size_t max = (std::min)(env.size(), i + (inter.attackVolEnv - pos));
					for (; i < max; i++, pos++) {
						env[i] = amp * (pos + 1);
					}
					if (i >= env.size()) {
						return result;
					}
				}

				// Hold(アタックが終わってからディケイが始まるまで)
				const size_t beginHold = beginAttack + inter.attackVolEnv;			// Hold開始位置
				if (position + i < beginHold + inter.holdVolEnv) {					// エンベロープのホールド時間(アタックが終わってからディケイが始まるまで)
					size_t pos = (position + i) - beginHold;					// Hold開始からの位置
					const size_t max = (std::min)(env.size(), i + (inter.holdVolEnv - pos));
					for (; i < max; i++, pos++) {
						env[i] = 1.0;
					}
					if (i >= env.size()) {
						return result;
					}
				}

				// DecayRate
				const size_t beginDecay = beginHold + inter.holdVolEnv;				// DecayRate開始位置
				if (position + i < beginDecay + inter.decayVolEnv) {
					const auto amp = inter.divSustainDecay;							// 1サンプルあたりのamp値 = (1.0 - m_sustainVolEnv) / m_decayVolEnv;
					size_t pos = (position + i) - beginDecay;					// DecayRate開始からの位置
					const size_t max = (std::min)(env.size(), i + (inter.decayVolEnv - pos));
					for (; i < max; i++, pos++) {
						const size_t remain = inter.decayVolEnv - pos;				// ディケイ完了までの時間(サンプル数)
						env[i] = inter.sustainVolEnv + (amp * remain);
					}
					if (i >= env.size()) {
						return result;
					}
				}

				// sustainVolEnv
				for (; i < env.size(); i++) {
					env[i] = inter.sustainVolEnv;
				}

				return result;
			}

			void keyoff(const Note& note) {
				auto& inter = ensureInter(note);

				if (m_keyoff) return;		// 既にkeyoff済みなら無視する(正常系でもあり得る)
				const auto amplitudes = getAmplitudes(inter.interInfo, m_renderedSize, 1);
				Keyoff k;
				k.position = m_renderedSize;
				k.amplitude = amplitudes.env[0];
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
					a *= volumeAmplitudeTable[note.m_presetKey.velocity];	// ベロシティ (ベロシティには推奨式が定義されてないがvolumeの推奨式と同等とする)
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

				// Enverope
				const auto envelope = getAmplitudes(interInfo, m_renderedSize, size);
				// const auto envelope = m_envelope.getAmplitudes(m_renderedSize, size);

				if (envelope.same) {
					result.amplitude.l *= *envelope.same;
					result.amplitude.r *= *envelope.same;
				}
				const std::vector<T>& env = envelope.env;

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

	// volume用 振幅値(0.0～1.0)テーブル
	template <typename T> const std::array<T, 128> RendererT<T>::volumeAmplitudeTable = [] {
		std::array<T, 128> table = {};
		for (int n = 0; n < table.size(); n++) {
			T m = n * (static_cast<T>(1) / 127);					// 0.0～1.0
			T db = static_cast<T>(40.0) * std::log10(m);			// dBを算出 GM2仕様：gain[dB] = 40 * log10(cc7/127)
			table[n] = math::pow10(db * (static_cast<T>(1) / 20));	// 振幅値(0.0～1.0)を算出 std::pow(10, db / 20.0);
		}
		return table;
	}();

	using RendererF = RendererT<float>;
	using Renderer = RendererT<double>;
}


