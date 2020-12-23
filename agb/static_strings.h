#ifndef STATIC_STRINGS_H
#define STATIC_STRINGS_H

#include <stdbool.h>
#include <stdint.h>

/* Given a C string at VMA, map it to a menu strtab index */
struct static_str {
    uint32_t vma;
    uint16_t idx;
};

uint16_t static_str_map(const char* vma);

#endif
