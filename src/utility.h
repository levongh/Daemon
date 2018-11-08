#pragma once

#include <atomic>
#include <iostream>
#include <memory>

#include "statuscode.h"

namespace server {

class CaseInsensitiveEqual
{
public:

    static bool caseInsensitiveEqual(const std::string& lhs,
                                     const std::string& rhs) noexcept;

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

using CaseInsensitiveMultimap = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;

#include "percent.h"
#include "querystring.h"
#include "httpheader.h"

} //namespace server
