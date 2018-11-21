
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
auto ServerBase<SocketType>::createConnection(Args&&... args) noexcept -> ConnectionPtr
{
    auto connections = m_connections;
    auto connectionsMutex = m_connectionsMutex;
    auto connection = ConnectionPtr(
            new Connection<SocketType> (m_handlerRunner, std::forward<Args>(args) ...),
            [connections, connectionsMutex] (Connection<SocketType>* connection) {
            std::unique_lock<std::mutex> lock(*connectionsMutex);
            auto it = connections->find(connection);
            if (it != connections->end()) {
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
void ServerBase<SocketType>::read(const SessionPtr& session)
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
                    session->m_request->m_streambuf.size() == session->m_request->m_streambuf.max_size()) {
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
                            if (session->m_request->m_streambuf.size() == session->m_request->m_streambuf.max_size()) {
                                auto response = ResponsePtr(new Response<SocketType>(session, m_config.m_timeoutContent));
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
                readChunkedTransferEncoded(session, chunksStreambuf);
            } else {
                findResource(session);
            }
        } else if (m_onError) {
            m_onError(session->m_request, ec);
        }
    });
}

template <typename SocketType>
void ServerBase<SocketType>::readChunkedTransferEncoded(const SessionPtr& session,
                                                        const std::shared_ptr<asio::streambuf> &chunksStreambuf)
{
    session->m_connection->set_timeout(m_config.m_timeoutContent);
    asio::async_read_until(*session->m_connection->m_socket,
                            session->m_request->m_streambuf,
                            "\r\n", [this, session, chunksStreambuf] (const error_code &ec, size_t bytesTransferred) {
        session->m_connection->cancel_timeout();
        auto lock = session->m_connection->m_handlerRunner->continue_lock();
        if(!lock) {
            return;
        }
        if((!ec || ec == asio::error::not_found) &&
                session->m_request->m_streambuf.size() == session->m_request->m_streambuf.max_size()) {
            auto response = ResponsePtr(new Response<SocketType>(session, m_config.m_timeoutContent));
            response->write(StatusCode::client_error_payload_too_large);
            response->send();
            if(m_onError) {
                m_onError(session->m_request, make_error_code::make_error_code(errc::message_size));
            }
            return;
        }
        if(!ec) {
            std::string line;
            getline(session->m_request->m_content, line);
            bytesTransferred -= line.size() + 1;
            line.pop_back();
            unsigned long length = 0;
            try {
                length = stoul(line, 0, 16);
            } catch(...) {
                if(m_onError) {
                    m_onError(session->m_request, make_error_code::make_error_code(errc::protocol_error));
                }
                return;
            }

            auto numAdditionalBytes = session->m_request->m_streambuf.size() - bytesTransferred;
            if((2 + length) > numAdditionalBytes) {
                session->m_connection->set_timeout(m_config.m_timeoutContent);
                asio::async_read(*session->m_connection->m_socket,
                                  session->m_request->m_streambuf,
                                  asio::transfer_exactly(2 + length - numAdditionalBytes),
                                  [this, session, chunksStreambuf, length](const error_code &ec, size_t) {
                    session->m_connection->cancel_timeout();
                    auto lock = session->m_connection->m_handlerRunner->continue_lock();
                    if(!lock) {
                        return;
                    }
                    if(!ec) {
                        if(session->m_request->m_streambuf.size() == session->m_request->m_streambuf.max_size()) {
                            auto response = ResponsePtr(new Response<SocketType>(session, m_config.m_timeoutContent));
                            response->write(StatusCode::client_error_payload_too_large);
                            response->send();
                        if(m_onError) {
                           m_onError(session->m_request, make_error_code::make_error_code(errc::message_size));
                        }
                        return;
                        }
                        readChunkedTransferEncodedChunk(session, chunksStreambuf, length);
                    }  else if(m_onError) {
                        m_onError(session->m_request, ec);
                    }
                });
            } else {
                readChunkedTransferEncodedChunk(session, chunksStreambuf, length);
            }
        } else if(m_onError) {
            m_onError(session->m_request, ec);
        }
    });
}

