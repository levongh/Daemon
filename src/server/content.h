#pragma once

namespace server {

template <typename SocketType>
class ServerBase;

template <typename SocketType>
class Request;

template <typename SocketType>
class Content : public std::istream
{
    friend class ServerBase<SocketType>;
    friend class Request<SocketType>;

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
    Content(asio::streambuf& streambuf) noexcept
        : std::istream(&streambuf)
        , m_streambuf(streambuf)
    {}

private:
    asio::streambuf& m_streambuf;

};

} // namespace server

