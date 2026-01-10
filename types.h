#pragma once

// Should always inline no matter what.
#ifndef _ALWAYS_INLINE_
#if defined(__GNUC__)
#define _ALWAYS_INLINE_ __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define _ALWAYS_INLINE_ __forceinline
#else
#define _ALWAYS_INLINE_ inline
#endif
#endif

// Should always inline, except in dev builds because it makes debugging harder.
#ifndef _FORCE_INLINE_
#ifdef DEV_ENABLED
#define _FORCE_INLINE_ inline
#else
#define _FORCE_INLINE_ _ALWAYS_INLINE_
#endif
#endif

//https://www.gnu.org/software/gnulib/manual/html_node/Attributes.html
//https://learn.microsoft.com/en-us/cpp/code-quality/annotating-function-behavior?view=msvc-170
#ifndef _NO_DISCARD_
#if defined(__GNUC__)
#define _NO_DISCARD_ __attribute__((__warn_unused_result__))
#elif defined(_MSC_VER)
#define _NO_DISCARD_ _Check_return_
#else
#define _NO_DISCARD_ [[nodiscard]]
#endif
#endif

typedef float                   f32;
typedef double                  f64;
typedef long double             f128;
typedef unsigned char           u8;
typedef unsigned short int      u16;
typedef unsigned int            u32;
typedef unsigned long long int  u64;
typedef char                    i8;
typedef signed short int        i16;
typedef int                     i32;
typedef long long int           i64;

#ifdef __cplusplus
typedef bool b8;
#define TRUE    true
#define FALSE   false
#else
typedef unsigned char b8;
#define TRUE    ((b8)0x1)
#define FALSE   ((b8)0x0)
#endif
