
#include <bits/stdc++.h>

#include <boost/program_options.hpp>

#include "./sequencer/SoundfontInfo.h"

using namespace rlib;


#if 1
template <typename T> void findFiles(const std::filesystem::path & directory, const std::string & pattern, const T &f) {

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
		std::string input, pathSoundfontDir;
		po::options_description desc("options");
		desc.add_options()
			("version", "show version")
			("help", "show help")
			("soundfontDir,d", po::value(&pathSoundfontDir)->required(), "input folder")	// 入力Soundfontファイルフォルダ
			;

		po::positional_options_description pd;
		// pd.add("input", -1);

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);

		if (vm.count("version")) {
			std::cout << "sfinfo version 1.0.0" << std::endl;
			return 0;
		}

		if (vm.count("help")) {
			std::cout << desc << std::endl;		// ヘルプ表示
			return 0;
		}

		po::notify(vm);


		auto sfDir = std::filesystem::u8path(pathSoundfontDir);

		const std::filesystem::path& directory = sfDir;
		const std::string pattern("*.sf2");

		Json json;

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

		std::cout << json.stringify(Json::Stringify::standard) << std::endl;

	} catch (std::exception& e) {
		std::clog << e.what() << std::endl;
		return 1;
	}

	return 0;
}
