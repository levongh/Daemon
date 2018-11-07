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
