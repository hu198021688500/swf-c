#include <argz.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "config.h"
#if defined(HAVE_LOCALE_CHARSET)
#include <localcharset.h>
#elif defined (HAVE_LANGINFO_CODESET)
#include <langinfo.h>
#define locale_charset()  nl_langinfo(CODESET)
#endif

#include "argv.h"
#include "trietool.h"

#define DEBUG

#define LOG_ERROR	0
#define LOG_NOTICE	1
#define LOG_DEBUG	2

#define ALPHA_ENC       "UCS-4LE"
#define DELIM_CHAR      '-'
#define MAX_WORD_LEN    20
#define MAX_LINE_LEN    20*2+1
#define MAX_TEXT_LEN    1024
#define FILE_NAME_LEN   256
#define N_ELEMENTS(a)   (sizeof(a)/sizeof((a)[0]))

//#define DEBUG_ARGS_FORMAT       "[%s,%d,%s]"
#define DEBUG_ARGS_FORMAT       "[File:%s,Line:%d]"
//#define DEBUG_ARGS_VALUE        __FILE__,__LINE__,__FUNCTION__
#define DEBUG_ARGS_VALUE        __FILE__,__LINE__

/**
 * 程序初始化
 * @param env
 */
void
init_program_env(int argc, char **argv, ProgEnv *env) {
    env->path = ".";
    env->trie_name = "datrie";

    decode_switch(env, argc, argv);

    init_conv(env);

    prepare_trie(env);

    UT_icd word_pair_icd = {sizeof (word_pair), NULL, NULL, NULL};
    utarray_new(env->replace_words, &word_pair_icd);

    char file_name[FILE_NAME_LEN] = {'\0'};
    snprintf(file_name, sizeof (file_name), "%s/%s.list", env->path, env->trie_name);

    FILE *fp;
    if ((fp = fopen(file_name, "r")) != NULL) {
        char line[MAX_LINE_LEN] = {'\0'};
        while (fgets(line, MAX_LINE_LEN, fp)) {
            char *argv[] = {line};
            command_add(1, argv, env);
        }
    }
    fclose(fp);
}

/**
 * 接收命令
 * @param env
 */
