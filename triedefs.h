/* 
 * File:   triedefs.h
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:18
 */

#ifndef TRIEDEFS_H
#define	TRIEDEFS_H

#include "typedefs.h"

typedef uint32 AlphaChar;

#define ALPHA_CHAR_ERROR   (~(AlphaChar)0)

typedef unsigned char TrieChar;

#define TRIE_CHAR_TERM  '\0'

#define TRIE_CHAR_MAX   255

typedef int32 TrieIndex;

#define TRIE_INDEX_ERROR  0

#define TRIE_INDEX_MAX  0x7fffffff

typedef int32 TrieData;

#define TRIE_DATA_ERROR -1

#endif	/* TRIEDEFS_H */

