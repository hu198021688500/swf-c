/* 
 * File:   trie.h
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:18
 */

#ifndef TRIE_H
#define	TRIE_H

#include "triedefs.h"
#include "alpha-map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Trie Trie;

typedef Bool(*TrieEnumFunc)     (const AlphaChar *key, TrieData key_data, void *user_data);

typedef struct _TrieState TrieState;

Trie * trie_new(const AlphaMap *alpha_map);

Trie * trie_new_from_file(const char *path);

Trie * trie_fread(FILE *file);

void trie_free(Trie *trie);

int trie_save(Trie *trie, const char *path);

int trie_fwrite(Trie *trie, FILE *file);

Bool trie_is_dirty(const Trie *trie);

Bool trie_retrieve(const Trie *trie, const AlphaChar *key, TrieData *o_data);

Bool trie_get_state_data_by_key(const Trie *trie, const AlphaChar *key, TrieData *o_data);

Bool trie_store(Trie *trie, const AlphaChar *key, TrieData data);

Bool trie_store_if_absent(Trie *trie, const AlphaChar *key, TrieData data);

Bool trie_delete(Trie *trie, const AlphaChar *key);

Bool trie_enumerate(const Trie *trie, TrieEnumFunc enum_func, void *user_data);

TrieState * trie_root(const Trie *trie);

TrieState * trie_state_clone(const TrieState *s);

void trie_state_copy(TrieState *dst, const TrieState *src);

void trie_state_free(TrieState *s);

void trie_state_rewind(TrieState *s);

Bool trie_state_walk(TrieState *s, AlphaChar c);

Bool trie_state_is_walkable(const TrieState *s, AlphaChar c);

#define trie_state_is_terminal(s)       trie_state_is_walkable((s),TRIE_CHAR_TERM)

Bool trie_state_is_single(const TrieState *s);

#define trie_state_is_leaf(s)           (trie_state_is_single(s) && trie_state_is_terminal(s))

TrieData trie_state_get_data(const TrieState *s);

TrieData trie_from_state_get_data(const TrieState *s);

#ifdef __cplusplus
}
#endif

#endif	/* TRIE_H */