void
init_sock_accept_env(ProgEnv *env) {
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

/**
 * 字符转换初始化
 * @param env
 */
void
init_conv(ProgEnv *env) {
    const char *prev_locale;
    const char *locale_codeset;

    prev_locale = setlocale(LC_CTYPE, "");
    locale_codeset = locale_charset();
    setlocale(LC_CTYPE, prev_locale);

    env->to_alpha_conv = iconv_open(ALPHA_ENC, locale_codeset);
    env->from_alpha_conv = iconv_open(locale_codeset, ALPHA_ENC);
}

/**
 * 将字符串转换成alpha
 * @param env
 * @param in
 * @param out
 * @param out_size
 * @return 
 */
size_t
conv_to_alpha(const char *in, AlphaChar *out, size_t out_size, ProgEnv *env) {
    char *in_p = (char *) in;
    char *out_p = (char *) out;

    size_t in_left = strlen(in);
    size_t out_left = out_size * sizeof (AlphaChar);

    size_t res;
    const unsigned char *byte_p;
    assert(sizeof (AlphaChar) == 4);

    res = iconv(env->to_alpha_conv, (char **) &in_p, &in_left, &out_p, &out_left);
    if (res < 0) {
        return res;
    }
    res = 0;
    for (byte_p = (const unsigned char *) out; res < out_size && byte_p + 3 < (unsigned char*) out_p; byte_p += 4) {
        out[res++] = byte_p[0] | (byte_p[1] << 8) | (byte_p[2] << 16) | (byte_p[3] << 24);
    }
    if (res < out_size) {
        out[res] = 0;
    }

    return res;
}

/**
 * 将alpha转换成字符串
 * @param env
 * @param in
 * @param out
 * @param out_size
 * @return 
 */
size_t
conv_from_alpha(const AlphaChar *in, char *out, size_t out_size, ProgEnv *env) {
    size_t res;
    size_t in_left = alpha_char_strlen(in) * sizeof (AlphaChar);
    assert(sizeof (AlphaChar) == 4);

    for (res = 0; in[res]; res++) {
        unsigned char b[4];
        b[0] = in[res] & 0xff;
        b[1] = (in[res] >> 8) & 0xff;
        b[2] = (in[res] >> 16) & 0xff;
        b[3] = (in[res] >> 24) & 0xff;
        memcpy((char *) &in[res], b, 4);
    }
    res = iconv(env->from_alpha_conv, (char **) &in, &in_left, &out, &out_size);

    *out = 0;

    return res;
}

void
close_conv(ProgEnv *env) {
    iconv_close(env->to_alpha_conv);
    iconv_close(env->from_alpha_conv);
}

/**
 * 获取分隔符的位置
 * 获取原始字符窜的长度
 * 获取替换字符窜的长度
 * @param str 
 * @param ori_word_len
 * @param replace_word_len
 * @return 
 */
char *
get_word_pair_info(char *str, unsigned *ori_word_len, unsigned *replace_word_len) {
    Bool flag = TRUE;
    char *delimPos = NULL, *tmp = str;
    *ori_word_len = 0, *replace_word_len = 1;

    while (*tmp && *tmp != '\n' && *tmp != '\r') {
        if (flag) {
            (*ori_word_len)++;
        } else {
            (*replace_word_len)++;
        }
        if (*tmp == DELIM_CHAR) {
            flag = FALSE;
            delimPos = tmp;
        }
        tmp++;
    }

    *tmp = '\0';
    *delimPos = '\0';

    return delimPos;
}

/**
 * 初始化搜索树
 * @param env
 * @return 
 */
void
prepare_trie(ProgEnv * env) {
    char file_name[FILE_NAME_LEN] = {'\0'};
    snprintf(file_name, sizeof (file_name), "%s/%s.tri", env->path, env->trie_name);
    env->trie = trie_new_from_file(file_name);

    if (!env->trie) {
        FILE *fp;
        AlphaMap *alpha_map;

        snprintf(file_name, sizeof (file_name), "%s/%s.abm", env->path, env->trie_name);

        if ((fp = fopen(file_name, "r")) == NULL) {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Cannot open alphabet map file %s", DEBUG_ARGS_VALUE, file_name);
            return;
        }

        alpha_map = alpha_map_new();

        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof (line), fp)) {
            int b, e;
            if (sscanf(line, " [ %x , %x ] ", &b, &e) != 2) {
                continue;
            }
            if (b > e) {
                write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Range begin (%x) > range end (%x)", DEBUG_ARGS_VALUE, b, e);
                continue;
            }
            alpha_map_add_range(alpha_map, b, e);
        }

        env->trie = trie_new(alpha_map);

        alpha_map_free(alpha_map);
        fclose(fp);
    }
}

/**
 * 启动程序执行
 * @param argc 参数个数
 * @param argv 参数数组
 * @param env 程序环境
 * @return 
 */
void
decode_switch(ProgEnv * env, int argc, char *argv[]) {
    int index;

    for (index = 1; index < argc && *argv[index] == '-'; index++) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            usage();
            exit(EXIT_SUCCESS);
        } else if (strcmp(argv[index], "-v") == 0 || strcmp(argv[index], "--version") == 0) {
            printf("%s\n", VERSION);
            exit(EXIT_SUCCESS);
        } else if (strcmp(argv[index], "-p") == 0 || strcmp(argv[index], "--path") == 0) {
            env->path = argv[++index];
        } else if (strcmp(argv[index], "-n") == 0 || strcmp(argv[index], "--name") == 0) {
            env->trie_name = argv[++index];
        } else {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Unknown option: %s", DEBUG_ARGS_VALUE, argv[index]);
            exit(EXIT_FAILURE);
        }
    }
}

void
decode_command(int argc, char *argv[], ProgEnv * env) {
    int index;

    for (index = 0; index < argc; index++) {
        if (strcmp(argv[index], "add") == 0) {
            ++index;
            index += command_add(argc - index, argv + index, env);
        } else if (strcmp(argv[index], "add-list") == 0) {
            ++index;
            index += command_add_list(argc - index, argv + index, env);
        } else if (strcmp(argv[index], "delete") == 0) {
            ++index;
            index += command_delete(argc - index, argv + index, env);
        } else if (strcmp(argv[index], "delete-list") == 0) {
            ++index;
            index += command_delete_list(argc - index, argv + index, env);
        } else if (strcmp(argv[index], "query") == 0) {
            ++index;
            command_query(argc - index, argv + index, env);
        } else if (strcmp(argv[index], "replace") == 0) {
            ++index;
            command_replace(argc - index, argv + index, env);
        } else if (strcmp(argv[index], "list") == 0) {
            ++index;
            index += command_list(argc - index, argv + index, env);
        } else if (strcmp(argv[index], "exit") == 0) {
            exit(EXIT_SUCCESS);
        } else {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Unknown command: %s", DEBUG_ARGS_VALUE, argv[index]);
            return;
        }
    }
}

