// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "RuntimeSockets.h"
#include <memory>
#include <mutex>
#include <cstdint>

// Audio-only synchronization. A lease keeps the descriptor open for an entire
// multipart write; replacing a connection never waits for the sender.
class AudioConnectionState
{
    struct Socket
    {
        oxrsys::runtime_socket::SocketHandle value = oxrsys::runtime_socket::InvalidSocket;
        ~Socket() { oxrsys::runtime_socket::Close(value); }
    };
public:
    struct Lease
    {
        std::shared_ptr<Socket> socket;
        uint64_t generation = 0;
        explicit operator bool() const { return bool(socket); }
        auto Descriptor() const { return socket->value; }
    };
    Lease Acquire() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_;
    }
    template<class Reset> void Replace(oxrsys::runtime_socket::SocketHandle socket, Reset reset)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++current_.generation;
        current_.socket.reset();
        if (oxrsys::runtime_socket::IsValid(socket))
        {
            current_.socket = std::make_shared<Socket>();
            current_.socket->value = socket;
        }
        reset();
    }
    template<class Reset> void ResetGeneration(Reset reset)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++current_.generation;
        reset();
    }
    template<class Action> bool WithCurrent(const Lease& lease, Action action)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!lease || lease.generation != current_.generation || lease.socket != current_.socket)
            return false;
        action();
        return true;
    }
private:
    mutable std::mutex mutex_;
    Lease current_;
};
