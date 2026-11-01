#include "third_party/doctest/doctest.h"

#include "testutil.h"
#include "RedEngineFL/Application/pc/pcFrameLimiter.h"

using namespace FrameLimiter;

namespace
{

struct FakeClock
{
    __int64 iNow;
    __int64 iOvershoot;
    __int64 iStep;
    int iWaits;
    __int64 iFirstWait;
};

__int64 FakeNow(void* p)
{
    FakeClock* c = (FakeClock*)p;
    __int64 r = c->iNow;
    c->iNow += c->iStep;
    return r;
}

void FakeWait(void* p, __int64 iTicks)
{
    FakeClock* c = (FakeClock*)p;
    if (c->iWaits == 0)
    {
        c->iFirstWait = iTicks;
    }
    c->iWaits++;
    // a wait that wakes early still moves the clock on
    const __int64 iAdvance = iTicks + c->iOvershoot;
    c->iNow += iAdvance > 0 ? iAdvance : 1;
}

FrameOps Ops(FakeClock& c)
{
    FrameOps o;
    o.Now = FakeNow;
    o.WaitTicks = FakeWait;
    o.pCtx = &c;
    return o;
}

FakeClock Clock(__int64 iNow, __int64 iStep)
{
    FakeClock c;
    c.iNow = iNow;
    c.iOvershoot = 0;
    c.iStep = iStep;
    c.iWaits = 0;
    c.iFirstWait = 0;
    return c;
}

FramePlan Plan(__int64 iMin, __int64 iMargin)
{
    FramePlan p;
    p.iMinTicks = iMin;
    p.iMarginTicks = iMargin;
    p.iSlackTicks = 0;
    return p;
}

}

TEST_CASE("frame limiter 80 fps period matches the retail divisor" * doctest::test_suite("unit"))
{
    CHECK(MinTicksForFps(10000000, 80) == 125000);
    CHECK(MinTicksForFps(10000000, 80) == 10000000 / 80);
    CHECK(MinTicksForFps(10000000, 0) == 1);
    CHECK(MinTicksForFps(10000000, 1000) == 10000);
    CHECK(MinTicksForFps(10, 80) == 1);
}

TEST_CASE("frame limiter uncapped never returns zero ticks" * doctest::test_suite("unit"))
{
    FakeClock c = Clock(0, 1);
    __int64 iOut = 0;
    __int64 iDelta = Pace(Ops(c), Plan(0, 0), 0, iOut);
    CHECK(iDelta >= 1);
    CHECK(iOut == iDelta);
}

TEST_CASE("frame limiter does not wait under the margin" * doctest::test_suite("unit"))
{
    FakeClock c = Clock(120000, 1);
    __int64 iOut = 0;
    __int64 iDelta = Pace(Ops(c), Plan(125000, 10000), 0, iOut);
    CHECK(c.iWaits == 0);
    CHECK(iDelta >= 125000);
}

TEST_CASE("frame limiter waits then spins above the margin" * doctest::test_suite("unit"))
{
    FakeClock c = Clock(0, 1);
    __int64 iOut = 0;
    __int64 iDelta = Pace(Ops(c), Plan(125000, 10000), 0, iOut);
    CHECK(c.iWaits == 1);
    CHECK(c.iFirstWait == 115000);
    CHECK(iDelta >= 125000);
}

TEST_CASE("frame limiter backward clock jump completes the frame at once" * doctest::test_suite("unit"))
{
    FakeClock c = Clock(-1250000, 1);
    __int64 iOut = 0;
    __int64 iDelta = Pace(Ops(c), Plan(125000, 10000), 0, iOut);
    CHECK(c.iWaits == 0);
    CHECK(iDelta == 125000);
    CHECK(iOut == -1250000);
}

TEST_CASE("frame limiter forward clock jump returns at once" * doctest::test_suite("unit"))
{
    FakeClock c = Clock(5000000, 1);
    __int64 iOut = 0;
    __int64 iDelta = Pace(Ops(c), Plan(125000, 10000), 0, iOut);
    CHECK(c.iWaits == 0);
    CHECK(iDelta == 5000000);
}

TEST_CASE("frame limiter without a timer sleeps coarsely only below 50 fps" * doctest::test_suite("unit"))
{
    // A rendered run whose timers all failed
    const FramePlan k80 = PlanFor(kTimerNone, false, 10000000, 80);
    const FramePlan k30 = PlanFor(kTimerNone, false, 10000000, 30);
    CHECK(k80.iSlackTicks == 0);
    CHECK(k80.iMinTicks <= k80.iMarginTicks);
    CHECK(k30.iMinTicks > k30.iMarginTicks);
    FakeClock c80 = Clock(0, 1);
    __int64 iOut = 0;
    Pace(Ops(c80), k80, 0, iOut);
    CHECK(c80.iWaits == 0);
    FakeClock c30 = Clock(0, 1);
    __int64 iDelta = Pace(Ops(c30), k30, 0, iOut);
    CHECK(c30.iWaits == 1);
    CHECK(c30.iFirstWait == k30.iMinTicks - k30.iMarginTicks);
    CHECK(iDelta >= k30.iMinTicks);
}

TEST_CASE("frame limiter headless sleeps instead of spinning" * doctest::test_suite("unit"))
{
    REQUIRE(Init(true) == kTimerNone);
    FrameOps kOps;
    FramePlan kReal;
    BindReal(kOps, kReal, 120);
    Shutdown();
    CHECK(kReal.iMarginTicks == 0);
    CHECK(kReal.iSlackTicks > 0);
    const FramePlan k120 = PlanFor(kTimerNone, true, 10000000, 120);
    CHECK(k120.iMarginTicks == 0);
    CHECK(k120.iSlackTicks == 10000);
    // The frame is one sleep, and a sleep that ends inside the last millisecond ends the frame
    FakeClock c = Clock(0, 1);
    c.iOvershoot = -5000;
    __int64 iOut = 0;
    __int64 iDelta = Pace(Ops(c), k120, 0, iOut);
    CHECK(c.iWaits == 1);
    CHECK(c.iFirstWait == k120.iMinTicks);
    CHECK(iDelta >= k120.iMinTicks - k120.iSlackTicks);
    // A rendered run keeps its exact pacing
    const FramePlan kHires = PlanFor(kTimerHires, false, 10000000, 120);
    CHECK(kHires.iSlackTicks == 0);
    CHECK(kHires.iMarginTicks == 10000);
}
