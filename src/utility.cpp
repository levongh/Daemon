#include "utility.h"

using namespace server;

bool CaseInsensitiveEqual::caseInsensitiveEqual(const std::string& lhs,
                                                const std::string& rhs) noexcept
{
    if (lhs.size() == rhs.size()) {
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char a, char b) {
                return std::tolower(a) == std::tolower(b);
                });
    }
    return false;
}


std::string Percent::encode(const std::string& value) noexcept
{
    static auto hexChars = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size()); //!Minimum size of result

    for (auto& chr : value) {
        if(!((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z') || chr == '-' || chr == '.' || chr == '_' || chr == '~')) {
            result += std::string("%") + hexChars[static_cast<unsigned char>(chr) >> 4] + hexChars[static_cast<unsigned char>(chr) & 15];
        }  else {
            result += chr;
        }
    }
    return result;
}

std::string Percent::decode(const std::string& value) noexcept
{
    std::string result;
    result.reserve(value.size() / 3 + (value.size() % 3)); // Minium size of result
    for (size_t i = 0; i < value.size(); ++i) {
        auto& chr = value[i];
        if (chr == '%' && i +2 < value.size()) {
            auto hex = value.substr(i + 1, 2);
            auto decodedChr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            result += decodedChr;
            i += 2;
        } else if (chr == '+') {
            result += ' ';
        } else {
            result += chr;
        }
    }
    return result;
}
