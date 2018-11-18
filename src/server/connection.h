#pragma once

#include "../util/lock.h"

namespace server {

template <typename SocketType>
class Connection : public std::enable_shared_from_this<Connection<SocketType> >
{
public:
    template <typename... Args>
        Connection(std::shared_ptr<ScopeRunner> handlerRunner, Args&&... args) noexcept
        : m_handlerRunner(std::move(handlerRunner))
        , m_socket(new SocketType(std::forward<Args>(args)...))
        {}

    void close() noexcept
    {
        error_code ec;
        // The following operations seems to be needed to run sequentially
        std::unique_lock<std::mutex> lock(m_socketCloseMutex);
        m_socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        m_socket->lowest_layer().close(ec);
    }

    void set_timeout(long seconds) noexcept
    {
        if (seconds == 0) {
            m_timer = nullptr;
            return;
        }
        m_timer = std::unique_ptr<asio::steady_timer>(new asio::steady_timer(m_socket->get_io_service()));
        m_timer->expires_from_now(std::chrono::seconds(seconds));
        auto self = this->shared_from_this();
        m_timer->async_wait([self](const error_code& ec) {
                if (!ec)
                self->close();
                });
    }

    void cancel_timeout() noexcept
    {
        if (m_timer) {
            error_code ec;
            m_timer->cancel(ec);
        }
    }

public:
    std::shared_ptr<ScopeRunner> m_handlerRunner;
    ///@brief Socket must be unique_ptr since asio::ssl::stream<asio::ip::tcp::socket> is not movable
    std::unique_ptr<SocketType> m_socket;
    std::mutex m_socketCloseMutex;

    std::unique_ptr<asio::steady_timer> m_timer;
    std::shared_ptr<asio::ip::tcp::endpoint> m_remoteEndpoint;
};

} // namespace server

