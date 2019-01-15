#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "defs.h"
#include "script_disass.h"
#include "strtab.h"

static void usage() {
    fprintf(stderr, "shpn-tool: analyze and modify Silent Hill Play Novel GBA ROM\n"
#ifdef HAS_ICONV
                    "Built with iconv support"
#endif
                    "\nusage: <ROM> <verb> [...]\n\n"
                    "ROM is the AGB-ASHJ ROM path\n"
                    "Supported verbs:\n"
                    "script <name> <dump | embed>\n"
                    "\tdump [out] -- Dump script to file at \"out\" or to stdout\n"
                    "\tembed in [out] -- Embed script at \"in\" into ROM at \"out\" or into \"ROM\""
                    "\n\n"
                    "strtab <name> <dump | embed>\n"
                    "\tdump [idx] [out] -- Dump strtab entry at \"idx\" or all entries to file at"
                        " \"out\" or stdout.\n"
                    "\tembed [idx] [in] -- Embed strtab entry at \"idx\" or all entries from file"
                        " \"in\" or stdin."
                    "\n\n");
}

static struct {
    enum {VERB_NOP, VERB_SCRIPT, VERB_STRTAB} verb;
    union {
        enum {SCRIPT_DUMP, SCRIPT_EMBED} script_verb;
        enum {STRTAB_DUMP, STRTAB_EMBED} strtab_verb;
    };
    char* rom_path;
    char* in_path, * out_path;
    union {
        char* script_name;
        char* strtab_name;
    };
    uint32_t strtab_idx;
    bool has_strtab_idx;
} opts;

static bool parse_script_verb(int argc, char* const* argv, int i) {
    int j = i + 1;

    if (j < argc) {
        opts.script_name = argv[j];
    } else {
        fprintf(stderr, "Missing script name\n");
        return false;
    }

    if (++j < argc) {
        if (!strcmp(argv[j], "dump"))
            opts.script_verb = SCRIPT_DUMP;
        else if (!strcmp(argv[j], "embed"))
            opts.script_verb = SCRIPT_EMBED;
        else {
            fprintf(stderr, "Unrecognised argument %s for script verb\n", argv[j]);
            return false;
        }
    } else {
        fprintf(stderr, "Missing argument for script verb\n");
        return false;
    }

    if (++j < argc) {
        if (opts.script_verb == SCRIPT_DUMP)
            opts.out_path = argv[j];
        else if (opts.script_verb == SCRIPT_EMBED)
            opts.in_path = argv[j];
    } else if (opts.script_verb == SCRIPT_EMBED) {
        fprintf(stderr, "Missing in path for embed verb\n");
        return false;
    }

    if (++j < argc && opts.script_verb == SCRIPT_EMBED)
        opts.out_path = argv[j];

    return true;
}

static bool parse_strtab_verb(int argc, char* const* argv, int i) {
    int j = i + 1;

    if (j < argc) {
        opts.strtab_name = argv[j];
    } else {
        fprintf(stderr, "Missing script name for strtab\n");
        return false;
    }

    if (++j < argc) {
        if (!strcmp(argv[j], "dump"))
            opts.strtab_verb = STRTAB_DUMP;
        else if (!strcmp(argv[j], "embed"))
            opts.strtab_verb = STRTAB_EMBED;
        else {
            fprintf(stderr, "Unrecognised argument %s for strtab verb\n", argv[j]);
            return false;
        }
    } else {
        fprintf(stderr, "Missing argument for strtab verb\n");
        return false;
    }

    if (++j < argc) {
        if (opts.strtab_verb == STRTAB_EMBED) {
            opts.strtab_idx = strtoul(argv[j], NULL, 0);
            if (errno != 0)
                opts.has_strtab_idx = false; /* FIXME: Non-portable? */
            else
                opts.has_strtab_idx = true;
        }

        if (opts.strtab_verb == STRTAB_DUMP)
            opts.out_path = argv[j];
    }

    if (++j < argc) {
        if (opts.strtab_verb == STRTAB_DUMP) {
            opts.strtab_idx = strtoul(argv[j], NULL, 0);
            if (errno != 0)
                opts.has_strtab_idx = false; /* FIXME: Non-portable? */
            else
                opts.has_strtab_idx = true;
        }
        else if (opts.strtab_verb == STRTAB_EMBED)
            opts.in_path = argv[j];
    }

    return true;
}