template <typename SocketType>
void ServerBase<SocketType>::readChunkedTransferEncodedChunk(const SessionPtr& session,
                                                             const std::shared_ptr<asio::streambuf> &chunksStreambuf,
                                                             unsigned long length)
{
    std::ostream tmpStream(chunksStreambuf.get());
    if(length > 0) {
        std::unique_ptr<char[]> buffer(new char[length]);
        session->m_request->m_content.read(buffer.get(), static_cast<std::streamsize>(length));
        tmpStream.write(buffer.get(), static_cast<std::streamsize>(length));
        if(chunksStreambuf->size() == chunksStreambuf->max_size()) {
            auto response = ResponsePtr(new Response<SocketType>(session, m_config.m_timeoutContent));
            response->write(StatusCode::client_error_payload_too_large);
            response->send();
            if(m_onError) {
                m_onError(session->m_request, make_error_code::make_error_code(errc::message_size));
            }
            return;
        }
    }

    ///@brief Remove "\r\n"
    session->m_request->m_content.get();
    session->m_request->m_content.get();

    if(length > 0) {
        readChunkedTransferEncoded(session, chunksStreambuf);
    } else {
        if(chunksStreambuf->size() > 0) {
            std::ostream ostream(&session->m_request->m_streambuf);
            ostream << chunksStreambuf.get();
        }
        findResource(session);
    }
}

template <typename SocketType>
void ServerBase<SocketType>::findResource(const SessionPtr& session)
{
    // Upgrade connection
    if(m_onUpgrade) {
        auto it = session->m_request->m_header.find("Upgrade");
        if(it != session->m_request->m_header.end()) {
            // remove connection from connections
            {
                std::unique_lock<std::mutex> lock(*m_connectionsMutex);
                auto it = m_connections->find(session->m_connection.get());
                if(it != m_connections->end()) {
                    m_connections->erase(it);
                }
            }
            m_onUpgrade(session->m_connection->m_socket, session->m_request);
            return;
        }
    }
    // Find path- and method-match, and call write
    for(auto &regexMethod : m_resource) {
        auto it = regexMethod.second.find(session->m_request->m_method);
        if(it != regexMethod.second.end()) {
            regex::smatch smRes;
            if(regex::regex_match(session->m_request->m_path, smRes, regexMethod.first)) {
                session->m_request->m_pathMatch = std::move(smRes);
                write(session, it->second);
                return;
            }
        }
    }
    auto it = m_defaultResource.find(session->m_request->m_method);
    if(it != m_defaultResource.end()) {
        write(session, it->second);
    }
}

template <typename SocketType>
void ServerBase<SocketType>::write(const SessionPtr& session,
           Callback &resourceFunction)
{
    session->m_connection->set_timeout(m_config.m_timeoutContent);
    auto response = ResponsePtr(new Response<SocketType>(session, m_config.m_timeoutContent),
        [this](Response<SocketType>* response_ptr) {
        auto response = ResponsePtr(response_ptr);
        response->send([this, response](const error_code &ec) {
            if(!ec) {
                if(response->m_closeConnectionAfterResponse) {
                    return;
                }

                auto range = response->m_session->m_request->m_header.equal_range("Connection");
                for(auto it = range.first; it != range.second; it++) {
                    if(CaseInsensitiveEqual::caseInsensitiveEqual(it->second, "close")) {
                        return;
                    } else if(CaseInsensitiveEqual::caseInsensitiveEqual(it->second, "keep-alive")) {
                        auto newSession = std::make_shared<Session<SocketType> >(m_config.m_maxRequestStreambufSize, response->m_session->m_connection);
                        read(newSession);
                        return;
                    }
                }
                if(response->m_session->m_request->m_httpVersion >= "1.1") {
                    auto newSession = std::make_shared<Session<SocketType> >(m_config.m_maxRequestStreambufSize, response->m_session->m_connection);
                    this->read(newSession);
                    return;
                }
            } else if(m_onError) {
                m_onError(response->m_session->m_request, ec);
            }
        });
    });

    try {
        resourceFunction(response, session->m_request);
    } catch(const std::exception &) {
        if(m_onError) {
            m_onError(session->m_request, make_error_code::make_error_code(errc::operation_canceled));
        }
        return;
    }
}

