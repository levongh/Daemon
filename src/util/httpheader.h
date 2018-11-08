#pragma once

namespace server {

class HttpHeader
{
public:
    ///@brief Parse header fields
    static CaseInsensitiveMultimap parse(std::istream& stream) noexcept
    {
        CaseInsensitiveMultimap result;
        std::string line;
        getline(stream, line);

        size_t paramEnd;
        while ((paramEnd = line.find(':')) != std::string::npos) {
            size_t valueStart = paramEnd + 1;
            while (valueStart + 1 < line.size() && line[valueStart] == ' ') {
                ++valueStart;
            }
            if (valueStart < line.size()) {
                result.emplace(line.substr(0, paramEnd), line.substr(valueStart, line.size() - valueStart - 1));
            }
            getline(stream, line);
        }
        return result;

    }
};

} //namespace server
