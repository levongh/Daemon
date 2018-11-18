#pragma once


#include <regex>

namespace server {

namespace regex = std;

class RegexOrderable : public regex::regex
{
public:
    RegexOrderable(const char* regexCstr)
        : regex::regex(regexCstr)
        , m_str(regexCstr)
    {}

    RegexOrderable(std::string regexStr)
        : regex::regex(regexStr)
        , m_str(regexStr)
    {}

    bool operator<(const RegexOrderable& rhs) const noexcept
    {
        return m_str < rhs.m_str;
    }

private:
    std::string m_str;
};

}

