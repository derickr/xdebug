/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2002-2021 Derick Rethans                               |
   +----------------------------------------------------------------------+
   | This source file is subject to version 1.01 of the Xdebug license,   |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | https://xdebug.org/license.php                                       |
   | If you did not receive a copy of the Xdebug license and are unable   |
   | to obtain it through the world-wide-web, please send a note to       |
   | derick@xdebug.org so we can mail you a copy immediately.             |
   +----------------------------------------------------------------------+
 */
#ifndef __XDEBUG_RECORDER_PRIVATE_H__
#define __XDEBUG_RECORDER_PRIVATE_H__

#include "recorder.h"
#include "lib/hash.h"

#define XG_RECORDER(v)    (XG(globals.recorder.v))
#define XINI_RECORDER(v)  (XG(settings.recorder.v))

struct _xdebug_recorder_context
{
	FILE        *recorder_file;
	char        *recorder_filename;
	xdebug_hash *file_ref_list;
	xdebug_hash *func_ref_list;
	xdebug_hash *var_ref_list;
};

struct _xdebug_ref_list_entry
{
	uint64_t  idx;
	char     *name;
	size_t    name_len;
};

typedef struct _xdebug_recorder_context xdebug_recorder_context;
typedef struct _xdebug_ref_list_entry xdebug_ref_list_entry;

#endif
