#include "d3d_vblank.h"
#include <algorithm>
#include <chrono>
#include <thread>
namespace { using Clock=std::chrono::steady_clock; Clock::time_point next; }
void recomp_d3d_vblank_reset(void) { next=Clock::now(); }
void recomp_d3d_wait_vblank(void)
{
    next=std::max(next+std::chrono::nanoseconds(1000000000/60),Clock::now());
    std::this_thread::sleep_until(next);
}
