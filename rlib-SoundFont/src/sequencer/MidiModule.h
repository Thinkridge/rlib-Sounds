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

		virtual void setMidiEvent(const midi::Event& ev) = 0;

		// レンダリング(波形データ出力（結果配列がsize未満なら完了=無音）
		virtual std::vector<StereoSample<T>> readSamples(size_t size) = 0;

		// Eventはリリース音も含めて全て処理されている状態か
		virtual bool isSilence()const = 0;
	};


}
