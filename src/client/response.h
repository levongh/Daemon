#pragma once

namespace client {

template <typename SocketType>
class Content;

template <typename SocketType>
class ClientBase;

template <typename SocketType>
class Client;

template <typename SocketType>
class Response
{
    friend class ClientBase<SocketType>;
    friend class Client<SocketType>;

public:
    std::string m_httpVersion;
    std::string m_statusCode;

    Content<SocketType> m_content;

    server::CaseInsensitiveMultimap m_header;

private:
    asio::streambuf m_streambuf;

    Response(size_t maxResponseStreambufSize) noexcept
        : m_streambuf(maxResponseStreambufSize)
        , m_content(m_streambuf)
    {}
};

} // namespace client
