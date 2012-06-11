/* 
 * File:   alpha-map.h
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:07
 */

#ifndef ALPHA_MAP_H
#define	ALPHA_MAP_H

#include <stdio.h>

#include "typedefs.h"
#include "triedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AlphaMap AlphaMap;

AlphaMap * alpha_map_new();

AlphaMap * alpha_map_clone(const AlphaMap *a_map);

void alpha_map_free(AlphaMap *alpha_map);

int alpha_map_add_range(AlphaMap *alpha_map, AlphaChar begin, AlphaChar end);

int alpha_char_strlen(const AlphaChar *str);

#ifdef	__cplusplus
}
#endif

#endif	/* ALPHA_MAP_H */

