#include "StringUtils.h"


std::wstring Util::Utf8ToWstring(const std::string& str) {
    if(str.empty()) return std::wstring();
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}


std::string Util::WstringToUtf8(const std::wstring& str) {
    if(str.empty()) return std::string();
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(str);
}


bool Util::isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

