#include "helper.h"

// Convert a wide Unicode string to an UTF8 string
std::string To_string(const std::wstring& wstr)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(wstr);
}

// Convert an UTF8 string to a wide Unicode String
std::wstring To_wstring(const std::string& str)
{
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(str);
}
