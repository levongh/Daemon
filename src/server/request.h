#pragma once

#include "regexorderable.h"

namespace server {

template <typename SocketType>
class ServerBase;

template <typename SocketType>
class Server;

template <typename SocketType>
class Session;

template <typename SocketType>
class Request
{
    friend class ServerBase<SocketType>;
    friend class Server<SocketType>;
    friend class Session<SocketType>;

public:
    std::string remoteEndpointAddress() noexcept
    {
        try {
            return m_remoteEndpoint->address().to_string();
        } catch (...) {
            std::string();
        }
    }

    unsigned short remoteEndpointPort() noexcept
    {
        return m_remoteEndpoint->port();
    }

    ///@brief Returns query keys with percent-decoded values;
    CaseInsensitiveMultimap parseQueryString() noexcept
    {
        return QueryString::parse(m_queryString);
    }

public:
    std::string m_method, m_path, m_queryString, m_httpVersion;
    Content<SocketType> m_content;
    CaseInsensitiveMultimap m_header;
    regex::smatch m_pathMatch;
    std::shared_ptr<asio::ip::tcp::endpoint> m_remoteEndpoint;

    ///@brief The time when the request header was fully read.
    std::chrono::system_clock::time_point m_headerReadTime;

private:
    asio::streambuf m_streambuf;

    Request(size_t maxRequestStreambufSize, std::shared_ptr<asio::ip::tcp::endpoint> remoteEndpoint) noexcept
        : m_streambuf(maxRequestStreambufSize)
        , m_content(m_streambuf)
        , m_remoteEndpoint(std::move(remoteEndpoint))
        {
        }
};

} // namespace server
