#pragma once

#include "MidiEvent.h"
#include "Smf.h"
#include "Wav.h"
#include "TempoList.h"
#include "MidiModule.h"

// toWavUsingPath 用
#include "SoundfontMidiModule.h"
#include "./SoundfontInfo.h"
//#include "../fm/FmMidiModule.h"

namespace rlib {

	class SmfToWav {
		SmfToWav(TempoListT<double>&& tempoList, std::map<std::string, midi::Smf::Events>&& mapEvents)
			: m_tempoList(tempoList)
			, m_mapEvents(mapEvents)
		{
		}
	public:
		const TempoListT<double>						m_tempoList;
		const std::map<std::string, midi::Smf::Events>	m_mapEvents;		// map<instrument,events>

		static SmfToWav create(const midi::Smf& smf)
		{
			TempoListT<double>							tempoList;
			std::map<std::string, midi::Smf::Events>	mapEvents;
			const auto mul = TempoListT<double>::timeBase / smf.timeBase;			// timebaseを480にする

			for (auto& track : smf.tracks) {

				std::string instrumentName;
				midi::Smf::Events *pEvents = nullptr;
				const auto getEvents = [&]{
					if (!pEvents) {
						auto& events = mapEvents[instrumentName];
						pEvents = &events;
					}
					return pEvents;
				};
				
				for (auto& event : track.events) {

					if (auto meta = std::dynamic_pointer_cast<const midi::EventMeta>(event.event)) {
						switch (meta->type) {
						case  midi::EventMeta::Type::instrumentName:
							instrumentName = meta->getText();
							pEvents = nullptr;
							break;
						case midi::EventMeta::Type::tempo:
							tempoList.insert(event.position * mul, static_cast<typename decltype(tempoList)::Type>(meta->getTempo()));
							break;
						default:
							getEvents()->emplace(event.position * mul, event.event);
							break;
						}
					} else {
						getEvents()->emplace(event.position * mul, event.event);
					}
				}
			}

			return SmfToWav(std::move(tempoList), std::move(mapEvents));

		}