static bool parse_argv(int argc, char* const* argv) {
    if (argc >= 2)
        opts.rom_path = argv[1];
    else {
        fprintf(stderr, "Missing ROM path\n");
        return false;
    }

    if (argc >= 3) {
        if (!strcmp(argv[2], "script")) {
            opts.verb = VERB_SCRIPT;
            return parse_script_verb(argc, argv, 2);
        } else if (!strcmp(argv[2], "strtab")) {
            opts.verb = VERB_STRTAB;
            return parse_strtab_verb(argc, argv, 2);
        } else {
            fprintf(stderr, "Unrecognized verb %s\n", argv[2]);
            return false;
        }
    } else {
        return false;
    }

    return true;
}

static bool script_verbs(uint8_t* rom, size_t sz) {
    bool ret = false;

    const struct script_desc* desc = script_for_name(opts.script_name);
    if (!desc) {
        fprintf(stderr, "Unrecognized script %s\n", opts.script_name);
        return false;
    }

    FILE* fin = NULL, * fout = NULL;
    if (opts.in_path) {
        fin = fopen(opts.in_path, "rb");
        if (!fin) {
            perror("fopen");
            goto done;
        }
    }
    if (opts.out_path) {
        fout = fopen(opts.out_path, "wb");
        if (!fout) {
            perror("fopen");
            goto done;
        }
    }

    init_script_handlers();

    if (opts.script_verb == SCRIPT_DUMP)
        ret = script_dump(rom, sz, desc, fout ? fout : stdout);
    /* FIXME: Embed */

done:
    if (fin && fclose(fin))
        perror("fclose");
    if (fout && fclose(fout))
        perror("fclose");

    return ret;
}

static bool strtab_verbs(const uint8_t* rom, size_t sz) {
    bool ret = false;

    uint32_t strtab_vma = 0;
    if (!strcmp(opts.script_name, "Menu"))
        strtab_vma = STRTAB_MENU_VMA;
    else {
        const struct script_desc* desc = script_for_name(opts.script_name);
        if (!desc) {
            fprintf(stderr, "Unrecognized strtab name %s\n", opts.script_name);
            return false;
        }
        strtab_vma = desc->strtab_vma;
    }

    FILE* fin = NULL, * fout = NULL;
    if (opts.in_path) {
        fin = fopen(opts.in_path, "rb");
        if (!fin) {
            perror("fopen");
            goto done;
        }
    }
    if (opts.out_path) {
        fout = fopen(opts.out_path, "wb");
        if (!fout) {
            perror("fopen");
            goto done;
        }
    }

    if (opts.strtab_verb == SCRIPT_DUMP)
        ret = strtab_dump(rom, strtab_vma, opts.strtab_idx, opts.has_strtab_idx,
            fout ? fout : stdout);
    /* FIXME: Embed */

done:
    if (fin && fclose(fin))
        perror("fclose");
    if (fout && fclose(fout))
        perror("fclose");

    return ret;
}

uint32_t do_crc32(const void* buf, size_t size);

static bool host_is_le() {
    union {
        uint16_t u;
        uint8_t c;
    } u = {0x00ff};
    return u.c == UINT8_MAX;
}

int main(int argc, char** argv) {
    int ret = EXIT_SUCCESS;
    uint8_t* rom = NULL;

    if (!host_is_le()) {
        fprintf(stderr, "Only little-endian host is supported\n");
        return EXIT_FAILURE;
    }

    if (!parse_argv(argc, argv)) {
        usage();
        return EXIT_FAILURE;
    }

    int rom_fd = open(opts.rom_path, O_RDWR | O_SYMLINK);
    if (rom_fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    static struct stat rom_st;
    if (fstat(rom_fd, &rom_st) == -1) {
        perror("fstat");
        return EXIT_FAILURE;
    }

    rom = mmap(NULL, rom_st.st_size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE, rom_fd, 0);
    if (!rom || !rom_st.st_size) {
        perror("mmap");
        goto done;
    }

    if (do_crc32(rom, rom_st.st_size) != 0x318a1e9b) {
        fprintf(stderr, "ROM appears to be non-stock, proceeding..\n");
    }

    switch (opts.verb) {
        case VERB_SCRIPT: {
            ret = script_verbs(rom, rom_st.st_size) ? EXIT_SUCCESS : EXIT_FAILURE;
            break;
        }

        case VERB_STRTAB: {
            ret = strtab_verbs(rom, rom_st.st_size) ? EXIT_SUCCESS : EXIT_FAILURE;
            break;
        }

        case VERB_NOP:
        default:
            fprintf(stderr, "Unrecognized or missing verbs\n");
            usage();
            ret = EXIT_FAILURE;
            goto done;
    }

done:
    if (rom)
        munmap(rom, rom_st.st_size);
    if (rom_fd != -1)
        close(rom_fd);

    return ret;
}
