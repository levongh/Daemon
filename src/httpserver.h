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
class server;




} //namespace server