int
command_add(int argc, char *argv[], ProgEnv *env) {
    int index = 0;
    while (index < argc) {
        char *str = argv[index++], *delimPos = NULL;
        unsigned int ori_word_len = 0, replace_word_len = 1;
        delimPos = get_word_pair_info(str, &ori_word_len, &replace_word_len);
        if (delimPos != NULL) {
            char *ori_word = (char *) calloc(ori_word_len, sizeof (char));
            if (ori_word == NULL) {
                write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Out of memory", DEBUG_ARGS_VALUE);
                return FALSE;
            }
            char *replace_word = (char *) calloc(replace_word_len, sizeof (char));
            if (replace_word == NULL) {
                write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Out of memory", DEBUG_ARGS_VALUE);
                return FALSE;
            }

            TrieData data;
            AlphaChar word_alpha[MAX_WORD_LEN] = {0};

            snprintf(ori_word, sizeof (ori_word), "%s", str);
            snprintf(replace_word, sizeof (replace_word), "%s", delimPos + 1);

            data = (TrieData) utarray_len(env->replace_words);
            conv_to_alpha(ori_word, word_alpha, N_ELEMENTS(word_alpha), env);

            if (!trie_store(env->trie, word_alpha, data)) {
                write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Failed to add entry '%s' with data %d", DEBUG_ARGS_VALUE, ori_word, data);
            } else {
                word_pair word_data;
                word_data.ori_str = ori_word;
                word_data.replace_str = replace_word;
                utarray_push_back(env->replace_words, &word_data);
                return TRUE;
            }
        } else {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Did not find the delimiter in (%s)", DEBUG_ARGS_VALUE, str);
        }
    }

    return index;
}

int
command_add_list(int argc, char *argv[], ProgEnv * env) {
    FILE *fp;
    int index;
    iconv_t saved_conv;
    char line[MAX_LINE_LEN];
    const char *enc_name, *input_name;

    index = 0;
    enc_name = 0;

    saved_conv = env->to_alpha_conv;
    if (strcmp(argv[0], "-e") == 0 || strcmp(argv[0], "--encoding") == 0) {
        if (++index >= argc) {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"add-list option \"%s\" requires encoding name", DEBUG_ARGS_VALUE, argv[0]);
            return index;
        }
        enc_name = argv[index++];
    }
    if (index >= argc) {
        write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"add-list requires input word list file name", DEBUG_ARGS_VALUE);
        return index;
    }
    input_name = argv[index++];

    if (enc_name) {
        iconv_t conv = iconv_open(ALPHA_ENC, enc_name);
        if ((iconv_t) (-1) == conv) {
            write_log(LOG_NOTICE, "Conversion from \"%s\" to \"%s\" is not supported.", DEBUG_ARGS_VALUE, enc_name, ALPHA_ENC);
            return index;
        }

        env->to_alpha_conv = conv;
    }

    if ((fp = fopen(input_name, "r")) == NULL) {
        write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"add-list: Cannot open input file \"%s\"", DEBUG_ARGS_VALUE, input_name);
        goto exit_iconv_openned;
    }

    while (fgets(line, sizeof (line), fp)) {
        char *argv[] = {line};
        command_add(1, argv, env);
    }

    fclose(fp);

exit_iconv_openned:
    if (enc_name) {
        iconv_close(env->to_alpha_conv);
        env->to_alpha_conv = saved_conv;
    }

    return index;
}

int
command_delete(int argc, char *argv[], ProgEnv * env) {
    int index = 0;

    for (; index < argc; index++) {
        AlphaChar word_alpha[MAX_WORD_LEN];

        conv_to_alpha(argv[index], word_alpha, N_ELEMENTS(word_alpha), env);

        int index;
        if (!trie_retrieve(env->trie, word_alpha, &index)) {
            continue;
        }

        if (!trie_delete(env->trie, word_alpha)) {
            write_log(LOG_NOTICE, "No entry '%s'. Not deleted.", DEBUG_ARGS_VALUE, argv[index]);
        } else {
            word_pair *word_data = (word_pair*) utarray_eltptr(env->replace_words, index);
            free(word_data->ori_str);
            free(word_data->replace_str);
            word_data = NULL;
        }
    }

    return index;
}

