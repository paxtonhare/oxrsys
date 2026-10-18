// SPDX-License-Identifier: MPL-2.0

#include <catch2/catch_test_macros.hpp>

#include "StreamingAudioQueue.h"

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{

StreamingAudioFrame MakeAudioFrame(uint32_t frameIndex, float sample)
{
    StreamingAudioFrame frame = {};
    frame.frameIndex = frameIndex;
    frame.presentationTimeNs = static_cast<int64_t>(frameIndex) * 10'000'000;
    frame.frameCount = 1;
    frame.samples = {sample, sample};
    return frame;
}

} // namespace

TEST_CASE("StreamingAudioQueue remains bounded and preserves the newest audio order",
          "[streaming][audio]")
{
    StreamingAudioQueue queue(2);
    queue.Start();

    CHECK(!queue.Push(MakeAudioFrame(1, 0.1f)));
    CHECK(!queue.Push(MakeAudioFrame(2, 0.2f)));
    CHECK(queue.Push(MakeAudioFrame(3, 0.3f)));
    CHECK(queue.Size() == 2);

    std::atomic<bool> running{true};
    StreamingAudioFrame popped = {};
    REQUIRE(queue.WaitPop(running, popped));
    CHECK(popped.frameIndex == 2);
    CHECK(popped.presentationTimeNs == 20'000'000);
    REQUIRE(popped.samples.size() == 2);
    CHECK(popped.samples[0] == 0.2f);

    REQUIRE(queue.WaitPop(running, popped));
    CHECK(popped.frameIndex == 3);
    CHECK(queue.Size() == 0);

    queue.Stop();
    CHECK(!queue.WaitPop(running, popped));
}
