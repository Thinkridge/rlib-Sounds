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

emscripten::val smfToWav(Soundfont* soundFont, const std::string& smfBinary) {
	// std::cout << "smfToWav" << std::endl;
	auto ret = emscripten::val::object();
	try{
		std::ostringstream oss;
		{
			auto is = std::istringstream(smfBinary, std::istringstream::binary);
			auto smf = rlib::midi::Smf::fromStream(is);
			const auto smfToWav = rlib::SmfToWav::create(smf);
			rlib::soundfont::MidiModuleT<float> midiModule(*soundFont, 44100);
			std::map<std::string, std::reference_wrapper<rlib::midi::MidiModuleBase<float>>> mapMidiModule;
			mapMidiModule.emplace("", midiModule);
			const rlib::Wav wav = smfToWav.toWav<float>(mapMidiModule, 44100);
			wav.exportFile(oss);
		}
		const auto s = oss.str();
		ret.set("errorCode", 0);
		ret.set("result", emscripten::val::global("Uint8Array").new_(emscripten::typed_memory_view(s.size(),reinterpret_cast<const uint8_t*>(s.data()))));
		return ret;
	} catch (std::exception& e) {
		// std::cout << "smfToWav std::exception" << std::endl;
		auto ret = emscripten::val::object();
		ret.set("errorCode", -1);
		ret.set("message", emscripten::val::u8string(e.what()));
		return ret;
	} catch (...) {
		// std::cout << "smfToWav unknown exception" << std::endl;
	}
	ret.set("errorCode", -1);
	ret.set("message", emscripten::val::u8string("unknown exception"));
	return ret;
}


EMSCRIPTEN_BINDINGS(moduleSoundfont) {
    emscripten::function("loadSoundfont", &loadSoundfont, emscripten::return_value_policy::take_ownership());
    emscripten::function("smfToWav", &smfToWav, emscripten::return_value_policy::take_ownership());

	emscripten::class_<Soundfont>("Soundfont")
		.function("info", std::function{ [](const Soundfont& self) {
			const rlib::Json json = rlib::soundfont::getSoundfontInfo(*self);
			return jsonToEmscriptenObj(json);
		} });
}
