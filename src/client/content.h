#pragma once

namespace client{

template<typename SocketType>
class ClientBase;

template<typename SocketType>
class Content : public std::istream
{
    friend class ClientBase<SocketType>;

public:
    size_t size() noexcept
    {
        return m_streambuf.size();
    }

    ///@brief Convinience function to return std::string. The stream buffer is consumed
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

} // namespace client
