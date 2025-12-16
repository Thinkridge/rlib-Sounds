

#ifndef _MSC_VER
#include <bits/stdc++.h>
#else
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <variant>
#endif

#include <boost/program_options.hpp>

#include "./sequencer/SmfToWav.h"
#include "./sequencer/SoundfontMidiModule.h"
#include "./sequencer/SoundfontInfo.h"
// #include "./fm/FmMidiModule.h"

using namespace rlib;


#if 1
template <typename T> void findFiles(const std::filesystem::path& directory, const std::string& pattern, const T& f) {

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

}
#endif



int main(const int argc, const char* const argv[])
{
	namespace po = boost::program_options;

	try {
		std::string input, pathSoundfont, pathSoundfontDir, output;
		po::options_description desc("options");
		desc.add_options()
			("version", "show version")
			("help", "show help")
			("preset", "show preset")
			("input,i", po::value(&input)->required(), "input file (required)")				// 入力SMFファイルパス
			("soundfont,s", po::value(&pathSoundfont)->required(), "input file (required)")	// 入力Soundfontファイルパス(デフォルトのsoundfont)
			("soundfontDir,d", po::value(&pathSoundfontDir), "input folder")				// 入力Soundfontファイルフォルダ
			("output,o", po::value(&output)->required(), "output file (required)")			// 出力Wavファイル
			;

		po::positional_options_description pd;
		// pd.add("input", -1);

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);

		if (vm.count("version")) {
			std::cout << "smftowav version 1.0.4" << std::endl;
			return 0;
		}

		if (vm.count("help")) {
			std::cout << desc << std::endl;		// ヘルプ表示
			return 0;
		}

		if (vm.count("preset")) {
			auto pathSoundfontDir = vm["soundfontDir"].as<std::string>();
			auto sfDir = std::filesystem::u8path(pathSoundfontDir);

			const std::filesystem::path& directory = sfDir;
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

			// {// fm
			// 	json["fm"] = fm::MidiModuleT<float>(44100).getPresetInfo();
			// }

			std::cout << json.stringify(Json::Stringify::standard) << std::endl;


			return 0;
		}

		po::notify(vm);

		const auto smf = [&] {
			auto path = std::filesystem::u8path(input);
			std::fstream fs(path, std::ios::in | std::ios::binary);
			if (fs.fail()) {
				throw std::runtime_error("input file open error.");
			}
			return midi::Smf::fromStream(fs);
		}();

#if 0
		const auto sf = [&] {
			auto path = std::filesystem::u8path(pathSoundfont);
			std::fstream fs(path, std::ios::in | std::ios::binary);
			if (fs.fail()) {
				throw std::runtime_error("soundfont file open error.");
			}
			return soundfont::SoundfontT<float>::fromStream(fs);
			}();

		const Wav wav = rlib::smfToWav(smf, sf, 44100);
#else
		const auto smfToWav = rlib::SmfToWav::create(smf);
		const Wav wav = smfToWav.toWavUsingPath<float>(std::filesystem::u8path(pathSoundfont), std::filesystem::u8path(pathSoundfontDir), 44100);
#endif

		auto path = std::filesystem::u8path(output);
		std::fstream fs(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (fs.fail()) {
			throw std::runtime_error("output file open error.");
		}

		wav.exportFile(fs);

#if 0
		try {
			const sequencer::Smf smf = sequencer::mmlToSmf(mml);
			auto fileImage = smf.getFileImage();

			auto path = std::filesystem::u8path(output);
			std::fstream fs(path, std::ios::out | std::ios::binary | std::ios::trunc);
			if (fs.fail()) {
				throw std::runtime_error("output file open error.");
			}
			fs.write(reinterpret_cast<const char* const>(fileImage.data()), fileImage.size());

		} catch (const sequencer::MmlCompiler::Exception& e) {
			const size_t lineNumber = [&] {		// 行番号取得
				static const std::regex re(R"(\r\n|\n|\r)");
				auto i = std::sregex_iterator(mml.cbegin(), e.it, re);
				size_t lf = std::distance(i, std::sregex_iterator()); // 改行の数
				return lf + 1;
			}();
			const auto msg = sequencer::MmlCompiler::Exception::getMessage(e.code);	// エラーメッセージ取得
			auto errorWord = [&] {
				std::string s(e.errorWord);
				std::smatch m;
				if (std::regex_search(s, m, std::regex(R"(\r\n|\n|\r)"))) {
					s = std::string(s.cbegin(), m[0].first);
				}
				return s;
			}();

			const auto s = string::format(R"(line %d %s : %s)", lineNumber, errorWord, msg);
			throw std::runtime_error(s);
		}
#endif

	} catch (std::exception& e) {
		std::clog << e.what() << std::endl;
		return 1;
	}

	return 0;
}