int
command_delete_list(int argc, char *argv[], ProgEnv * env) {
    FILE *fp;
    int index;
    iconv_t saved_conv;
    char line[MAX_WORD_LEN];
    const char *enc_name, *input_name;

    index = 0;
    enc_name = 0;

    saved_conv = env->to_alpha_conv;
    if (strcmp(argv[0], "-e") == 0 || strcmp(argv[0], "--encoding") == 0) {
        if (++index >= argc) {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"delete-list option \"%s\" requires encoding name", DEBUG_ARGS_VALUE, argv[0]);
            return index;
        }
        enc_name = argv[index++];
    }
    if (index >= argc) {
        write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"delete-list requires input word list file name", DEBUG_ARGS_VALUE);
        return index;
    }
    input_name = argv[index++];

    if (enc_name) {
        iconv_t conv = iconv_open(ALPHA_ENC, enc_name);
        if ((iconv_t) - 1 == conv) {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Conversion from \"%s\" to \"%s\" is not supported.", DEBUG_ARGS_VALUE, enc_name, ALPHA_ENC);
            return index;
        }

        env->to_alpha_conv = conv;
    }

    if ((fp = fopen(input_name, "r")) == NULL) {
        write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"delete-list: Cannot open input file \"%s\"", DEBUG_ARGS_VALUE, input_name);
        goto exit_iconv_openned;
    }

    while (fgets(line, sizeof (line), fp)) {
        char *argv[] = {line};
        command_delete(1, argv, env);
    }

    fclose(fp);

exit_iconv_openned:
    if (enc_name) {
        iconv_close(env->to_alpha_conv);
        env->to_alpha_conv = saved_conv;
    }

    return index;
}

int
command_query(int argc, char *argv[], ProgEnv *env) {
    if (argc == 0) {
        write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Query: No word specified.", DEBUG_ARGS_VALUE);
        return 0;
    }

    AlphaChar word_alpha[MAX_WORD_LEN] = {0};
    conv_to_alpha(argv[0], word_alpha, N_ELEMENTS(word_alpha), env);

    TrieData data;
    if (trie_retrieve(env->trie, word_alpha, &data)) {
        word_pair *p = (word_pair*) utarray_eltptr(env->replace_words, (int) data);
        if (p == NULL) {
            write_log(LOG_DEBUG, DEBUG_ARGS_FORMAT"Query: Word '%s' found, But the replace str not found int dynamic arrays.", DEBUG_ARGS_VALUE, argv[0], p->replace_str);
        } else {
            write_log(LOG_DEBUG, DEBUG_ARGS_FORMAT"Query: Word '%s' found. The replace str is '%s'", DEBUG_ARGS_VALUE, argv[0], p->replace_str);
        }
    } else {
        write_log(LOG_DEBUG, DEBUG_ARGS_FORMAT"Query: Word '%s' not found.", DEBUG_ARGS_VALUE, argv[0]);
    }

    return 1;
}

Bool
list_enum_func(const AlphaChar *key, TrieData key_data, void *user_data) {
    ProgEnv *env = (ProgEnv *) user_data;
    char key_locale[1024];

    conv_from_alpha(key, key_locale, N_ELEMENTS(key_locale), env);
    printf("%s\t%d\n", key_locale, key_data);

    return TRUE;
}

int
command_list(int argc, char *argv[], ProgEnv * env) {
    trie_enumerate(env->trie, list_enum_func, (void *) env);
    return 0;
}

int
command_replace(int argc, char *argv[], ProgEnv *env) {
    if (argc == 0) {
        write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Replace: No text specified.", DEBUG_ARGS_VALUE);
        return 0;
    }

    char *text = argv[0];

    TrieState *s;
    AlphaChar text_alpha[MAX_TEXT_LEN] = {0};
    const AlphaChar *p, *tmp = text_alpha;
    conv_to_alpha(text, text_alpha, N_ELEMENTS(text_alpha), env);

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
            TrieData index = (int) trie_from_state_get_data(s);
            word_pair *p_word_pair = (word_pair*) utarray_eltptr(env->replace_words, (int) index);
            if (p_word_pair != NULL) {
                int offset = tmp - text_alpha;
                int flag = text_alpha_replace_word(text_alpha, p_word_pair, offset, env);
                tmp = tmp + (flag - 1);
            } else {
                write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Replace:Find word but not found in dynamic[%d] arrays.", DEBUG_ARGS_VALUE, index);
            }
        }
        trie_state_free(s);
        tmp++;
    }

    char text_locale[MAX_TEXT_LEN];
    conv_from_alpha(text_alpha, text_locale, N_ELEMENTS(text_locale), env);
    printf("%s\n", text_locale);
    return 1;
}

