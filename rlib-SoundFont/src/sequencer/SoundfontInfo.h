#pragma once

#include "../json/Json.h"
#include "../stringformat/StringFormat.h"
#include "./Soundfont.h"

namespace rlib::soundfont {

	inline Json getSoundfontInfo(const Soundfont& soundfont) {
		Json json;

		auto& m = json.ensureMap();
		{
			const auto & fi = soundfont.doc().fileInfo;
			auto &t = m["FileInfo"];
			auto str = [&](const char* key, const std::string& s) {
				auto &u = t[key];
				if(!s.empty()) u = s;
			};
			t["ifil"] = string::format("%d.%d", fi.ifil.major, fi.ifil.minor);
			t["isng"] = fi.isng;
			str("INAM", fi.INAM);
			{
				auto& u = t["iver"];
				if (fi.iver) u = string::format("%d.%d", fi.iver->major, fi.iver->minor);
			}
			str("ICRD", fi.ICRD);
			str("IENG", fi.IENG);
			str("IPRD", fi.IPRD);
			str("ICOP", fi.ICOP);
			str("ICMT", fi.ICMT);
			str("ISFT", fi.ISFT);
		}
		{
			auto& v = m["Presets"].ensureArray();
			for (auto& preset : soundfont.doc().presets) {
				auto& t = v.emplace_back();
				t["bank"] = preset.presetNo.first;
				t["no"] = preset.presetNo.second;
				t["name"] = preset.name;
			}
		}

		return json;
	}


}