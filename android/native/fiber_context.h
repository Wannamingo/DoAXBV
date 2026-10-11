#ifndef DOAXBV_ANDROID_FIBER_CONTEXT_H
#define DOAXBV_ANDROID_FIBER_CONTEXT_H
#include <stddef.h>
typedef void *LPVOID;
#define VOID void
#define WINAPI
#define FIBER_FLAG_FLOAT_SWITCH 1
void *ConvertThreadToFiberEx(void *parameter, unsigned flags);
int ConvertFiberToThread(void);
void *CreateFiberEx(size_t committed, size_t reserved, unsigned flags,
                    void (*entry)(void *), void *parameter);
void SwitchToFiber(void *fiber);
void DeleteFiber(void *fiber);
#endif
