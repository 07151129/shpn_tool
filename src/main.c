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
#include "embed.h"
#include "script_as.h"
#include "script_disass.h"
#include "strtab.h"

static void usage() {
    fprintf(stderr,
        "shpn-tool: analyze and modify Silent Hill Play Novel GBA ROM\n"
    #ifdef HAS_ICONV
        "Built with iconv support"
    #endif
        "\nusage: <ROM> <verb> [...]\n\n"
        "ROM is the AGB-ASHJ ROM path\n"
        "Supported verbs:\n"
        "script <name> <vma> <strtab_script_vma> <strtab_menu_vma> <dump | embed>\n"
        "\tdump [out] -- Dump script to file at \"out\" or to stdout\n"
        "\tembed <in> <use_rom_strtab> <size> <strtab> <menu> <strtab_sz> <menu_sz> <out> -- Embed script at \"in\" "
        "with strtab at \"strtab\", menu strtab at \"menu\" into \"out\""
        "\n\n"
        "strtab <vma> <dump | embed>\n"
        "\tdump [out] [idx] -- Dump strtab entry at \"idx\" or all "
            "entries to file at \"out\" or stdout\n"
        "\tembed <in> <size> <Script|Menu> <out> -- Embed all strtab entries from file \"in\" to "
        "file \"out\""
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
    char* strtab_script_path, * strtab_menu_path;
    union {
        uint32_t strtab_vma; /* for strtab verb */
        struct {
            char* script_name;
            uint32_t script_vma, script_sz, strtab_script_vma, strtab_menu_vma;
        }; /* for script verbs */
    };
    union {
        uint32_t strtab_sz; /* for standalone strtab embedding */
        struct { uint32_t strtab_script_sz, strtab_menu_sz; }; /* for script embedding */
    };
    uint32_t strtab_idx;
    bool has_strtab_idx;
    bool strtab_embed_script;
    bool use_rom_strtabs;
} opts;

/* FIXME: Refactor arg parsing.. */

static bool parse_script_verb(int argc, char* const* argv, int i) {
    int j = i + 1;

    if (j < argc)
        opts.script_name = argv[j];
    else {
        fprintf(stderr, "Missing script name\n");
        return false;
    }

    if (++j < argc) {
        char* end;
        opts.script_vma = strtoul(argv[j], &end, 0);
    } else if (!opts.script_vma) {
        fprintf(stderr, "Missing non-zero script address\n");
        return false;
    }

    if (++j < argc) {
        char* end;
        opts.strtab_script_vma = strtoul(argv[j], &end, 0);
    } else if (!opts.strtab_vma) {
        fprintf(stderr, "Missing non-zero address for script strtab\n");
        return false;
    }

    if (++j < argc) {
        char* end;
        opts.strtab_menu_vma = strtoul(argv[j], &end, 0);
    } else if (!opts.strtab_vma) {
        fprintf(stderr, "Missing non-zero address for menu strtab\n");
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

    if (opts.script_verb == SCRIPT_EMBED && ++j < argc) {
        opts.use_rom_strtabs = atoi(argv[j]) > 0;
    } else if (opts.script_verb == SCRIPT_EMBED) {
        fprintf(stderr, "Missing use_rom_strtabs for embed verb\n");
        return false;
    }

    if (opts.script_verb == SCRIPT_EMBED) {
        if (++j < argc) {
            char* end;
            opts.script_sz = strtoul(argv[j], &end, 0);
        } else if (!opts.script_sz) {
            fprintf(stderr, "Missing non-zero script size\n");
            return false;
        }

        if (++j < argc)
            opts.strtab_script_path = argv[j];
        else {
            fprintf(stderr, "Missing script strtab path\n");
            return false;
        }

        if (++j < argc)
            opts.strtab_menu_path = argv[j];
        else {
            fprintf(stderr, "Missing menu strtab path\n");
            return false;
        }

        if (++j < argc) {
            char* end;
            opts.strtab_script_sz = strtoul(argv[j], &end, 0);
        } else if (!opts.strtab_script_sz) {
            fprintf(stderr, "Missing non-zero size for script strtab\n");
            return false;
        }

        if (++j < argc) {
            char* end;
            opts.strtab_menu_sz = strtoul(argv[j], &end, 0);
        } else if (!opts.strtab_menu_sz) {
            fprintf(stderr, "Missing non-zero size for menu strtab\n");
            return false;
        }

        if (++j < argc)
            opts.out_path = argv[j];
        else {
            fprintf(stderr, "Missing destination ROM path\n");
            return false;
        }
    }

    return true;
}

static bool parse_strtab_verb(int argc, char* const* argv, int i) {
    int j = i + 1;

    if (j < argc) {
        char* end;
        opts.strtab_vma = strtoul(argv[j], &end, 0);
    } else if (!opts.strtab_vma) {
        fprintf(stderr, "Missing non-zero address for strtab\n");
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
        if (opts.strtab_verb == STRTAB_EMBED)
            opts.in_path = argv[j];

        if (opts.strtab_verb == STRTAB_DUMP)
            opts.out_path = argv[j];
    }

    if (opts.strtab_verb == STRTAB_EMBED) {
        if (++j < argc) {
            char* end;
            opts.strtab_sz = strtoul(argv[j], &end, 0);
        } else if (!opts.strtab_sz) {
            fprintf(stderr, "Missing non-zero size for strtab\n");
            return false;
        }

        if (++j < argc) {
            opts.strtab_embed_script = !strcmp(argv[j], "Script");
        } else if (!opts.strtab_sz) {
            fprintf(stderr, "Missing embed target for strtab\n");
            return false;
        }
    }

    if (++j < argc) {
        if (opts.strtab_verb == STRTAB_DUMP) {
            char* end;
            opts.strtab_idx = strtoul(argv[j], &end, 0);
            opts.has_strtab_idx = end > argv[j];
        }
        else if (opts.strtab_verb == STRTAB_EMBED)
            opts.out_path = argv[j];
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

/* Amount of padding that ROM needs to accomodate sz_desired bytes at vma_desired */
static uint32_t rom_pad_sz(size_t rom_sz, uint32_t vma_desired, size_t sz_desired) {
    if (VMA2OFFS(vma_desired) + sz_desired < rom_sz)
        return 0;
    return VMA2OFFS(vma_desired) + sz_desired - rom_sz;
}

static bool regions_intersect(uint32_t vma_first, size_t sz_first, uint32_t vma_second,
    size_t sz_second) {
    return vma_first + sz_first > vma_second && vma_second + sz_second > vma_first;
}

static bool script_verbs(uint8_t* rom, size_t sz) {
    assert(opts.strtab_script_vma && opts.strtab_menu_vma);

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
        ret = script_dump(rom, sz, opts.script_vma, desc, fout ? fout : stdout, opts.strtab_script_vma,
            opts.strtab_menu_vma);
    else if (opts.script_verb == SCRIPT_EMBED) {
        assert(fin && fout);
        assert(opts.strtab_script_path && opts.strtab_menu_path);
        assert(opts.strtab_script_sz && opts.strtab_menu_sz);

        size_t pad_sz = rom_pad_sz(sz, opts.strtab_script_vma, opts.strtab_script_sz) +
            rom_pad_sz(sz, opts.strtab_menu_vma, opts.strtab_menu_sz) +
            rom_pad_sz(sz, opts.script_vma, opts.script_sz);
        if (sz + pad_sz > MAX_ROM_SZ) {
            fprintf(stderr, "Embedding would exceed maximum allowed ROM size 0x%llx\n", MAX_ROM_SZ);
            goto done;
        }
        if (regions_intersect(opts.strtab_script_vma, opts.strtab_script_sz,
            opts.strtab_menu_vma, opts.strtab_menu_sz) ||
            regions_intersect(opts.strtab_script_vma, opts.strtab_script_sz,
            opts.script_vma, opts.script_sz) ||
            regions_intersect(opts.strtab_menu_vma, opts.strtab_menu_sz,
                opts.script_vma, opts.script_sz)) {
            fprintf(stderr, "Specified memory regions would intersect\n");
            goto done;
        }

        uint8_t * rom_cpy = NULL;

        FILE* strtab_scr = NULL, * strtab_menu = NULL;
        strtab_scr = fopen(opts.strtab_script_path, "rb");
        strtab_menu = fopen(opts.strtab_menu_path, "rb");
        if (!strtab_scr || !strtab_menu) {
            perror("fopen");
            goto done_embed;
        }

        size_t sz_strtab_scr = 0;
        size_t sz_strtab_menu = 0;
        size_t sz_script = 0;

        struct stat st;
        if (stat(opts.in_path, &st) == -1) {
            perror("stat");
            goto done_embed;
        }
        sz_script = st.st_size;

        if (stat(opts.strtab_script_path, &st) == -1) {
            perror("stat");
            goto done_embed;
        }
        sz_strtab_scr = st.st_size;

        if (stat(opts.strtab_menu_path, &st) == -1) {
            perror("stat");
            goto done_embed;
        }
        sz_strtab_menu = st.st_size;

        rom_cpy = malloc(sz + pad_sz);
        if (!rom_cpy) {
            perror("malloc");
            goto done_embed;
        }
        memcpy(rom_cpy, rom, sz);
        // memset(&rom_cpy[sz], 0xff, pad_sz);

        ret = embed_script(rom_cpy, sz + pad_sz,
                opts.script_sz,
                VMA2OFFS(opts.script_vma),
                opts.use_rom_strtabs,
                fin, strtab_scr, strtab_menu,
                opts.in_path,
                sz_script, sz_strtab_scr, sz_strtab_menu,
                opts.strtab_script_vma, opts.strtab_menu_vma,
                opts.strtab_script_sz, opts.strtab_menu_sz,
                desc->patch_info.size_vma, desc->patch_info.ptr_vma);

        if (ret) {
            ret = fwrite(rom_cpy, 1, sz + pad_sz, fout) == sz + pad_sz;
            if (!ret)
                perror("fwrite");
        }

done_embed:
        if (strtab_scr && fclose(strtab_scr))
            perror("fclose");
        if (strtab_menu && fclose(strtab_menu))
            perror("fclose");
        if (rom_cpy)
            free(rom_cpy);
    }

done:
    if (fin && fclose(fin))
        perror("fclose");
    if (fout && fclose(fout))
        perror("fclose");

    return ret;
}

static bool strtab_verbs(const uint8_t* rom, size_t sz) {
    assert(opts.strtab_vma);

    bool ret = false;

    FILE* fin = NULL, * fout = NULL;
    size_t in_sz = 0;
    if (opts.in_path) {
        fin = fopen(opts.in_path, "rb");
        if (!fin) {
            perror("fopen");
            goto done;
        }
        struct stat st;
        stat(opts.in_path, &st);
        in_sz = st.st_size;
    }

    if (opts.out_path) {
        fout = fopen(opts.out_path, "wb");
        if (!fout) {
            perror("fopen");
            goto done;
        }
    }

    if (opts.strtab_verb == STRTAB_DUMP)
        ret = strtab_dump(rom, sz, opts.strtab_vma, opts.strtab_idx, opts.has_strtab_idx,
            fout ? fout : stdout);
    else if (opts.strtab_verb == STRTAB_EMBED) {
#if !HAS_ICONV
        fprintf(stderr, "shpn_tool needs to be built with iconv support for this operation\n");
        goto done;
#endif
        if (!opts.in_path) {
            fprintf(stderr, "Missing strtab in file arg\n");
            goto done;
        }
        if (!opts.out_path) {
            fprintf(stderr, "Missing strtab out file arg\n");
            goto done;
        }

        size_t pad_sz = rom_pad_sz(sz, opts.strtab_vma, opts.strtab_sz);
        if (sz + pad_sz > MAX_ROM_SZ) {
            fprintf(stderr, "Embedding would exceed maximum allowed ROM size 0x%llx\n", MAX_ROM_SZ);
            goto done;
        }

        struct strtab_embed_ctx* ectx = strtab_embed_ctx_new();
        if (!ectx || !strtab_embed_ctx_with_file(fin, in_sz, ectx)) {
            fprintf(stderr, "Failed to process %s for embedding\n", opts.in_path);
            goto done;
        }
        ectx->rom_vma = opts.strtab_vma;

        uint8_t* rom_cpy = malloc(sz + pad_sz);
        if (!rom_cpy)
            perror("malloc");
        else {
            memcpy(rom_cpy, rom, sz);
            // memset(&rom_cpy[sz], 0xff, pad_sz);

            iconv_t conv = conv_for_embedding();
            if (conv == (iconv_t)-1 || !embed_strtab(rom_cpy, sz + pad_sz, ectx, opts.strtab_sz,
                opts.strtab_embed_script ? STRTAB_SCRIPT_PTR_VMA : STRTAB_MENU_PTR_VMA, conv))
                fprintf(stderr, "Failed to embed strtab from %s\n", opts.in_path);
            else if (fwrite(rom_cpy, 1, sz + pad_sz, fout) < sz + pad_sz)
                perror("fwrite");
            else
                ret = true;
#ifdef HAS_ICONV
            iconv_close(conv);
#endif
        }

        if (rom_cpy)
            free(rom_cpy);
        if (ectx)
            strtab_embed_ctx_free(ectx);
    }

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