		template <typename T = double> Wav toWav(const std::map<std::string, std::reference_wrapper<midi::MidiModuleBase<T>>>& mapMidiModule, uint32_t sampleRate = 44100) const {
			Wav wav;
			wav.setMode(Wav::Mode::Stereo16bit);
			wav.m_format.nSamplesPerSec = sampleRate;

			auto& wavData = wav.data<Wav::Stereo16>();

			{// 必要バッファ確保
				const auto timeLength = m_tempoList.getTime([&]{
					uint64_t max = 0;
					for (auto& i : m_mapEvents) {
						if (!i.second.empty()) {
							max = (std::max)(max, i.second.rbegin()->position);
						}
					}
					return max;
				}());
				if (timeLength > 10 * 60) {		// (とりあえず)10分以上はエラー
					throw std::runtime_error("Songs must be no longer than 10 minutes.");
				}
				wavData.reserve(static_cast<size_t>((timeLength + 20) * sampleRate));	// 必要バッファ確保
			}

			if (mapMidiModule.empty()) {
				throw std::runtime_error("MidiModules is empty.");
			}

			//const auto mapMidiModule = [&] {
			//	std::map<std::string, std::unique_ptr<soundfont::MidiModuleT<T>>> mapMidiModule;
			//	for (auto& i : mapSoundfont) {
			//		mapMidiModule.emplace(i.first, std::make_unique<soundfont::MidiModuleT<T>>(i.second, sampleRate));
			//	}
			//	return mapMidiModule;
			//}();

			const auto combinedEvents = [&] {
				std::multimap<midi::Smf::Event, std::reference_wrapper<midi::MidiModuleBase<T>>, midi::Smf::Event::Less> combinedEvents;
				for (auto& i : m_mapEvents) {
					auto it = mapMidiModule.find(i.first);
					midi::MidiModuleBase<T>& midiModule = it != mapMidiModule.end() ? it->second : mapMidiModule.begin()->second;
					for (auto& j : i.second) {
						combinedEvents.emplace(j, midiModule);
					}
				}
				return combinedEvents;
			}();

			if (combinedEvents.empty()) {
				return wav;
			}

			const auto pushSamples = [&](const auto& samples) {
				for (const auto& s : samples) {
					const auto toInt = [](T n) {return std::clamp(static_cast<int32_t>(n * 32768), -32768, 32767); };
					wavData.emplace_back(toInt(s.l), toInt(s.r));
				}
			};
			const auto pushEmpty = [&](size_t size) {
				wavData.resize(wavData.size() + size);
			};

			struct {
				const uint32_t m_sampleRate;
				std::uintmax_t m_current = 0;
				size_t next(double time) {
					const auto next = static_cast<std::uintmax_t>(time * m_sampleRate);
					const std::intmax_t needSize = next - m_current;
					if (needSize < 0) {
						throw std::runtime_error("readSamplesUntilTime failed time.");		// failsafe 
					}
					m_current += needSize;
					return needSize;
				}
			}renderedSize{ sampleRate };

			{// 先頭のイベントまでの無音
				auto current = combinedEvents.cbegin();
				const auto nextTime = m_tempoList.getTime(current->first.position);
				const auto needSize = renderedSize.next(nextTime);
				pushEmpty(needSize);
			}

			auto render = [&](size_t size) {
				std::vector<typename midi::StereoSample<T>> samples;
				for (auto& it : mapMidiModule) {
					auto readed = static_cast<midi::MidiModuleBase<T>&>(it.second).readSamples(size);
					if (samples.empty()) {
						samples = std::move(readed);
					} else {
						samples.resize((std::max)(samples.size(), readed.size()));
						for (size_t i = 0; i < readed.size(); i++) {
							samples[i].l += readed[i].l;
							samples[i].r += readed[i].r;
						}
					}
				}
				pushSamples(samples);
				if (auto lack = size - samples.size(); lack > 0) pushEmpty(lack);
			};

			for (auto current = combinedEvents.cbegin(); current != combinedEvents.end(); ) {
				
				const auto next = combinedEvents.upper_bound(current->first.position);
				for (auto it = current; it != next; it++) {
					const midi::Smf::Event &event = it->first;
					midi::MidiModuleBase<T>& midiModule = it->second;
					midiModule.setMidiEvent(*event.event);
				}

				if (next != combinedEvents.end()) {
					const auto nextTime = m_tempoList.getTime(next->first.position);
					const auto needSize = renderedSize.next(nextTime);
					render(needSize);
				}

				current = next;
			}

			for (size_t c = 0; c < 10; c++) {	// 余韻は最長で10秒としておく

				if ([&] {	// 終了?
					for (auto& it : mapMidiModule) {
						if (!static_cast<midi::MidiModuleBase<T>&>(it.second).isSilence()) return false;
					}
					return true;
				}()) break;

				render(wav.m_format.nSamplesPerSec);
			}

			pushEmpty(sampleRate / 2);			// 最後に0.5秒

			return wav;
		}

#ifndef __EMSCRIPTEN__
		// フォルダを指定することでwav生成まで行うユーティリティ
		template <typename T = double> Wav toWavUsingPath(const std::filesystem::path& defaultSoundfont, const std::filesystem::path& soundfontDir, uint32_t sampleRate = 44100) const {
			const std::map<std::string, std::shared_ptr<midi::MidiModuleBase<T>>> mapMidiModule = [&] {
				using Soundfont = soundfont::Soundfont;
				std::map<std::string, std::shared_ptr<midi::MidiModuleBase<T>>> mapMidiModule;
				std::shared_ptr<const Soundfont> spDefaultSoundfont;	// デフォルトSoundfont

				const auto loadSoundfont = [&](const std::filesystem::path& fullpath)->std::shared_ptr<midi::MidiModuleBase<T>> {
					try {
						const bool bDefault = std::filesystem::canonical(defaultSoundfont) == std::filesystem::canonical(fullpath);	// デフォルトSoundfont？
						auto spSoundfont = bDefault && spDefaultSoundfont ? spDefaultSoundfont : nullptr;
						if (!spSoundfont) {
							std::fstream fs(fullpath, std::ios::in | std::ios::binary);
							if (fs.fail()) {
								std::clog << "not found " << fullpath << std::endl;
								return nullptr;
							}
							spSoundfont = std::make_shared<const Soundfont>(Soundfont::fromStream(fs));
						}
						if (bDefault) spDefaultSoundfont = spSoundfont;		// デフォルトSoundfontの読み込みだったならキープ
						return std::make_shared<soundfont::MidiModuleT<T>>(spSoundfont, sampleRate);
					} catch (std::exception& e) {
						std::clog << "soundfont parse exception " << fullpath << " " << e.what() << std::endl;
					} catch (...) {
						std::clog << "soundfont parse unknown exception " << fullpath << std::endl;
					}
					return nullptr;
				};

				for (auto& i : m_mapEvents) {
					const std::string& instrument = i.first;
					if (auto it = mapMidiModule.find(instrument); it != mapMidiModule.end()) continue;	// 読み込み済なら次へ

					try {
						if (!instrument.empty()) {

							if (instrument == "fm") {									// FM音源なら
								mapMidiModule[instrument] = std::make_shared<fm::MidiModuleT<T>>(sampleRate);
								continue;
							}

							if (!soundfontDir.empty()) {
								if (auto sp = loadSoundfont(soundfontDir / instrument)) {	// SoundFont の読み込みを試みる
									mapMidiModule.emplace(instrument, sp);
									continue;
								}
							}

						}
					} catch (std::exception& e) {
						std::clog << "failed instrument:" << instrument << " " << e.what() << std::endl;
					} catch (...) {
						std::clog << "failed instrument:" << instrument << std::endl;
					}

					// default soundfont を使用する
					if (auto it = mapMidiModule.find(""); it == mapMidiModule.end()) {
						auto sp = loadSoundfont(defaultSoundfont);
						mapMidiModule.emplace("", sp);
					}
				}

				return mapMidiModule;
			}();

			const auto mapRef = [&]{
				std::map<std::string, std::reference_wrapper<midi::MidiModuleBase<T>>> m;
				for (auto& i : mapMidiModule) m.emplace(i.first, *i.second);
				return m;
			}();

			return toWav<T>(mapRef, sampleRate);
		}

