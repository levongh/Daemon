#pragma once

/**
 * @class  Server
 * @file   server.h
 * @brief  TODO add description
 * @author Levon Ghukasyan
 */

#include "server/serverbase.h"

namespace server {

using HTTP = asio::ip::tcp::socket;

template <>
class Server<HTTP> : public ServerBase<HTTP>
{
public:
    Server() noexcept
        : ServerBase<HTTP>::ServerBase(80)
    {
    }

protected:
    void accept() override
    {
        auto connection = createConnection(*m_ioService);
        m_acceptor->async_accept(*connection->m_socket, [this, connection] (const error_code& ec) {
            auto lock = connection->m_handlerRunner->continue_lock();
            if (!lock) {
                return;
            }
            ///@brief Immediately start accepting a new connection (unless m_ioService has been stopped)
            if (ec != asio::error::operation_aborted) {
                this->accept();
            }

            auto session = std::make_shared<Session<HTTP> >(m_config.m_maxRequestStreambufSize, connection);
            if (!ec) {
                asio::ip::tcp::no_delay option(true);
                error_code ec;
                session->m_connection->m_socket->set_option(option, ec);
                this->read(session);
            } else if (m_onError) {
                this->m_onError(session->m_request, ec);
            }
        });
    }
};


} // namespace server
