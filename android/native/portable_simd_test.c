#define RECOMP_FORCE_PORTABLE_SIMD
#include "recomp_types.h"
#include <fenv.h>
#include <stdio.h>

int main(void)
{
    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const uint64_t expected[] = {
        UINT64_C(0xfffffffe00000002), UINT64_C(0xfffffffd00000002),
        UINT64_C(0xfffffffe00000003), UINT64_C(0xfffffffe00000002)
    };
    for (unsigned i = 0; i < 4; ++i) {
        if (fesetround(modes[i]) || MMX_CVTPS2PI(2.5f, -2.5f) != expected[i]) return 1;
    }
    if (fesetround(FE_TONEAREST)) return 2;
    if (MMX_CVTPS2PI(INFINITY, NAN) != UINT64_C(0x8000000080000000)) return 3;
    if (MMX_CVTPS2PI(2147483648.0f, -2147483648.0f) != UINT64_C(0x8000000080000000)) return 4;
    if (MMX_CVTPS2PI(2147483520.0f, -2147483520.0f) != UINT64_C(0x800000807fffff80)) return 5;
    RecompXmm a, b;
    a.u[0] = 0x7fc00013u; a.u[1] = 0x80000000u; a.u[2] = 0x7f800000u; a.u[3] = 0u;
    b.u[0] = 0xffc01024u; b.u[1] = 1u; b.u[2] = 0x12345678u; b.u[3] = 0xffffffffu;
    for (unsigned selector = 0; selector < 256; ++selector) {
        RecompXmm r = XMM_SHUFFLE(a, b, selector);
        if (r.u[0] != a.u[selector & 3] || r.u[1] != a.u[(selector >> 2) & 3] ||
            r.u[2] != b.u[(selector >> 4) & 3] || r.u[3] != b.u[selector >> 6]) return 6;
    }
    puts("ARM portability: lane bits, rounding modes, NaN, infinity and integer limits passed.");
    return 0;
}
