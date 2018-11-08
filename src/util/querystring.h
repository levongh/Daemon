#pragma once

namespace server {

///@brief Query string creation and parsing
class QueryString
{
public:
    ///@brief query string created from given field names and values
    static std::string create(const CaseInsensitiveMultimap& fields) noexcept
    {
        std::string result;
        bool first = true;
        for (auto& field : fields) {
            result += (!first ? "&" : "") + field.first + '=' + Percent::encode(field.second);
            first = false;
        }
        return result;
    }

    static CaseInsensitiveMultimap parse(const std::string& queryString) noexcept
    {
        CaseInsensitiveMultimap result;
        if (queryString.empty()) {
            return result;
        }
        size_t namePos = 0;
        auto nameEndPos = std::string::npos;
        auto valuePos = std::string::npos;
        for (size_t c = 0; c < queryString.size(); ++c) {
            if (queryString[c] == '&') {
                auto name = queryString.substr(namePos,
                        (nameEndPos == std::string::npos ? c : nameEndPos) - namePos);
                if (!name.empty()) {
                    auto value = valuePos == std::string::npos ?
                        std::string() : queryString.substr(valuePos, c - valuePos);
                        result.emplace(std::move(name), Percent::decode(value));
                }
                namePos = c + 1;
                nameEndPos = std::string::npos;
                valuePos = std::string::npos;
            } else if (queryString[c] == '=') {
                nameEndPos= c;
                valuePos = c + 1;
            }
        }
        if (namePos < queryString.size()) {
            auto name = queryString.substr(namePos, nameEndPos - namePos);
            if (!name.empty()) {
                auto value = valuePos >= queryString.size() ? std::string() : queryString.substr(valuePos);
                result.emplace(std::move(name), Percent::decode(value));
            }
        }
        return result;
    }

};


} //namespace server
