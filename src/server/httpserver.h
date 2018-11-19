#pragma once

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

#include "../util/utility.h"

///@brief Server includes
//@{
#include "config.h"
#include "content.h"
#include "session.h"
#include "request.h"
#include "response.h"
#include "connection.h"
#include "regexorderable.h"
//@}

namespace server {

template <typename SocketType>
class ServerBase
{
public:
    /// If you know the server port in advance, use start() instead.
    /// Returns assigned port. If io_service is not set, an internal io_service is created instead.
    /// Call before acceptAndRun().
    unsigned short bind();

    /// If you know the server port in advance, use start() instead.
    /// Accept requests, and if io_service was not set before calling bind(), run the internal io_service instead.
    /// Call after bind().
    void acceptAndRun();

    ///@brief Start the server by calling bind and acceptAndRun()
    void start();

    ///@brief Stop accepting new requests, and close current connections.
    void stop() noexcept;

    virtual ~ServerBase() noexcept;

protected:
    ServerBase(unsigned short port) noexcept;

    virtual void afterBind()
    {}
    virtual void accept() = 0;

    template <typename... Args>
    std::shared_ptr<Connection<SocketType> > createConnection(Args&&... args) noexcept;

public:
    ///@brief Set before calling start().
    Config<SocketType> m_config;

public:
    ///@brief Warning: do not add or remove resources after start() is called
    std::map<RegexOrderable, std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<SocketType>::Response>, std::shared_ptr<typename ServerBase<SocketType>::Request>)> > > m_resource;

    std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<SocketType>::Response>, std::shared_ptr<typename ServerBase<SocketType>::Request>)> > m_defaultResource;

    std::function<void(std::shared_ptr<typename ServerBase<SocketType>::Request>, const error_code &)> m_onError;

    std::function<void(std::unique_ptr<SocketType> &, std::shared_ptr<typename ServerBase<SocketType>::Request>)> m_onUpgrade;

    /// If you have your own asio::io_service, store its pointer here before running start().
    std::shared_ptr<asio::io_service> m_ioService;

protected:
    bool m_internalIoService = false;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::vector<std::thread> m_threads;
    std::shared_ptr<std::unordered_set<Connection<SocketType>* > > m_connections;
    std::shared_ptr<std::mutex> m_connectionsMutex;
    std::shared_ptr<ScopeRunner> m_handlerRunner;


};

#include "httpserver_impl.h"

} //namespace server
