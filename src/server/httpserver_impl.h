
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
