#pragma once

namespace server {

template <typename SocketType>
class ServerBase;

template <typename SocketType>
class Config
{
public:
    /**
     * @brief Port number to use. Defaults to 80 for HTTP and 443 for HTTPS.
     * Set 0 to get an assigned port
     */
    unsigned short m_port;

    /**
     * @brief if io_sevice is not set, number of threads that the server will use then start() is called
     * Default is 1 thread
     */
    size_t m_threadPoolSize = 1;
    ///@brief Timeout on request handling. Defaults to 5 seconds
    long m_timeoutRequest = 5;
    ///@brief Timeout on content handling. Defaults to 300 seconds
    long m_timeoutRequst = 300;
    /**
     * @brief Maximum size of request stream buffer. Defualts to architecture maximum.
     * Reacting this limit will result in a message size error code
     */
    size_t m_maxRequestStreambufSize = std::numeric_limits<size_t>::max();

    /**
     * @brief IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
     * If empty, the address will be any address.
     */
    std::string m_address;

    /**
     * @brief Set to false to avoid binding the socket to an address that is already in use.
     * Default is true.
     */
    bool m_reuseSddress = false;

private:
    friend class ServerBase<SocketType>;

    Config(unsigned short port) noexcept
        : m_port(port)
        {
        }

};

} //namespace server
