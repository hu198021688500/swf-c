/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * trietool.c - Trie manipulation tool
 * Created: 2006-08-15
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#if defined(HAVE_LOCALE_CHARSET)
#include <localcharset.h>
#elif defined (HAVE_LANGINFO_CODESET)
#include <langinfo.h>
#define locale_charset()  nl_langinfo(CODESET)
#endif
#include <iconv.h>

#include <assert.h>

#include <datrie/trie.h>

#include "argv.h"
#include "utarray.h"

/* iconv encoding name for AlphaChar string */
#define ALPHA_ENC   "UCS-4LE"

#define N_ELEMENTS(a)   (sizeof(a)/sizeof((a)[0]))
#define ngx_string(str) { sizeof(str) - 1, (u_char *) str }

#define MAX_WORD_LEN    20
#define MAX_LINE_LEN    20*2+1
#define MAX_TEXT_LEN    1024
#define FILE_NAME_LEN   256

typedef struct {
    const char *path;
    const char *trie_name;
    iconv_t to_alpha_conv;
    iconv_t from_alpha_conv;
    Trie *trie;
    UT_array *replace_words;
} ProgEnv;

typedef struct {
    int len;
    AlphaChar *data;
} word_str;

typedef struct {
    word_str ori_str;
    word_str replace_str;
} word_pair;


static void init_conv(ProgEnv *env);
static size_t conv_to_alpha(ProgEnv *env, const char *in, AlphaChar *out, size_t out_size);
static size_t conv_from_alpha(ProgEnv *env, const AlphaChar *in, char *out, size_t out_size);
static void close_conv(ProgEnv *env);

static int prepare_trie(ProgEnv *env);
static int close_trie(ProgEnv *env);

static int decode_switch(int argc, char *argv[], ProgEnv *env);
static int decode_command(int argc, char *argv[], ProgEnv *env);

static int command_add(int argc, char *argv[], ProgEnv *env);
static int command_add_list(int argc, char *argv[], ProgEnv *env);
static int command_delete(int argc, char *argv[], ProgEnv *env);
static int command_delete_list(int argc, char *argv[], ProgEnv *env);
static int command_query(int argc, char *argv[], ProgEnv *env);
static int command_list(int argc, char *argv[], ProgEnv *env);
static int command_replace(int argc, char *argv[], ProgEnv *env);

static void usage(const char *prog_name, int exit_status);

static char *string_trim(char *s);

static void accept_command(ProgEnv *env);

static int text_alpha_replace_word(AlphaChar *text_alpha, const word_pair *word_pair, const int offset);

int main(int argc, char *argv[]) {
    int i, ret;
    ProgEnv env;

    env.path = ".";

    init_conv(&env);

    i = decode_switch(argc, argv, &env);
    if (i == argc)
        usage(argv[0], EXIT_FAILURE);

    env.trie_name = argv[i++];

    if (prepare_trie(&env) != 0)
        exit(EXIT_FAILURE);

    UT_icd word_pair_icd = {sizeof (word_pair), NULL, NULL, NULL};
    utarray_new(env.replace_words, &word_pair_icd);

    //ret = decode_command (argc - i, argv + i, &env);
    accept_command(&env);

    if (close_trie(&env) != 0)
        exit(EXIT_FAILURE);

    close_conv(&env);

    return ret;
}

static void
accept_command(ProgEnv *env) {
    int argc;
    char **argv;
    char *ignore = " ";
    char cmd_line_str[255] = {'\0'};

    while (TRUE) {
        printf("Please input the command:\n");

        gets(cmd_line_str);

        if (cmd_line_str[0] != '\0') {
            argv = argv_create(cmd_line_str, ignore, &argc);
            if (NULL != argv && 0 < argc) {
                decode_command(argc, argv, env);
            }

            memset(cmd_line_str, 0, 255);
            argv_destroy(argv);
        }
    }
}

static void
init_conv(ProgEnv *env) {
    const char *prev_locale;
    const char *locale_codeset;

    prev_locale = setlocale(LC_CTYPE, "");
    locale_codeset = locale_charset();
    setlocale(LC_CTYPE, prev_locale);

    env->to_alpha_conv = iconv_open(ALPHA_ENC, locale_codeset);
    env->from_alpha_conv = iconv_open(locale_codeset, ALPHA_ENC);
}

