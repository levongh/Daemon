#pragma once

namespace server {

class ResponseMessage
{
public:
    static bool parse(std::istream& stream,
                      std::string& version,
                      std::string& statusCode,
                      CaseInsensitiveMultimap &header) noexcept
    {
        header.clear();
        std::string line;
        getline(stream, line);
        size_t versionEnd = line.find(' ');
        if (versionEnd != std::string::npos) {
            if ( 5 < line.size()) {
                version = line.substr(5, versionEnd - 5);
            } else {
                return false;
            }
            if ((versionEnd + 1) < line.size()) {
                statusCode = line.substr(versionEnd + 1, line.size() - (versionEnd + 1) - 1);
            } else {
                return false;
            }
            header = HttpHeader::parse(stream);
        } else {
            return false;
        }
        return true;
    }
};

} // namespace server
