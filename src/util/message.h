#pragma once

namespace server {

class RequestMessage
{
public:
    /// Parse request line and header fields
    static bool parse(std::istream& stream,
                      std::string& method,
                      std::string& path,
                      std::string& queryString,
                      std::string& version,
                      CaseInsensitiveMultimap& header) noexcept
    {
        header.clear();
        std::string line;
        getline(stream, line);
        size_t methodEnd;
        if ((methodEnd = line.find(' ')) != std::string::npos) {
            method = line.substr(0, methodEnd);
            size_t queryStart = std::string::npos;
            size_t pathAndQueryStringEnd = std::string::npos;
            for (size_t i = methodEnd + 1; i < line.size(); ++i) {
                if (line[i] == '?' && (i + 1) < line.size()) {
                    queryStart = i + 1;
                } else if (line[i] == ' ') {
                    pathAndQueryStringEnd = i;
                    break;
                }
            }
            if (pathAndQueryStringEnd != std::string::npos) {
                if (queryStart != std::string::npos) {
                    path = line.substr(methodEnd + 1, pathAndQueryStringEnd - methodEnd - 2);
                    queryString = line.substr(queryStart, pathAndQueryStringEnd - queryStart);
                } else {
                    path = line.substr(methodEnd + 1, pathAndQueryStringEnd= methodEnd - 1);
                }

                size_t protocolEnd;
                if ((protocolEnd = line.find('/', pathAndQueryStringEnd + 1)) != std::string::npos) {
                    if (line.compare(pathAndQueryStringEnd + 1, protocolEnd - pathAndQueryStringEnd - 1, "HTTP") != 0) {
                        return false;
                    }
                    version = line.substr(protocolEnd + 1, line.size() - protocolEnd - 2);
                } else {
                    return false;
                }
                header = HttpHeader::parse(stream);
            } else {
                return false;
            }
        } else {
            return false;
        }
        return true;
    }
};

class ResponseMessage
{
public:
    ///@brief Parse status line and header fields
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