		// soundfont(フォルダを指定)及びFMの音色データをJSONで出力するユーティリティ
		static Json getPresetsInfo(const std::filesystem::path& soundfontDir) {

			const auto findFiles = [](const std::filesystem::path & directory, const std::string & pattern, auto f) {
				// ワイルドカードパターンを正規表現に変換
				const auto regexPattern = [&] {
					std::string regex_pattern = std::regex_replace(pattern, std::regex("\\."), "\\.");
					regex_pattern = std::regex_replace(regex_pattern, std::regex("\\*"), ".*");
					regex_pattern = std::regex_replace(regex_pattern, std::regex("\\?"), ".");
					return std::regex(regex_pattern, std::regex_constants::icase);
				}();
				for (const auto& entry : std::filesystem::directory_iterator(directory)) {
					if (entry.is_regular_file()) {
						if (std::regex_match(entry.path().filename().string(), regexPattern)) {
							// std::cout << entry.path().string() << std::endl;
							f(entry);
						}
					}
				}
			};

			const std::filesystem::path& directory = soundfontDir;
			const std::string pattern("*.sf2");

			Json json;

			// soundfont
			findFiles(directory, pattern, [&json](auto& entry) {
				const auto sf = [&] {
					std::fstream fs(entry.path(), std::ios::in | std::ios::binary);
					if (fs.fail()) {
						throw std::runtime_error("soundfont file open error.");
					}
					return soundfont::Soundfont::fromStream(fs);
				}();
				json[entry.path().filename().string()] = soundfont::getSoundfontInfo(sf);
			});

			// fm
			json["fm"] = fm::MidiModuleT<float>(44100).getPresetInfo();

			return json;
		}

#endif

	};

}
