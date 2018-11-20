
template <typename SocketType>
void ServerBase<SocketType>::start()
{
    bind();
    acceptAndRun();
}

template <typename SocketType>
void ServerBase<SocketType>::stop() noexcept
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

template <typename SocketType>
ServerBase<SocketType>::~ServerBase() noexcept
{
    m_handlerRunner->stop();
    stop();
}

template <typename SocketType>
ServerBase<SocketType>::ServerBase(unsigned short port) noexcept
    : m_config(port)
    , m_connections(new std::unordered_set<Connection<SocketType> *>())
    , m_connectionsMutex(new std::mutex())
    , m_handlerRunner(new ScopeRunner())
{}

template <typename SocketType>
void ServerBase<SocketType>::acceptAndRun()
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
    
    template <typename SocketType>
unsigned short ServerBase<SocketType>::bind()
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

template <typename SocketType>
template <typename... Args>
std::shared_ptr<Connection<SocketType> > ServerBase<SocketType>::createConnection(Args&&... args) noexcept
{
    auto connections = m_connections;
    auto connectionsMutex = m_connectionsMutex;
    auto connection = std::shared_ptr<Connection<SocketType> >(
            new Connection<SocketType> (m_handlerRunner, std::forward<Args>(args) ...),
            [connections, connectionsMutex] (Connection<SocketType>* connection) {
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

template <typename SocketType>
void ServerBase<SocketType>::read(const std::shared_ptr<Session<SocketType> >& session)
{
    session->m_connection->set_timeout(m_config.m_timeoutRequest);
    asio::async_read_until(*session->m_connection->m_socket,
        session->m_request->m_streambuf, "\r\n\r\b",
        [this, session](const error_code& ec, size_t bytesTransferred) {
            session->m_connection->cancel_timeout();
            auto lock = session->m_connection->m_handlerRunner->continue_lock();
            if (!lock) {
                return;
            }
            session->m_request->m_headerReadTime = std::chrono::system_clock::now();
            if ((!ec || ec == asio::error::not_found) &&
                    session->m_request->m_streambuf.size() == session->request->m_streambuf.max_size()) {
                auto response = ResponsePtr(new Response<SocketType>(session, m_config.m_timeoutContent));
                response->write(StatusCode::client_error_payload_too_large);
                response->send();
                if (m_onError) {
                    m_onError(session->m_request, make_error_code::make_error_code(errc::message_size));
                }
                return;
            }
            if (!ec) {
                // request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
                // "After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                // The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                // streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                size_t numAdditionalBytes = session->m_request->m_streambuf.size() - bytesTransferred;
                if (!RequestMessage::parse(session->m_request->m_content,
                            session->m_request->m_method,
                            session->m_request->m_path,
                            session->m_request->m_queryString,
                            session->m_request->m_httpVersion,
                            session->m_request->m_header)) {
                    if (m_onError) {
                        m_onError(session->m_request, make_error_code::make_error_code(errc::protocol_error));
                    }
                    return;
                }
                // if content, read that as well
                auto headerIt = session->m_request->m_header.find("Content-Length");
                if (headerIt != session->m_request->m_header.end()) {
                    unsigned long long contentLength = 0;
                    try {
                        contentLength = stoull(headerIt->second);
                    } catch (const std::exception& ) {
                        if (m_onError) {
                            m_onError(session->m_request, make_error_code::make_error_code(errc::protocol_error));
                        }
                        return;
                    }
                    if (contentLength > numAdditionalBytes) {
                        session->m_connection->set_timeout(m_config.m_timeoutContent);
                        asio::async_read(*session->m_connection->m_socket,
                                session->m_request->m_streambuf,
                                asio::transfer_exactly(contentLength - numAdditionalBytes),
                                [this, session] (const error_code &ec, size_t) {
                        session->m_connection->cancel_timeout();
                        auto lock = session->m_connection->m_handlerRunner->continue_lock();
                        if (!lock) {
                            return;
                        }
                        if (!ec) {
                            if (session->m_request->m_streambuf.size() == session->m_request.streambuf.max_size()) {
                                auto response = ResponsePtr(new Response<SocketType>(session, m_config->m_timeoutContent));
                                response->write(StatusCode::client_error_payload_too_large);
                                response->send();
                                if (m_onError) {
                                    m_onError(session->m_request,  make_error_code::make_error_code(errc::message_size));
                                }
                                return;
                            }
                            findResource(session);
                        } else if (m_onError) {
                            m_onError(session->m_request, ec);
                        }
                    });
                } else {
                    findResource(session);
                }
            } else if ((headerIt = session->m_request->m_header.find("Transfer-Encoding")) != session->m_request->m_header.end() &&
                    headerIt->second == "chunked") {
                auto chunksStreambuf = std::make_shared<asio::streambuf>(m_config.m_maxRequestStreambufSize);
                read_cunked_transfer_encoded(session, chunksStreambuf);
            } else {
                findResource(session);
            }
        } else if (m_onError) {
            m_onError(session->m_request, ec);
        }
    });
}
