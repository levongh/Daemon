#pragma once

#include "utility.h"

#include <mutex>

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
        std::string remoteEndpointAddress() noexcept
        {
            try {
                return m_remoteEndpoint->address().to_string();
            } catch (...) {
                std::string();
            }
        }

        unsigned short remoteEndpointPort() noexcept
        {
            return m_remoteEndpoint->port();
        }

        ///@brief Returns query keys with percent-decoded values;
        CaseInsensitiveMultimap parseQueryString() noexcept
        {
            return QueryString::parse(m_queryString);
        }

    public:
        std::string m_method, m_path, m_queryString, m_httpVersion;
        Content m_content;
        CaseInsensitiveMultimap m_header;
        regex::smatch m_pathMatch;
        std::shared_ptr<asio::ip::tcp::endpoint> m_remoteEndpoint;

        ///@brief The time when the request header was fully read.
        std::chrono::system_clock::time_point m_headerReadTime;

    private:
        asio::streambuf m_streambuf;

        Request(size_t maxRequestStreambufSize, std::shared_ptr<asio::ip::tcp::endpoint> remoteEndpoint) noexcept
            : m_streambuf(maxRequestStreambufSize)
            , m_content(m_streambuf)
            , m_remoteEndpoint(std::move(remoteEndpoint))
        {
        }
    };

protected:
    class Connection : public std::enable_shared_from_this<Connection>
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

    class Session
    {
    public:
        Session(size_t maxRequestStreambufSize, std::shared_ptr<Connection> connection) noexcept
            : m_connection(std::move(connection))
        {
            if (m_connection->remoteEndpoint) {
                error_code ec;
                m_connection->remoteEndpoint = std::make_shared<asio::ip::tcp::endpoint>(
                        m_connection->m_socket->lowest_layer().remote_endpoint(ec));
            }
            m_request = std::shared_ptr<Request>(new Request(maxRequestStreambufSize, m_connection->remoteEndpoint));
        }

    public:
        std::shared_ptr<Connection> m_connection;
        std::shared_ptr<Request> m_request;
    };
};

} //namespace server
