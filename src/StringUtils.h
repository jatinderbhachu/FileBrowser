#pragma once
#include <codecvt>
#include <string.h>

namespace Util {

    std::wstring Utf8ToWstring(const std::string& str);
    std::string WstringToUtf8(wchar_t* str);

}
