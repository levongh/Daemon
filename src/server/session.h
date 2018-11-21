#pragma once

namespace server {

template <typename SocketType>
class Connection;

template <typename SocketType>
class Request;

template <typename SocketType>
class Session
{
public:
    Session(size_t maxRequestStreambufSize, std::shared_ptr<Connection<SocketType> > connection) noexcept
        : m_connection(std::move(connection))
        {
            if (m_connection->m_remoteEndpoint) {
                error_code ec;
                m_connection->m_remoteEndpoint = std::make_shared<asio::ip::tcp::endpoint>(
                        m_connection->m_socket->lowest_layer().remote_endpoint(ec));
            }
            m_request = std::shared_ptr<Request<SocketType> >(new Request<SocketType>(maxRequestStreambufSize, m_connection->m_remoteEndpoint));
        }

public:
    std::shared_ptr<Connection<SocketType> > m_connection;
    std::shared_ptr<Request<SocketType> > m_request;
};


} //namespace server
