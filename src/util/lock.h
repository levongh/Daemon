#pragma once

#include <memory>
#include <atomic>

#ifdef __SSE2__
#include <emmintrin.h>

namespace server {

inline void spinLoopPause() noexcept
{
    _mm_pause();
}

} //namespace server

#elif defined(_MSC_VER) && _MSC_VER >= 1800 && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>

namespace server {

inline void spinLoopPause() noexcept
{
    _mm_pause();
}

} //namespace server

#else

namespace server {

inline void spinLoopPause() noexcept
{
}

} //namespace server

#endif

namespace server {

class SharedLock
{
private:
    friend class ScopeRunner;
    std::atomic<long>& m_count;;

private:
    SharedLock(std::atomic<long>& count) noexcept
        : m_count(count)
        {}
    SharedLock(const SharedLock& other) = delete;
    SharedLock& operator=(const SharedLock& other) = delete;

public:
    ~SharedLock() noexcept
    {
        m_count.fetch_sub(1);
    }
};

///@brief Makes it possible to for instance cancel Asio handlers without stopping asio::io_service
class ScopeRunner
{
///@brief Scope count that is set to -1 if scopes are to be canceled
private:
    std::atomic<long> m_count;

public:
    ScopeRunner() noexcept
        : m_count(0)
    {
    }

    ///@brief Returns nullptr if scope should be exited, or a shared lock otherwise
    std::unique_ptr<SharedLock> continue_lock() noexcept
    {
        long expected = m_count;
        while (expected >= 0 && !m_count.compare_exchange_weak(expected, expected + 1)) {
            spinLoopPause();
        }
        if (expected < 0) {
            return nullptr;
        } else {
            return std::unique_ptr<SharedLock>(new SharedLock(m_count));
        }
    }

    ///@brief Blocks until all shared locks are released, then prevents future shared locks
    void stop() noexcept
    {
        long expected = 0;
        while (!m_count.compare_exchange_weak(expected, -1)) {
            if (expected < 0) {
                return;
            }
            expected = 0;
            spinLoopPause();
        }
    }
};

} //namespace server
