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
    asio::ssl::context m_context;

private:
        bool m_setSessionIdContext;
};

} // namespace server
