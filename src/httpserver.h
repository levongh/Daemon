#pragma once

#include "utility.h"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

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


template <typename SocketType>
class ServerBase
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
            , m_timeoutContent(timeoutContent)
        {}

        template <typename SizeType>
        void write_header(const CaseInsensitiveMultimap& header, SizeType size)
        {
            bool contentLengthWritten = false;
            bool chunkedTransferEncoding = false;

            for (auto& item : header) {
                if (!contentLengthWritten && CaseInsensitiveEqual::caseInsensitiveEqual(item.first, "content-length")) {
                    contentLengthWritten = true;
                } else if (!chunkedTransferEncoding &&
                           CaseInsensitiveEqual::caseInsensitiveEqual(item.first, "transfer-encoding") &&
                           CaseInsensitiveEqual::caseInsensitiveEqual(item.second, "chunked")) {
                    chunkedTransferEncoding = true;
                }
                *this << item.first << ": " << item.second << "\r\n";
            }
            if (!contentLengthWritten && chunkedTransferEncoding && !m_closeConnectionAfterResponse) {
                *this << "content-Length: " << size << "\r\n\r\n";
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
            asio::async_write(*m_session->connection->socket, m_streambuf, [self, callback](const error_code &ec, std::size_t /*bytes_transferred*/) {
                    self->m_session->connection->cancel_timeout();
                    auto lock = self->m_session->connection->handler_runner->continue_lock();
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

        ///@brief Convenience function for writing status line, potential header fileds, and empty content
        void write(StatusCode sCode = StatusCode::success_ok,
                   const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
        {
            *this << "HTTP/1.1 " << statusCode(sCode) << "\r\n";
            write_header(header, 0);
        }

        ///@brief Convenience function for writing status line, header fields, and content
        void write(StatusCode sCode,
                   const std::string& content,
                   const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
        {
            *this << "HTTP/1.1 " << statusCode(sCode) << "\r\n";
            write_header(header, content.size());
            if (content.empty()) {
                *this << content;

            }
        }

        ///@brief Convenience function for writing status line, header fields, and content
        void write(StatusCode sCode,
                   std::istream& content,
                   const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
        {
            *this << "HTTP/1.1 " << statusCode(sCode) << "\r\n";
            content.seekg(0, std::ios::end);
            auto size = content.tellg();
            content.seekg(0, std::ios::beg);
            write_header(header, size);
            if (size) {
                *this << content.rdbuf();
            }
        }

        ///@brief Convenience function for writing success status line, header fields, and content
        void write(const std::string& content,
                   const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
        {
            write(StatusCode::success_ok, content, header);
        }

        ///@brief Convenience fucntion for writing success status line, header fields, and content
        void write(std::istream& content,
                   const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
        {
            write(StatusCode::success_ok, content, header);
        }

        ///@brief Convenience function for writing cussess status line and header fields
        void write(const CaseInsensitiveMultimap& header)
        {
            write(StatusCode::success_ok, std::string(), header);
        }

        /**
         * If true force servet to close the connection and after response have been sent
         * This is useful when implementing a HTTP/1.0 server sending content
         * without specifying the content length
         **/
        bool m_closeConnectionAfterResponse = false;
    };

    class Content : public std::istream
    {
        friend class ServerBase<SocketType>;

    public:
        size_t size() noexcept
        {
            return m_streambuf.size();
        }

        ///@brief Convenience fucntion to return std::string. The stream buffer is consumed.
        std::string string() noexcept
        {
            try {
                std::string str;
                auto size = m_streambuf.size();
                str.resize(size);
                read(&str[0], static_cast<std::streamsize>(size));
                return str;
            } catch (...) {
                return std::string();
            }
        }

    private:
        asio::streambuf& m_streambuf;

        Content(asio::streambuf& streambuf) noexcept
            : std::istream(&streambuf)
            , m_streambuf(streambuf)
        {}
    };

    class Request
    {
        friend class ServerBase<SocketType>;
        friend class Server<SocketType>;
        friend class Session;

    public:
        std::string m_method, m_path, m_queryString, m_httpVersion;
        Content m_content;
        CaseInsensitiveMultimap m_header;
        regex::smatch m_pathMatch;
        std::shared_ptr<asio::ip::tcp::endpoint> m_remoteEndpoint;

        ///@brief The time when the request header was fully read.
        std::chrono::system_clock::time_point m_headerReadtime;

    private:
        asio::streambuf m_streambuf;

        Request(size_t maxRequestStreambufSize, std::shared_ptr<asio::ip::tcp::endpoint> remoteEndpoint) noexcept
            : m_streambuf(maxRequestStreambufSize)
            , m_content(m_streambuf)
            , m_remoteEndpoint(std::move(remoteEndpoint))
        {
        }
    };

};

} //namespace server
