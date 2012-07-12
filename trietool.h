/*
 * File:   trietool.h
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:23
 */

#ifndef TRIETOOL_H
#define	TRIETOOL_H

#include <iconv.h>
#include <trie.h>

#include "utarray.h"

typedef struct {
    char *ori_str;
    char *replace_str;
} word_pair;

typedef struct {
    Trie *trie;
    const char *path;
    const char *trie_name;
    iconv_t to_alpha_conv;
    iconv_t from_alpha_conv;
    UT_array *replace_words;
} ProgEnv;

#ifdef	__cplusplus
extern "C" {
#endif

void
init_program_env(int argc, char **argv, ProgEnv *env);
void
program_exit(ProgEnv *env);

void
init_sock_accept_env(ProgEnv *env);

char *
get_word_pair_info(char *str, unsigned *ori_word_len, unsigned *replace_word_len);
int
text_alpha_replace_word(AlphaChar *text_alpha, word_pair *word_pair, int offset, ProgEnv *env);

void
init_conv(ProgEnv *env);
size_t
conv_to_alpha(const char *in, AlphaChar *out, size_t out_size, ProgEnv *env);
size_t
conv_from_alpha(const AlphaChar *in, char *out, size_t out_size, ProgEnv *env);
void
lose_conv(ProgEnv *env);

void
prepare_trie(ProgEnv *env);

void
decode_switch(ProgEnv * env, int argc, char *argv[]);
void
decode_command(int argc, char *argv[], ProgEnv *env);

int
command_add(int argc, char *argv[], ProgEnv *env);
int
command_add_list(int argc, char *argv[], ProgEnv *env);
int
command_delete(int argc, char *argv[], ProgEnv *env);
int
command_delete_list(int argc, char *argv[], ProgEnv *env);
int
command_query(int argc, char *argv[], ProgEnv *env);
int
command_list(int argc, char *argv[], ProgEnv *env);
int
command_replace(int argc, char *argv[], ProgEnv *env);

void
usage();
char *
string_trim(char *s);
void
write_log(int level, const char * fmt, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* TRIETOOL_H */