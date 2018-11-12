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

    };

};

} //namespace server