static size_t
conv_to_alpha(ProgEnv *env, const char *in, AlphaChar *out, size_t out_size) {
    char *in_p = (char *) in;
    char *out_p = (char *) out;
    size_t in_left = strlen(in);
    size_t out_left = out_size * sizeof (AlphaChar);
    size_t res;
    const unsigned char *byte_p;

    assert(sizeof (AlphaChar) == 4);

    /* convert to UCS-4LE */
    res = iconv(env->to_alpha_conv, (char **) &in_p, &in_left,
            &out_p, &out_left);

    if (res < 0)
        return res;

    /* convert UCS-4LE to AlphaChar string */
    res = 0;
    for (byte_p = (const unsigned char *) out;
            res < out_size && byte_p + 3 < (unsigned char*) out_p;
            byte_p += 4) {
        out[res++] = byte_p[0]
                | (byte_p[1] << 8)
                | (byte_p[2] << 16)
                | (byte_p[3] << 24);
    }
    if (res < out_size) {
        out[res] = 0;
    }

    return res;
}

static size_t
conv_from_alpha(ProgEnv *env, const AlphaChar *in, char *out, size_t out_size) {
    size_t in_left = alpha_char_strlen(in) * sizeof (AlphaChar);
    size_t res;

    assert(sizeof (AlphaChar) == 4);

    /* convert AlphaChar to UCS-4LE */
    for (res = 0; in[res]; res++) {
        unsigned char b[4];

        b[0] = in[res] & 0xff;
        b[1] = (in[res] >> 8) & 0xff;
        b[2] = (in[res] >> 16) & 0xff;
        b[3] = (in[res] >> 24) & 0xff;

        memcpy((char *) &in[res], b, 4);
    }

    /* convert UCS-4LE to locale codeset */
    res = iconv(env->from_alpha_conv, (char **) &in, &in_left,
            &out, &out_size);
    *out = 0;

    return res;
}

static void
close_conv(ProgEnv *env) {
    iconv_close(env->to_alpha_conv);
    iconv_close(env->from_alpha_conv);
}

static int
prepare_trie(ProgEnv *env) {
    char buff[256];

    snprintf(buff, sizeof (buff),
            "%s/%s.tri", env->path, env->trie_name);
    env->trie = trie_new_from_file(buff);

    if (!env->trie) {
        FILE *sbm;
        AlphaMap *alpha_map;

        snprintf(buff, sizeof (buff),
                "%s/%s.abm", env->path, env->trie_name);
        sbm = fopen(buff, "r");
        if (!sbm) {
            fprintf(stderr, "Cannot open alphabet map file %s\n", buff);
            return -1;
        }

        alpha_map = alpha_map_new();

        while (fgets(buff, sizeof (buff), sbm)) {
            int b, e;

            /* read the range
             * format: [b,e]
             * where: b = begin char, e = end char; both in hex values
             */
            if (sscanf(buff, " [ %x , %x ] ", &b, &e) != 2)
                continue;
            if (b > e) {
                fprintf(stderr, "Range begin (%x) > range end (%x)\n", b, e);
                continue;
            }

            alpha_map_add_range(alpha_map, b, e);
        }

        env->trie = trie_new(alpha_map);

        alpha_map_free(alpha_map);
        fclose(sbm);
    }

    return 0;
}

static int
close_trie(ProgEnv *env) {
    if (trie_is_dirty(env->trie)) {
        char path[256];

        snprintf(path, sizeof (path),
                "%s/%s.tri", env->path, env->trie_name);
        if (trie_save(env->trie, path) != 0) {
            fprintf(stderr, "Cannot save trie to %s\n", path);
            return -1;
        }
    }

    trie_free(env->trie);
    return 0;
}

static int
decode_switch(int argc, char *argv[], ProgEnv *env) {
    int opt_idx;

    for (opt_idx = 1; opt_idx < argc && *argv[opt_idx] == '-'; opt_idx++) {
        if (strcmp(argv[opt_idx], "-h") == 0 ||
                strcmp(argv[opt_idx], "--help") == 0) {
            usage(argv[0], EXIT_FAILURE);
        } else if (strcmp(argv[opt_idx], "-V") == 0 ||
                strcmp(argv[opt_idx], "--version") == 0) {
            printf("%s\n", VERSION);
            exit(EXIT_FAILURE);
        } else if (strcmp(argv[opt_idx], "-p") == 0 ||
                strcmp(argv[opt_idx], "--path") == 0) {
            env->path = argv[++opt_idx];
        } else if (strcmp(argv[opt_idx], "--") == 0) {
            ++opt_idx;
            break;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[opt_idx]);
            exit(EXIT_FAILURE);
        }
    }

    return opt_idx;
}

