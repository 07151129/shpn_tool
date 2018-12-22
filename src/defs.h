#ifndef DEFS_H
#define DEFS_H

#include <assert.h>
#include <stdint.h>

#define ROM_BASE 0x8000000ul
#define VMA2OFFS(vma) (assert((vma) >= ROM_BASE && (vma) - ROM_BASE <= 0x1ffffff), (vma) - ROM_BASE)
#define OFFS2VMA(offs) (assert((offs) <= 0x1ffffff), (offs) + ROM_BASE)

/* TODO */
#if 0
struct rom_desc {

};
#endif

#endif
