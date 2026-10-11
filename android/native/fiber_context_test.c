#include "fiber_context.h"
#include <fenv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static void *main_fiber;
static uint32_t passes;
static void entry(void *opaque)
{
    int mode = (int)(intptr_t)opaque;
    volatile uint64_t local = UINT64_C(0x123456789abcdef0);
    if (fesetround(mode)) abort();
    for (;;) {
        if (local != UINT64_C(0x123456789abcdef0) || fegetround() != mode) abort();
        ++passes;
        SwitchToFiber(main_fiber);
    }
}
int main(void)
{
    main_fiber = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
    void *down = CreateFiberEx(0, 1048576, FIBER_FLAG_FLOAT_SWITCH, entry, (void *)(intptr_t)FE_DOWNWARD);
    void *up = CreateFiberEx(0, 1048576, FIBER_FLAG_FLOAT_SWITCH, entry, (void *)(intptr_t)FE_UPWARD);
    if (!main_fiber || !down || !up || fesetround(FE_TONEAREST)) return 1;
    for (unsigned i = 0; i < 10000; ++i) {
        SwitchToFiber(down); SwitchToFiber(up);
        if (fegetround() != FE_TONEAREST) return 2;
    }
    DeleteFiber(down); DeleteFiber(up);
    if (passes != 20000 || !ConvertFiberToThread()) return 3;
    puts("ARM64 fibers: 20000 stack switches with independent rounding modes passed.");
    return 0;
}
