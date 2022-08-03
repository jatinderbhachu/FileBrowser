#include "string_util.h"


std::wstring Util::Utf8ToWstring(const std::string& str) {
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}


std::string Util::WstringToUtf8(wchar_t* str) {
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(str);
}

