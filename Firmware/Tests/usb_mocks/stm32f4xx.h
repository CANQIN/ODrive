#pragma once
#include <stdint.h>
#define __IO volatile
#define UNUSED(x) ((void)(x))
#define __STATIC_INLINE static inline
#ifdef _MSC_VER
#define __PACKED
#else
#define __PACKED __attribute__((packed))
#endif
