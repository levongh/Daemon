#pragma once

#include <limits>
#include <mutex>
#include "../util/utility.h"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

namespace client {

namespace asio = boost::asio;
using error_code = boost::system::error_code;
namespace errc = boost::system::errc;
using system_error = boost::system::system_error;
namespace make_error_code = boost::system::errc;
using string_view = const std::string&;


#include "content.h"
#include "response.h"
#include "config.h"

template<typename SocketType>
class Client;

template<typename SocketType>
class ClientBase
{
};

} // unnamed client
