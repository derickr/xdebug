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
	xdebug_hash *file_list;
};

typedef struct _xdebug_recorder_context xdebug_recorder_context;

#endif
