#pragma once

namespace server {

///@brief Percent encoding and decoding
class Percent
{
public:
    ///@ Returns percent-encoded string
    static std::string encode(const std::string& value) noexcept
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

    ///@ Returns percent-decoded string
    static std::string decode(const std::string& value) noexcept
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

};

} //namespace server
