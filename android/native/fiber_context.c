#include "fiber_context.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct Arm64Context {
    uint64_t callee[12]; /* x19..x30 */
    uint64_t sp;
    uint64_t simd[8]; /* ABI-preserved low 64 bits of v8..v15 */
    uint32_t fpcr, fpsr;
} Arm64Context;
_Static_assert(offsetof(Arm64Context, sp) == 96, "context assembly layout");
_Static_assert(offsetof(Arm64Context, simd) == 104, "context assembly layout");
_Static_assert(offsetof(Arm64Context, fpcr) == 168, "context assembly layout");

typedef struct HostFiber {
    Arm64Context context;
    void *stack;
    size_t mapping_size;
    void (*entry)(void *);
    void *parameter;
    int main;
} HostFiber;
static _Thread_local HostFiber *current;
extern void recomp_arm64_fiber_switch(Arm64Context *, const Arm64Context *);
extern void recomp_arm64_fiber_start(void);

void recomp_host_fiber_entry(HostFiber *fiber)
{
    if (fiber != current || !fiber->entry) abort();
    fiber->entry(fiber->parameter);
    abort();
}

void *ConvertThreadToFiberEx(void *parameter, unsigned flags)
{
    (void)flags;
    if (current) return NULL;
    current = calloc(1, sizeof *current);
    if (current) { current->main = 1; current->parameter = parameter; }
    return current;
}

int ConvertFiberToThread(void)
{
    if (!current || !current->main) return 0;
    free(current);
    current = NULL;
    return 1;
}

void *CreateFiberEx(size_t committed, size_t reserved, unsigned flags,
                    void (*entry)(void *), void *parameter)
{
    (void)committed; (void)flags;
    if (!entry) return NULL;
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    size_t bytes = reserved < (1u << 20) ? (1u << 20) : reserved;
    if (bytes > SIZE_MAX - 2 * page) return NULL;
    bytes = (bytes + page - 1) / page * page;
    HostFiber *fiber = calloc(1, sizeof *fiber);
    if (!fiber) return NULL;
    fiber->mapping_size = bytes + 2 * page;
    fiber->stack = mmap(NULL, fiber->mapping_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fiber->stack == MAP_FAILED || mprotect((char *)fiber->stack + page, bytes, PROT_READ | PROT_WRITE)) {
        if (fiber->stack != MAP_FAILED) munmap(fiber->stack, fiber->mapping_size);
        free(fiber); return NULL;
    }
    fiber->entry = entry;
    fiber->parameter = parameter;
    fiber->context.sp = ((uintptr_t)fiber->stack + page + bytes) & ~(uintptr_t)15;
    fiber->context.callee[0] = (uintptr_t)fiber;
    fiber->context.callee[11] = (uintptr_t)recomp_arm64_fiber_start;
    __asm__ volatile("mrs %x0, fpcr" : "=r"(fiber->context.fpcr));
    __asm__ volatile("mrs %x0, fpsr" : "=r"(fiber->context.fpsr));
    return fiber;
}

void SwitchToFiber(void *opaque)
{
    HostFiber *next = opaque, *previous = current;
    if (!next || !previous) abort();
    if (next == previous) return;
    current = next;
    recomp_arm64_fiber_switch(&previous->context, &next->context);
    if (current != previous) abort();
}

void DeleteFiber(void *opaque)
{
    HostFiber *fiber = opaque;
    if (!fiber) return;
    if (fiber == current || fiber->main) abort();
    munmap(fiber->stack, fiber->mapping_size);
    free(fiber);
}
