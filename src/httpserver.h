#pragma once

#include "utility.h"

#include <thread>
#include <mutex>
#include <unordered_set>

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
                    auto lock = self->m_session->connection->m_handlerRunner->continue_lock();
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

public:
    class Config
    {
        friend class ServerBase<SocketType>;

        Config(unsigned short port) noexcept
            : m_port(port)
        {
        }

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
    };

    ///@brief Set before calling start().
    Config m_config;

private:
    class regex_orderable : public regex::regex
    {
    public:
        regex_orderable(const char* regexCstr)
            : regex::regex(regexCstr)
            , m_str(regexCstr)
        {}

        regex_orderable(std::string regexStr)
            : regex::regex(regexStr)
            , m_str(regexStr)
        {}

        bool operator<(const regex_orderable& rhs) const noexcept
        {
            return m_str < rhs.m_str;
        }

    private:
        std::string m_str;
    };

public:
    ///@brief Warning: do not add or remove resources after start() is called
    std::map<regex_orderable, std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<SocketType>::Response>, std::shared_ptr<typename ServerBase<SocketType>::Request>)> > > m_resource;

    std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<SocketType>::Response>, std::shared_ptr<typename ServerBase<SocketType>::Request>)> > m_defaultResource;

    std::function<void(std::shared_ptr<typename ServerBase<SocketType>::Request>, const error_code &)> m_onError;

    std::function<void(std::unique_ptr<SocketType> &, std::shared_ptr<typename ServerBase<SocketType>::Request>)> m_onUpgrade;

    /// If you have your own asio::io_service, store its pointer here before running start().
    std::shared_ptr<asio::io_service> m_ioService;

    /// If you know the server port in advance, use start() instead.
    /// Returns assigned port. If io_service is not set, an internal io_service is created instead.
    /// Call before accept_and_run().
    unsigned short bind()
    {
        asio::ip::tcp::endpoint endpoint;
        if (m_config.m_address.size() > 0) {
            endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(m_config.m_address), m_config.m_port);
        } else {
            endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), m_config.m_port);
        }

        if (!m_ioService) {
            m_ioService = std::make_shared<asio::io_service>();
            m_internalIoService = true;
        }

        if (m_acceptor) {
            m_acceptor = std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*m_ioService));
        }
        m_acceptor->open(endpoint.protocol());
        m_acceptor->set_option(asio::socket_base::reuse_address(m_config.m_reuseAddress));
        m_acceptor->bind(endpoint);

        afterBind();
        return m_acceptor->local_endpoint().port();
    }

    /// If you know the server port in advance, use start() instead.
    /// Accept requests, and if io_service was not set before calling bind(), run the internal io_service instead.
    /// Call after bind().
    void acceptAndRun()
    {
        m_acceptor->listen();
        accept();

        if (m_internalIoService) {
            if (m_ioService->stopped()) {
                m_ioService->reset();
            }
            // If thread_pool_size>1, start m_io_service.run() in (thread_pool_size-1) threads for thread-pooling
            m_threads.clear();
            for (size_t c = 1; c < m_config.m_threadPoolSize; ++c) {
                m_threads.emplace_back([this]() {
                    this->m_ioService->run();
                        });
            }

            ///Main thread
            if (m_config.m_threadPoolSize > 0) {
                m_ioService->run();
            }

            ///Wait for the rest of the threads, if any, to finish as well
            for (auto& t : m_threads) {
                t.join();
            }
        }
    }

    ///@brief Start the server by calling bind and acceptAndRun()
    void start()
    {
        bind();
        acceptAndRun();
    }

    ///@brief Stop accepting new requests, and close current connections.
    void stop() noexcept
    {
        if(m_acceptor) {
            error_code ec;
            m_acceptor->close(ec);

            {
                std::unique_lock<std::mutex> lock(*m_connectionsMutex);
                for(auto &connection : *m_connections) {
                    connection->close();
                }
                m_connections->clear();
            }

            if(m_internalIoService) {
                m_ioService->stop();
            }
        }
    }

    virtual ~ServerBase() noexcept
    {
      m_handlerRunner->stop();
      stop();
    }

protected:
    bool m_internalIoService = false;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::vector<std::thread> m_threads;
    std::shared_ptr<std::unordered_set<Connection*> > m_connections;
    std::shared_ptr<std::mutex> m_connectionsMutex;
    std::shared_ptr<ScopeRunner> m_handlerRunner;

    ServerBase(unsigned short port) noexcept
        : m_config(port)
        , m_connections(new std::unordered_set<Connection *>())
        , m_connectionsMutex(new std::mutex())
        , m_handlerRunner(new ScopeRunner())
   {}

    virtual void afterBind()
    {}
    virtual void accept() = 0;

    template <typename... Args>
    std::shared_ptr<Connection> createConnection(Args&&... args) noexcept
    {
        auto connections = m_connections;
        auto connectionsMutex = m_connectionsMutex;
        auto connection = std::shared_ptr<Connection>(
                new Connection(m_handlerRunner, std::forward<Args>(args) ...),
                    [connections, connectionsMutex] (Connection* connection) {
                    std::unique_lock<std::mutex> lock(*connectionsMutex);
                    auto it = connections->find(connection);
                    if (it != connections.end()) {
                        connections->erase(it);
                    }
                    delete connection;
                });
        {
            std::unique_lock<std::mutex> lock(*connectionsMutex);
            connections->emplace(connection.get());
        }
        return connection;

    }

};

} //namespace server
