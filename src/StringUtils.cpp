#include "StringUtils.h"
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

std::wstring Util::Utf8ToWstring(const std::string& str) {
    if( str.empty() ) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring result( size, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
    return result;
}

std::string Util::WstringToUtf8(const std::wstring& str) {
    if( str.empty() ) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
    std::string result(size, 0 );
    WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size, NULL, NULL);
    return result;
}

bool Util::isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

