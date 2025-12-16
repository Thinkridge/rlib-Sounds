#include <stdio.h>
#include <emscripten.h>
#include <emscripten/bind.h>

#include <iostream>
#include <memory>
#include <vector>
#include <sstream>
#include <regex>
#include <exception>
#include <future>


#include "../stringformat/StringFormat.h"
#include "../sequencer/Soundfont.h"
#include "../sequencer/SoundfontInfo.h"
#include "../sequencer/Smf.h"
#include "../sequencer/SmfToWav.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <sstream>
#include <memory>
#include <thread>

using namespace emscripten;

// Jsonをemscripten::val::object()に変換
emscripten::val jsonToEmscriptenObj(const rlib::Json& json) {
	using Json = rlib::Json;
	struct F {
		static emscripten::val f(const Json& json) {
			if (json.isType(Json::Type::String)) {
				const std::string s = json.get<std::string>();
				return emscripten::val::u8string(s.c_str());
			} else if (json.isType(Json::Type::Int)) {
				const auto n = json.get<int>();
				return emscripten::val(n);
			} else if (json.isType(Json::Type::Float)) {
				const auto n = json.get<double>();
				return emscripten::val(n);
			} else if (json.isType(Json::Type::Bool)) {
				const auto n = json.get<bool>();
				return emscripten::val(n);
			} else if (json.isType(Json::Type::Null)) {
				return emscripten::val::null();
			} else if (json.isType(Json::Type::Map)) {
				auto val = emscripten::val::object();
				const auto& map = json.get<Json::Map>();
				for (auto& i : map) {
					val.set(i.first, F::f(i.second));
				}
				return val;
			} else if (json.isType(Json::Type::Array)) {
				std::vector<emscripten::val> v;
				const auto& list = json.get<Json::Array>();
				for (auto& i : list) {
					v.emplace_back(F::f(i));
				}
				return emscripten::val::array(std::move(v));
			}
			assert(false);
			return emscripten::val::null();
		}
	};
	return F::f(json);
}


using Soundfont = std::shared_ptr<const rlib::soundfont::Soundfont>;

Soundfont loadSoundfont(const std::string& sfBinary) {
    // std::cout << "loadSoundfont" << std::endl;
	std::istringstream is(sfBinary, std::ios::binary);
	auto soundfont = std::make_shared<Soundfont::element_type>(rlib::soundfont::Soundfont::fromStream(is));
    // std::cout << "loadSoundfont finished" << std::endl;
	return soundfont;
}

class AppFuture {
public:
	using ResultType = std::variant<std::monostate, std::string, Soundfont, std::vector<uint8_t>>;
	using ValueType = std::tuple<int, std::string, ResultType>;
	std::future<ValueType> m_future;
public:
	AppFuture(decltype(m_future) && f)
		:m_future(std::move(f))
	{}
};

AppFuture smfToWav(Soundfont* soundFont, const std::string& smfBinary) {
	std::cout << "smfToWav" << std::endl;
	auto f = std::async(std::launch::async, [soundFont, is = std::istringstream(smfBinary, std::istringstream::binary)]()mutable->AppFuture::ValueType {
		try {
			// Uint8Array であるかどうかをチェック
			//if (!smfBinary.instanceof(emscripten::val::global("Uint8Array"))) {
			//	std::cerr << "Error: Argument is not a Uint8Array." << std::endl;
			//	throw std::runtime_error("Error: Argument is not a Uint8Array.");
			//}
			auto smf = rlib::midi::Smf::fromStream(is);

			auto output = [&] {
				std::ostringstream oss;
				{
					const auto smfToWav = rlib::SmfToWav::create(smf);
					rlib::soundfont::MidiModuleT<float> midiModule(*soundFont, 44100);
					std::map<std::string, std::reference_wrapper<rlib::midi::MidiModuleBase<float>>> mapMidiModule;
					mapMidiModule.emplace("", midiModule);
					const rlib::Wav wav = smfToWav.toWav<float>(mapMidiModule, 44100);
					wav.exportFile(oss);
				}
				return oss.str();
			}();
			return std::make_tuple(0, std::string(), AppFuture::ResultType(std::vector<uint8_t>(output.begin(), output.end())));
		} catch (std::exception& e) {
			std::cout << "smfToWav std::exception" << std::endl;
			return std::make_tuple(-1, e.what(), AppFuture::ResultType());
		} catch (...) {
		}
		std::cout << "smfToWav unknown exception" << std::endl;
		return std::make_tuple(-1, "unknown exception", AppFuture::ResultType());
	});
	return AppFuture(std::move(f));
}


EMSCRIPTEN_BINDINGS(moduleSoundfont) {
    emscripten::function("loadSoundfont", &loadSoundfont, emscripten::return_value_policy::take_ownership());
    emscripten::function("smfToWav", &smfToWav, emscripten::return_value_policy::take_ownership());

	emscripten::class_<Soundfont>("Soundfont")
		.function("info", std::function{ [](const Soundfont& self) {
			const rlib::Json json = rlib::soundfont::getSoundfontInfo(*self);
			return jsonToEmscriptenObj(json);
		} });

	emscripten::class_<AppFuture>("AppFuture")
		.function("get", std::function{ [](AppFuture& self) {
			auto t = self.m_future.get();
			auto ret = emscripten::val::object();
			const auto errorCode = std::get<int>(t);
			// ret.set("ok", errorCode == 0);
			ret.set("errorCode", errorCode);
			if (!std::get<1>(t).empty()) {
				const std::string& s = std::get<1>(t);
				ret.set("message", emscripten::val::u8string(s.c_str()));
			}
			auto& v = std::get<2>(t);
			if (std::holds_alternative<std::string>(v)) {
				const auto& s = std::get<std::string>(v);
				ret.set("result", emscripten::val::u8string(s.c_str()));
			} else if (std::holds_alternative<std::vector<uint8_t>>(v)) {
				const auto& a = std::get<std::vector<uint8_t>>(v);
				ret.set("result", emscripten::val::global("Uint8Array").new_(emscripten::typed_memory_view(a.size(), a.data())));
			}
			return ret;
		} })
		.function("isProgress", std::function{ [](const AppFuture& self) {
			return self.m_future.wait_for(std::chrono::seconds(0)) == std::future_status::timeout;
		} });

	}
