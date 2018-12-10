#pragma once

namespace client {

template <typename SocketType>
class ClientBase;

template <typename SocketType>
class Config
{
    friend class ClientBase<SocketType>;

public:
    Config() noexcept
    {}

private:
    long m_timeout = 0;
    long m_temeoutConnet = 0;
    size_t m_maxResponseStreambufSize = std::numeric_limits<size_t>::max();
    std::string m_proxyServer;
};

} // namespace client
