#pragma once

#include <atomic>
#include <ioatream>
#include <memory>
#include <string>

#include "statuscode.h"

namespace server {

class CaseInsensitiveEqual
{
public:

    static inline bool caseInsensitiveEqual(const std::string& lhs,
                                            const std::string& rhs) noexcept
    {
        if (lhs.size() == rhs.size()) {
            return std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char a, char b) {
                    return std::tolower(a) == std::tolower(b);
                    });
        }
        return false;
    }

    bool operator()(const std::string& lhs, const std::string& rhs) const noexcept
    {
        return CaseInsensitiveEqual::caseInsensitiveEqual(lhs, rhs);
    }
};

// Based on https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
class CaseInsensitiveHash
{
public:
    size_t operator()(const std::string& str) const noexcept
    {
        size_t h = 0;
        std::hash<int> hash;
        for (auto c : str) {
            h ^= hash(std::tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

} //namespace server
