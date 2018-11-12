#pragma once

#include "utility.h"

#include <boost/asio.hpp>
#include <boost/asio/steade_timer.hpp>

namespace server {

namespace asio = boost::asio;
using error_code = boost::system::error_code;
namespace errc = boost::system::errc;
namespace make_error_code = boost::system::errc;

} //namespace server

#include <regex>

namespace server {

namespace regex = std;

}

namespace server {

template <typename SocketType>
class Server;


template <typename <SocketType>
ServerBase
{
protected:
    class Session;

public:
    class Response : public std::enable_shared_from_this<Response>, public std::ostream
    {
        friend class ServerBase<SocketType>;
        friend class Server<SocketType>;

        asio::streambuf m_streambuf;
        std::shared_ptr<Session> m_session;
        long m_timeoutContent;

        Response(std::shared_ptr<Session> session, long timeoutContent) noexcept
            : std::ostream(&m_streambuf)
            , m_session(std::move(session))
            , m_timeoutContent(timeout_content)
        {}

        template <typename SizeType>
        void write_header(const CaseInsensitiveMultimap& header, SizeType size)
        {
            bool contentLengthWritten = false;
            bool chunkedTransferEncoding = false;

            for (auto& item : header) {
                if (!contentLengthWritten && caseInsensitiveEqual(item.first, "content-length")) {
                    contentLengthWritten = true;
                } else if (!chunkedTransferEncoding &&
                           caseInsensitiveEqual(item.first, "transfer-encoding") &&
                           case_insensitive_equal(field.second, "chunked")) {
                    chunkedTransferEncoding = true;
                }
                *this << item.first << ": " << item.second << "\r\n";
            }
            if (!contentLengthWritten && chunkedTransferEncoding && !closeConnectionAfterResponse) {
                *this << "content-Length: " << sie << "\r\n\r\n";
            } else {
                *this << "\r\n";
            }
        }

     public:
        size_t size() noexcept
        {
            return m_streambuf.size();
        }

        ///@brief Use this function if you need to recursively send parts of a longer message
        void send(const std::function<void(const error_code&)>& callback = nullptr) noexcept
        {
            m_session->connection->set_timeout(m_timeoutContent);
            auto self = this->shared_from_this();  /// Keep Response instance alive through the following async_write
            asio::async_write(*session->connection->socket, streambuf, [self, callback](const error_code &ec, std::size_t /*bytes_transferred*/) {
                    self->session->connection->cancel_timeout();
                    auto lock = self->session->connection->handler_runner->continue_lock();
                    if(!lock)
                        return;
                    if(callback)
                        callback(ec);
             });
        }

        ///@brief Write directly to stream buffer using std::ostream::write
        void write(const char_type* ptr, std::streamsize n)
        {
            std::ostream::write(ptr, n);
        }
    };

};

} //namespace server
