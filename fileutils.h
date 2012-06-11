/* 
 * File:   fileutils.h
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:15
 */

#ifndef FILEUTILS_H
#define	FILEUTILS_H

#include <stdio.h>
#include "typedefs.h"

Bool file_read_int32(FILE *file, int32 *o_val);
Bool file_write_int32(FILE *file, int32 val);

Bool file_read_int16(FILE *file, int16 *o_val);
Bool file_write_int16(FILE *file, int16 val);

Bool file_read_int8(FILE *file, int8 *o_val);
Bool file_write_int8(FILE *file, int8 val);

Bool file_read_chars(FILE *file, char *buff, int len);
Bool file_write_chars(FILE *file, const char *buff, int len);

#endif	/* FILEUTILS_H */

