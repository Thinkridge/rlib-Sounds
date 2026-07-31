#pragma once

#include <future>
#include <typeindex>

#include "../sequencer/MidiEvent.h"

namespace rlib::midi {


	template <typename T = double> struct StereoSample {
		T l, r;
		bool operator==(const StereoSample& s) const { return l == s.l && r == s.r; }
		bool operator!=(const StereoSample& s) const { return !(*this == s); }
	};

	template <typename T = double> class MidiModuleBase {

	public:
		virtual ~MidiModuleBase() {};

		virtual uint32_t getSampleRate()const = 0;

		virtual void setMidiEvent(const midi::Event& ev) = 0;

		// レンダリング(波形データ出力（結果配列がsize未満なら完了=無音）
		virtual std::vector<StereoSample<T>> readSamples(size_t size) = 0;

		// Eventはリリース音も含めて全て処理されている状態か
		virtual bool isSilence()const = 0;
	};


	// volume用 振幅値(0.0～1.0)テーブル
	template <typename T> static const std::array<T, 128> volumeGainTable = [] {				// volume用 振幅値(0.0～1.0)テーブル
		std::array<T, 128> table = {};
		for (int n = 0; n < table.size(); n++) {
			T m = n * (static_cast<T>(1) / 127);	// 0.0～1.0
			table[n] = m * m;						// 振幅値(0.0～1.0)を算出 「GM2仕様：gain[dB] = 40 * log10(cc7/127)」 から cc7/127 の2乗
		}
		return table;
	}();

	// pan用 振幅値(0.0～1.0)テーブル
	template <typename T> static const std::array<std::pair<T, T>, 128> panGainTable = [] {
		// General MIDI Level2  cc#10:パン
		// Left  Channel Gain[dB] = 20 * log(cos(π / 2 * max(0, cc#10 - 1) / 126))
		// Right Channel Gain[dB] = 20 * log(sin(π / 2 * max(0, cc#10 - 1) / 126))
		// + db→振幅値  std::pow(10, db / 20.0);
		std::array<std::pair<T, T>, 128> table;
		for (int n = 0; n < table.size(); n++) {
			constexpr T pi2 = static_cast<T>(3.14159265358979323846 / 2 / 126);
			const T m = pi2 * (std::max)(0, n - 1);			// -1オフセット=中央値は64
			table[n] = std::pair(std::cos(m), std::sin(m));	// 振幅値なのでcos,sinのみ
		}
		return table;
	}();

	// エンベロープ
	template <typename T = double> class Envelope
	{
	public:
		struct Params {
			size_t	delayVolEnv;			// エンベロープのディレイ(アタックが始まるまでのサンプル数)
			size_t	attackVolEnv;			// エンベロープのアタック時間(サンプル数)
			size_t	holdVolEnv;				// エンベロープのホールド時間(アタックが終わってからディケイが始まるまでのサンプル数）
			size_t	decayVolEnv;			// エンベロープのディケイ時間(サンプル数)
			T		sustainVolEnv;			// サステイン量 0.0～1.0
			size_t	releaseVolEnv;			// エンベロープのリリース時間(サンプル数)
		};
		const Params m_params;
		const T m_divRelease;				// ReleaseRate 1サンプルあたりのamp値
		const T m_divAttack;				// AttackRate 1サンプルあたりのamp値
		const T m_divSustainDecay;			// DecayRate 1サンプルあたりのamp値
		Envelope(const Params& params)
			:m_params(params)
			, m_divRelease(static_cast<T>(1.0) / std::max<size_t>(params.releaseVolEnv, 1))
			, m_divAttack(static_cast<T>(1.0) / std::max<size_t>(params.attackVolEnv, 1))
			, m_divSustainDecay((static_cast<T>(1.0) - params.sustainVolEnv) / std::max<size_t>(params.decayVolEnv, 1))
		{
		}

		// エンベロープ係数(0.0～1.0)取得 (キーオフ前)
		std::vector<T> getGains(size_t position, size_t size)const {
			std::vector<T> result(size);	// 今回出力するサンプル数
			size_t i = 0;

			// delayVolEnv (アタックが始まるまで)
			if (position + i < m_params.delayVolEnv) {
				size_t diff = m_params.delayVolEnv - (position + i);
				i += diff;
				if (i >= result.size()) return result;
			}

			// AttackRate
			if (position + i < m_params.delayVolEnv + m_params.attackVolEnv) {
				size_t pos = (position + i) - m_params.delayVolEnv;					// AttackRate開始からの位置
				const size_t max = (std::min)(result.size(), i + (m_params.attackVolEnv - pos));
				for (; i < max; i++, pos++) result[i] = m_divAttack * (pos + 1);
				if (i >= result.size()) return result;
			}

			// Hold (アタックが終わってからディケイが始まるまで)
			const size_t beginHold = m_params.delayVolEnv + m_params.attackVolEnv;	// Hold開始位置
			if (position + i < beginHold + m_params.holdVolEnv) {					// エンベロープのホールド時間(アタックが終わってからディケイが始まるまで)
				size_t pos = (position + i) - beginHold;							// Hold開始からの位置
				const size_t max = (std::min)(result.size(), i + (m_params.holdVolEnv - pos));
				for (; i < max; i++) result[i] = 1.0;
				if (i >= result.size()) return result;
			}

			// DecayRate
			const size_t beginDecay = beginHold + m_params.holdVolEnv;				// DecayRate開始位置
			if (position + i < beginDecay + m_params.decayVolEnv) {
				size_t pos = (position + i) - beginDecay;							// DecayRate開始からの位置
				const size_t max = (std::min)(result.size(), i + (m_params.decayVolEnv - pos));
				for (; i < max; i++, pos++) {
					const size_t remain = m_params.decayVolEnv - pos;				// ディケイ完了までの時間(サンプル数)
					result[i] = m_params.sustainVolEnv + (m_divSustainDecay * remain);
				}
				if (i >= result.size()) return result;
			}

			// sustainVolEnv
			for (; i < result.size(); i++) result[i] = m_params.sustainVolEnv;

			return result;
		}

		// エンベロープ係数(0.0～1.0)取得 (キーオフ後)
		std::vector<T> getGainsReleaseRate(size_t position, size_t size)const {
			const size_t remain = m_params.releaseVolEnv - position;	// 終了(無音)までの残りサンプル数
			std::vector<T> result(std::min(size, remain));				// 今回出力するサンプル数
			for (size_t i = 0; i < result.size(); i++) {
				const auto linear = m_divRelease * (remain - i);		// キーオフから終了(無音)までの位置を 1.0～0.0 で表した値
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
				result[i] = curve(linear, 8);				// 8:さじ加減
			}
			return result;
		}

	};

}
