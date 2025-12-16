#pragma once

#include <string>

namespace rlib::string {

	// system local to UTF8
	inline std::string SystemLocaleToUtf8(const std::string& localeStr) {
		static const std::locale loc = [] {
			boost::locale::generator g;
			g.locale_cache_enabled(true);
			return g(boost::locale::util::get_system_locale());
		}();
		return boost::locale::conv::to_utf<char>(localeStr.c_str(), loc);
	}

#ifdef _MSC_VER
	// MultiBute to UTF8
	inline std::string AtoUtf8(const char* psz) {
		std::string s(CT2A(CString(psz), CP_UTF8));
		return s;
	}
#else
	// linux版は未実装
#endif

#ifdef _MSC_VER
	// UTF8 → CString
	inline CString Utf8toT(const char* psz) {
		CString s(CA2T(psz, CP_UTF8));	// UTF8 → CString
		return s;
	}

	// CString → UTF8
	inline std::string TtoUtf8(LPCTSTR lpsz) {
		return std::string(CT2A(lpsz, CP_UTF8));
	}

	// CString → MultiByte
	inline std::string TtoMbcs(LPCTSTR lpsz) {
		return std::string(CT2A(lpsz));
	}

#endif

	// HEX文字列をバイナリにする
	// 例	auto v = String::HexStringToBinary("0189ABEFabef");
	template <class CharT> std::optional<std::vector<char>> HexStringToBinary(const CharT* lpszFormat) {
		std::vector<char> v;
		try {
			boost::algorithm::unhex(lpszFormat, std::back_inserter(v));
		}
		catch (...) {	// 入力が不正だった場合
			return boost::none;
		}
		return v;
	}

	// バイナリデータをHEX文字列にする
	template <typename InputIterator, typename OutputIterator> std::string BinaryToHexString(InputIterator b, OutputIterator e) {
		std::string s;
		try {
			boost::algorithm::hex(b, e, std::back_inserter(s));
		} catch (...) {	// なんかしらエラー
			assert(false);
		}
		return s;
	}

	// 文字列 trim 全角スペース対応
	inline std::string trim(const std::string& s) {
		// boost::regex で全角スペースを正しく処理する術がわからない。バイナリとして扱うことも不可だった。
		std::smatch m;
		static const std::regex re(u8R"(^(\s|　)*([\s\S]+?)(\s|　)*$)");
		if (std::regex_search(s, m, re)) {
			if (auto sm = m[2]; sm.matched) {
				return sm.str();
			}
		}
		return "";
	}

}
