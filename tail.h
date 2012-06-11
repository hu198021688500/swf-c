/* 
 * File:   tail.h
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:17
 */

#ifndef TAIL_H
#define	TAIL_H

#include "triedefs.h"

typedef struct _Tail Tail;

Tail * tail_new();

Tail * tail_fread(FILE *file);

void tail_free(Tail *t);

int tail_fwrite(const Tail *t, FILE *file);

const TrieChar * tail_get_suffix(const Tail *t, TrieIndex index);

Bool tail_set_suffix(Tail *t, TrieIndex index, const TrieChar *suffix);

TrieIndex tail_add_suffix(Tail *t, const TrieChar *suffix);

TrieData tail_get_data(const Tail *t, TrieIndex index);

Bool tail_set_data(Tail *t, TrieIndex index, TrieData data);

void tail_delete(Tail *t, TrieIndex index);

int tail_walk_str(const Tail *t, TrieIndex s, short *suffix_idx, const TrieChar *str, int len);

Bool tail_walk_char(const Tail *t, TrieIndex s, short *suffix_idx, TrieChar c);

#define tail_is_walkable_char(t,s,suffix_idx,c) (tail_get_suffix ((t), (s)) [suffix_idx] == (c))

#endif	/* TAIL_H */

