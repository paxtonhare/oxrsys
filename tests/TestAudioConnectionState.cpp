// SPDX-License-Identifier: MPL-2.0
#include <catch2/catch_test_macros.hpp>
#include "AudioConnectionState.h"
#include "StreamingAudioQueue.h"
#include <fcntl.h>
#include <unistd.h>

TEST_CASE("Audio multipart lease prevents descriptor reuse during replacement", "[audio][lifecycle]")
{
    int oldPair[2], newPair[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, oldPair) == 0);
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, newPair) == 0);
    AudioConnectionState state;
    state.Replace(oldPair[0], [] {});
    auto oldWrite = state.Acquire();
    const int descriptor = oldWrite.Descriptor();
    REQUIRE(send(descriptor, "header", 6, 0) == 6);

    // Deterministically suspend a multipart write between its header and payload.
    // Replacement must neither wait for the writer nor close its descriptor.
    bool negotiated = false;
    state.Replace(newPair[0], [&] { negotiated = true; });
    CHECK_FALSE(state.WithCurrent(oldWrite, [&] { negotiated = false; }));
    CHECK(negotiated);
    REQUIRE(fcntl(descriptor, F_GETFD) != -1);
    REQUIRE(send(oldWrite.Descriptor(), "payload", 7, 0) == 7);
    char bytes[13] = {};
    REQUIRE(recv(oldPair[1], bytes, sizeof(bytes), MSG_WAITALL) == sizeof(bytes));
    CHECK(std::string(bytes, sizeof(bytes)) == "headerpayload");
    CHECK(recv(newPair[1], bytes, sizeof(bytes), MSG_DONTWAIT) == -1);

    oldWrite = {};
    CHECK(fcntl(descriptor, F_GETFD) == -1);
    // Force the exact old numeric descriptor to be reused after its lease ends.
    REQUIRE(dup2(newPair[0], descriptor) == descriptor);
    REQUIRE(send(descriptor, "new", 3, 0) == 3);
    REQUIRE(recv(newPair[1], bytes, 3, MSG_WAITALL) == 3);
    CHECK(std::string(bytes, 3) == "new");
    close(descriptor);
    close(oldPair[1]);
    close(newPair[1]);
}

TEST_CASE("Old audio failure and capture cannot mutate a new negotiation", "[audio][lifecycle]")
{
    int sockets[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    AudioConnectionState state;
    StreamingAudioQueue queue(8);
    queue.Start();
    bool negotiated = true;
    state.Replace(sockets[0], [] {});
    auto oldWrite = state.Acquire();
    // A negotiation can also restart on the same descriptor, without replacement.
    state.ResetGeneration([&] { negotiated = false; queue.Clear(); });
    const auto current = state.Acquire();
    REQUIRE(state.WithCurrent(current, [&] {
        negotiated = true;
        StreamingAudioFrame frame;
        frame.generation = current.generation;
        queue.Push(std::move(frame));
    }));
    CHECK_FALSE(state.WithCurrent(oldWrite, [&] { negotiated = false; queue.Clear(); }));
    CHECK_FALSE(state.WithCurrent(oldWrite, [&] { queue.Push({}); }));
    CHECK(negotiated);
    CHECK(queue.Size() == 1);
    std::atomic<bool> running{true};
    StreamingAudioFrame frame;
    REQUIRE(queue.WaitPop(running, frame));
    CHECK(frame.generation == current.generation);
    close(sockets[1]);
}
