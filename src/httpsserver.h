#pragma once

#include <boost/asio/ssl.hpp>
#include <algorithm>
#include <openssl/ssl.h>

#include "server/serverbase.h"

namespace server {

using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;

template <>
class Server<HTTPS> : public ServerBase<HTTPS>
{
public:
    Server(const std::string& certFile,
           const std::string& privateKeyFile,
           const std::string& verifyFile = std::string())
        : ServerBase<HTTPS>::ServerBase(443)
        , m_context(asio::ssl::context::tlsv12)
    {
        m_context.use_certificate_chain_file(certFile);
        m_context.use_private_key_file(privateKeyFile, asio::ssl::context::pem);
        if (verifyFile.size() > 0) {
            m_context.load_verify_file(verifyFile);
            m_context.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert | asio::ssl::verify_client_once);
            m_setSessionIdContext = true;
        }
    }

protected:
    void afterBind() override
    {
        if (m_setSessionIdContext) {
            ///@brief Creating sessionIdContext from address:port but reversed due to small SSL_MAX_SSL_SESSION_ID_LENGTH
            auto sessionIdContext = std::to_string(m_acceptor->local_endpoint().port()) + ':';
            sessionIdContext.append(m_config.m_address.rbegin(), m_config.m_address.rend());
            SSL_CTX_set_session_id_context(m_context.native_handle(), reinterpret_cast<const unsigned char*>(sessionIdContext.data()),
                    std::min<size_t>(sessionIdContext.size(), SSL_MAX_SSL_SESSION_ID_LENGTH));
        }
    }

    void accept() override
    {
        auto connection = createConnection(*m_ioService, m_context);
        m_acceptor->async_accept(connection->m_socket->lowest_layer(),
                [this, connection](const error_code& ec) {
            auto lock = connection->m_handlerRunner->continue_lock();
            if (!lock) {
                return;
            }
            if (ec != asio::error::operation_aborted) {
                this->accept();
            }

            auto session = std::make_shared<Session<HTTPS> >(m_config.m_maxRequestStreambufSize, connection);

            if (!ec) {
                asio::ip::tcp::no_delay option(true);
                error_code ec;
                session->m_connection->set_timeout(m_config.m_timeoutRequest);
                session->m_connection->m_socket->async_handshake(asio::ssl::stream_base::server,
                    [this, session] (const error_code& ec) {
                        session->m_connection->cancel_timeout();
                        auto lock = session->m_connection->m_handlerRunner->continue_lock();
                        if (!lock) {
                            return;
                        }
                        if (!ec) {
                            this->read(session);
                        } else if (m_onError) {
                            m_onError(session->m_request, ec);
                        }
                });
            } else if (m_onError) {
                m_onError(session->m_request, ec);
            }
        });
    }

protected:
    asio::ssl::context m_context;

private:
        bool m_setSessionIdContext;
};

} // namespace server
