#ifndef DEFS_H
#define DEFS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define ROM_BASE 0x8000000ul
#define VMA2OFFS(vma) (assert((vma) >= ROM_BASE && (vma) - ROM_BASE <= 0x1ffffff), (vma) - ROM_BASE)
#define OFFS2VMA(offs) (assert((offs) <= 0x1ffffff), (offs) + ROM_BASE)

#ifndef HAS_ICONV
typedef int iconv_t;
#else
#include <iconv.h>
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(format)
#define FMT_PRINTF(fmt, va) __attribute__((format(printf, fmt, va)))
#else
#define FMT_PRINTF(fmt, va)
#endif

#endif