static int
decode_command(int argc, char *argv[], ProgEnv *env) {
    int opt_idx;

    for (opt_idx = 0; opt_idx < argc; opt_idx++) {
        if (strcmp(argv[opt_idx], "add") == 0) {
            ++opt_idx;
            opt_idx += command_add(argc - opt_idx, argv + opt_idx, env);
        } else if (strcmp(argv[opt_idx], "add-list") == 0) {
            ++opt_idx;
            opt_idx += command_add_list(argc - opt_idx, argv + opt_idx, env);
        } else if (strcmp(argv[opt_idx], "delete") == 0) {
            ++opt_idx;
            opt_idx += command_delete(argc - opt_idx, argv + opt_idx, env);
        } else if (strcmp(argv[opt_idx], "delete-list") == 0) {
            ++opt_idx;
            opt_idx += command_delete_list(argc - opt_idx, argv + opt_idx, env);
        } else if (strcmp(argv[opt_idx], "query") == 0) {
            ++opt_idx;
            opt_idx += command_query(argc - opt_idx, argv + opt_idx, env);
        } else if (strcmp(argv[opt_idx], "list") == 0) {
            ++opt_idx;
            opt_idx += command_list(argc - opt_idx, argv + opt_idx, env);
        } else if (strcmp(argv[opt_idx], "replace") == 0) {
            ++opt_idx;
            opt_idx += command_replace(argc - opt_idx, argv + opt_idx, env);
        } else {
            fprintf(stderr, "Unknown command: %s\n", argv[opt_idx]);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

static int
command_add(int argc, char *argv[], ProgEnv *env) {
    int opt_idx;

    opt_idx = 0;
    while (opt_idx < argc) {
        const char *key;
        AlphaChar key_alpha[256];
        TrieData data;

        key = argv[opt_idx++];
        data = (opt_idx < argc) ? atoi(argv[opt_idx++]) : TRIE_DATA_ERROR;

        conv_to_alpha(env, key, key_alpha, N_ELEMENTS(key_alpha));
        if (!trie_store(env->trie, key_alpha, data)) {
            fprintf(stderr, "Failed to add entry '%s' with data %d\n",
                    key, data);
        }
    }

    return opt_idx;
}

static int
command_add_list(int argc, char *argv[], ProgEnv *env) {
    const char *enc_name, *input_name;
    int opt_idx;
    iconv_t saved_conv;
    FILE *input;
    unsigned char line[256];

    enc_name = 0;
    opt_idx = 0;
    saved_conv = env->to_alpha_conv;
    if (strcmp(argv[0], "-e") == 0 ||
            strcmp(argv[0], "--encoding") == 0) {
        if (++opt_idx >= argc) {
            fprintf(stderr, "add-list option \"%s\" requires encoding name", argv[0]);
            return opt_idx;
        }
        enc_name = argv[opt_idx++];
    }
    if (opt_idx >= argc) {
        fprintf(stderr, "add-list requires input word list file name\n");
        return opt_idx;
    }
    input_name = argv[opt_idx++];

    if (enc_name) {
        iconv_t conv = iconv_open(ALPHA_ENC, enc_name);
        if ((iconv_t) - 1 == conv) {
            fprintf(stderr, "Conversion from \"%s\" to \"%s\" is not supported.\n", enc_name, ALPHA_ENC);
            return opt_idx;
        }

        env->to_alpha_conv = conv;
    }

    input = fopen(input_name, "r");
    if (!input) {
        fprintf(stderr, "add-list: Cannot open input file \"%s\"\n", input_name);
        goto exit_iconv_openned;
    }

    while (fgets(line, sizeof line, input)) {
        TrieData data_val;
        unsigned char *ori_word, *replace_word;
        int ori_word_alpha_len, replace_word_alpha_len;
        AlphaChar *p, *q, ori_word_alpha[MAX_WORD_LEN] = {0}, replace_word_alpha[MAX_WORD_LEN] = {0};

        ori_word = string_trim(line);
        if ('\0' != *ori_word) {
            for (replace_word = ori_word; *replace_word && !strchr("\t,", *replace_word); ++replace_word)
                ;
            if ('\0' != *replace_word) {
                *replace_word++ = '\0';
                while (isspace(*replace_word))
                    ++replace_word;
            }

            conv_to_alpha(env, ori_word, ori_word_alpha, N_ELEMENTS(ori_word_alpha));

            data_val = (TrieData) utarray_len(env->replace_words);

            if (trie_store(env->trie, ori_word_alpha, data_val)) {
                word_pair word_data;
                ori_word_alpha_len = alpha_char_strlen(ori_word_alpha);
                word_data.ori_str.len = ori_word_alpha_len;
                word_data.ori_str.data = (AlphaChar *) calloc(ori_word_alpha_len + 1, sizeof (AlphaChar));
                p = word_data.ori_str.data;
                q = ori_word_alpha;
                while (ori_word_alpha_len > 0) {
                    *p = *q;
                    p++;
                    q++;
                    ori_word_alpha_len--;
                }

                conv_to_alpha(env, replace_word, replace_word_alpha, N_ELEMENTS(replace_word_alpha));
                replace_word_alpha_len = alpha_char_strlen(replace_word_alpha);
                word_data.replace_str.len = replace_word_alpha_len;
                word_data.replace_str.data = (AlphaChar *) calloc(replace_word_alpha_len + 1, sizeof (AlphaChar));
                p = word_data.replace_str.data;
                q = replace_word_alpha;
                while (replace_word_alpha_len > 0) {
                    *p = *q;
                    p++;
                    q++;
                    replace_word_alpha_len--;
                }

                utarray_push_back(env->replace_words, &word_data);
            }
        }
    }

    fclose(input);

exit_iconv_openned:
    if (enc_name) {
        iconv_close(env->to_alpha_conv);
        env->to_alpha_conv = saved_conv;
    }

    return opt_idx;
}

static int
command_delete(int argc, char *argv[], ProgEnv *env) {
    int opt_idx;

    for (opt_idx = 0; opt_idx < argc; opt_idx++) {
        AlphaChar key_alpha[256];

        conv_to_alpha(env, argv[opt_idx], key_alpha, N_ELEMENTS(key_alpha));
        if (!trie_delete(env->trie, key_alpha)) {
            fprintf(stderr, "No entry '%s'. Not deleted.\n", argv[opt_idx]);
        }
    }

    return opt_idx;
}

static int
command_delete_list(int argc, char *argv[], ProgEnv *env) {
    const char *enc_name, *input_name;
    int opt_idx;
    iconv_t saved_conv;
    FILE *input;
    char line[256];

    enc_name = 0;
    opt_idx = 0;
    saved_conv = env->to_alpha_conv;
    if (strcmp(argv[0], "-e") == 0 ||
            strcmp(argv[0], "--encoding") == 0) {
        if (++opt_idx >= argc) {
            fprintf(stderr, "delete-list option \"%s\" requires encoding name",
                    argv[0]);
            return opt_idx;
        }
        enc_name = argv[opt_idx++];
    }
    if (opt_idx >= argc) {
        fprintf(stderr, "delete-list requires input word list file name\n");
        return opt_idx;
    }
    input_name = argv[opt_idx++];

    if (enc_name) {
        iconv_t conv = iconv_open(ALPHA_ENC, enc_name);
        if ((iconv_t) - 1 == conv) {
            fprintf(stderr,
                    "Conversion from \"%s\" to \"%s\" is not supported.\n",
                    enc_name, ALPHA_ENC);
            return opt_idx;
        }

        env->to_alpha_conv = conv;
    }

    input = fopen(input_name, "r");
    if (!input) {
        fprintf(stderr, "delete-list: Cannot open input file \"%s\"\n",
                input_name);
        goto exit_iconv_openned;
    }

    while (fgets(line, sizeof line, input)) {
        char *p;

        p = string_trim(line);
        if ('\0' != *p) {
            AlphaChar key_alpha[256];

            conv_to_alpha(env, p, key_alpha, N_ELEMENTS(key_alpha));
            if (!trie_delete(env->trie, key_alpha)) {
                fprintf(stderr, "No entry '%s'. Not deleted.\n", p);
            }
        }
    }

    fclose(input);

exit_iconv_openned:
    if (enc_name) {
        iconv_close(env->to_alpha_conv);
        env->to_alpha_conv = saved_conv;
    }

    return opt_idx;
}

static int
command_query(int argc, char *argv[], ProgEnv *env) {
    AlphaChar key_alpha[256];
    TrieData data;

    if (argc == 0) {
        fprintf(stderr, "query: No key specified.\n");
        return 0;
    }

    conv_to_alpha(env, argv[0], key_alpha, N_ELEMENTS(key_alpha));
    if (trie_retrieve(env->trie, key_alpha, &data)) {
        printf("%d\n", data);
    } else {
        fprintf(stderr, "query: Key '%s' not found.\n", argv[0]);
    }

    return 1;
}

static Bool
list_enum_func(const AlphaChar *key, TrieData key_data, void *user_data) {
    ProgEnv *env = (ProgEnv *) user_data;
    char key_locale[1024];

    conv_from_alpha(env, key, key_locale, N_ELEMENTS(key_locale));
    printf("%s\t%d\n", key_locale, key_data);
    return TRUE;
}

static int
command_list(int argc, char *argv[], ProgEnv *env) {
    trie_enumerate(env->trie, list_enum_func, (void *) env);
    return 0;
}

static int
command_replace(int argc, char *argv[], ProgEnv *env) {
    TrieState *s;
    int data, offset, flag;
    const AlphaChar *p, *tmp;
    const word_pair *p_word_pair;
    AlphaChar text_alpha[MAX_TEXT_LEN] = {0};

    if (argc == 0) {
        fprintf(stderr, "query: No content specified.\n");
        return 0;
    }

    conv_to_alpha(env, argv[0], text_alpha, N_ELEMENTS(text_alpha));

    tmp = text_alpha;

    while (*tmp) {
        p = tmp;
        s = trie_root(env->trie);
        if (!trie_state_is_walkable(s, *p)) {
            tmp++;
            trie_state_free(s);
            continue;
        } else {
            trie_state_walk(s, *p++);
        }
        while (trie_state_is_walkable(s, *p) && !trie_state_is_terminal(s)) {
            trie_state_walk(s, *p++);
        }
        if (trie_state_is_terminal(s)) {
            data = (int) trie_state_get_data(s);
            printf("%d\n", data);
            if (data) {
                p_word_pair = (word_pair*) utarray_eltptr(env->replace_words, data);
                if (p_word_pair != NULL) {
                    offset = tmp - text_alpha;
                    flag = text_alpha_replace_word(text_alpha, p_word_pair, offset);
                    tmp = tmp + (flag - 1);
                } else {
                    fprintf(stderr, "Replace:Find word but not found in dynamic[%d] arrays.\n", data);
                }
            }
        }
        trie_state_free(s);
        tmp++;
    }

    char text_locale[256];
    conv_from_alpha(env, text_alpha, text_locale, N_ELEMENTS(text_locale));
    fprintf(stderr, "%s\n", text_locale);
    return 1;

}

static int
text_alpha_replace_word(AlphaChar *text_alpha, const word_pair *word_pair, const int offset) {
    AlphaChar *p, *q;
    int flag, text_alpha_length;

    flag = word_pair->replace_str.len - word_pair->ori_str.len;

    if (flag > 0) {
        text_alpha_length = alpha_char_strlen(text_alpha);
        if (text_alpha_length == MAX_TEXT_LEN || text_alpha_length + flag > MAX_TEXT_LEN) {
            printf("Array index out of bounds.\n");
        }
        p = text_alpha + (text_alpha_length + flag - 1);
        q = text_alpha + (text_alpha_length - 1);
        int num = text_alpha_length - (offset + word_pair->ori_str.len);
        while (num > 0) {
            *p = *q;
            p--;
            q--;
            num--;
        }
    } else if (flag < 0) {
        p = text_alpha + (offset + word_pair->replace_str.len);
        q = text_alpha + (offset + word_pair->ori_str.len);
        while (*q) {
            *p = *q;
            p++;
            q++;
        }
        while (*p) {
            *p = 0;
            p++;
        }
    }
    p = word_pair->replace_str.data;
    q = text_alpha + offset;
    while (*p) {
        *q = *p;
        p++;
        q++;
    }
    return word_pair->replace_str.len;
}

static void
usage(const char *prog_name, int exit_status) {
    printf("%s - double-array trie manipulator\n", prog_name);
    printf("Usage: %s [OPTION]... TRIE CMD ARG ...\n", prog_name);
    printf(
            "Options:\n"
            "  -p, --path DIR           set trie directory to DIR [default=.]\n"
            "  -h, --help               display this help and exit\n"
            "  -V, --version            output version information and exit\n"
            "\n"
            "Commands:\n"
            "  add  WORD DATA ...\n"
            "      Add WORD with DATA to trie\n"
            "  add-list [OPTION] LISTFILE\n"
            "      Add words and data listed in LISTFILE to trie\n"
            "      Options:\n"
            "          -e, --encoding ENC    specify character encoding of LISTFILE\n"
            "  delete WORD ...\n"
            "      Delete WORD from trie\n"
            "  delete-list [OPTION] LISTFILE\n"
            "      Delete words listed in LISTFILE from trie\n"
            "      Options:\n"
            "          -e, --encoding ENC    specify character encoding of LISTFILE\n"
            "  query WORD\n"
            "      Query WORD data from trie\n"
            "  list\n"
            "      List all words in trie\n"
            );

    exit(exit_status);
}

static char *
string_trim(char *s) {
    char *p;

    /* skip leading white spaces */
    while (*s && isspace(*s))
        ++s;

    /* trim trailing white spaces */
    p = s + strlen(s) - 1;
    while (isspace(*p))
        --p;
    *++p = '\0';

    return s;
}

/*
vi:ts=4:ai:expandtab
 */