int
text_alpha_replace_word(AlphaChar *text_alpha, word_pair *word_pair, int offset, ProgEnv *env) {
    AlphaChar ori_word_alpha[MAX_WORD_LEN] = {0};
    AlphaChar replace_word_alpha[MAX_WORD_LEN] = {0};
    conv_to_alpha(word_pair->ori_str, ori_word_alpha, N_ELEMENTS(ori_word_alpha), env);
    conv_to_alpha(word_pair->replace_str, replace_word_alpha, N_ELEMENTS(replace_word_alpha), env);

    int ori_word_alpha_len = alpha_char_strlen(ori_word_alpha);
    int replace_word_alpha_len = alpha_char_strlen(replace_word_alpha);

    int flag = replace_word_alpha_len - ori_word_alpha_len;

    AlphaChar *p, *q;
    if (flag > 0) {
        int text_alpha_length = alpha_char_strlen(text_alpha);
        if (text_alpha_length == MAX_TEXT_LEN || text_alpha_length + flag > MAX_TEXT_LEN) {
            write_log(LOG_ERROR, DEBUG_ARGS_FORMAT"Array index out of bounds", DEBUG_ARGS_VALUE);
        }
        p = text_alpha + (text_alpha_length + flag - 1);
        q = text_alpha + (text_alpha_length - 1);
        int num = text_alpha_length - (offset + ori_word_alpha_len);
        while (num > 0) {
            *p = *q;
            p--;
            q--;
            num--;
        }
    } else if (flag < 0) {
        p = text_alpha + (offset + replace_word_alpha_len);
        q = text_alpha + (offset + ori_word_alpha_len);
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
    p = replace_word_alpha;
    q = text_alpha + offset;
    while (*p) {
        *q = *p;
        p++;
        q++;
    }
    return replace_word_alpha_len;
}

void
program_exit(ProgEnv * env) {
    if (trie_is_dirty(env->trie)) {
        char file_name[FILE_NAME_LEN];
        snprintf(file_name, sizeof (file_name), "%s/%s.tri", env->path, env->trie_name);
        if (trie_save(env->trie, file_name) != 0) {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Cannot save trie to %s", DEBUG_ARGS_VALUE, file_name);
            return;
        }

        FILE *fp;
        snprintf(file_name, sizeof (file_name), "%s/%s.list", env->path, env->trie_name);
        if ((fp = fopen(file_name, "w")) == NULL) {
            write_log(LOG_NOTICE, DEBUG_ARGS_FORMAT"Cannot open  %s", DEBUG_ARGS_VALUE, file_name);
            return;
        }

        word_pair *word_data = NULL;
        while ((word_data = (word_pair *) utarray_next(env->replace_words, word_data))) {
            char line[MAX_LINE_LEN] = {'\0'};
            snprintf(line, sizeof (line), "%s-%s\n", word_data->ori_str, word_data->replace_str);
            fwrite(line, sizeof (char), strlen(line), fp);
        }

        fclose(fp);
    }

    utarray_free(env->replace_words);
    trie_free(env->trie);
    close_conv(env);
}

void
usage() {
    printf("swf-c - double-array trie manipulator\n");
    printf("Usage: swf-c [OPTION]... TRIE CMD ARG ...\n");
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
}

char *
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

void
write_log(int level, const char * fmt, ...) {
    FILE *fp;
    va_list ap;
    struct tm *tm;
#ifdef DEBUG
    fp = stdout;
#else
    time_t ts;
    ts = time(NULL);
    tm = gmtime(&ts);
    char log_path[FILE_NAME_LEN];
    sprintf(log_path, "datrie%d%d.log", tm->tm_year + 1900, tm->tm_mon + 1);
    fp = fopen(log_path, "a+");
    if (!fp) {
        return;
    }
#endif
    time_t now;
    char buf[64];
    char *c[3] = {"ERROR", "NOTICE", "DEBUG"};
    va_start(ap, fmt);
    now = time(NULL);
    strftime(buf, 64, "[%d %b %H:%M:%S]", gmtime(&now));
    fprintf(fp, "%s %s:", buf, c[level]);
    vfprintf(fp, fmt, ap);
    fprintf(fp, "\n");
    fflush(fp);
    va_end(ap);
#ifndef DEBUG
    fclose(fp);
#endif
    if (level == LOG_ERROR) {
        exit(EXIT_FAILURE);
    }
}
