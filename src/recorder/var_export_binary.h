/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2021-2021 Derick Rethans                               |
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

#ifndef __HAVE_XDEBUG_RECORDER_VAR_EXPORT_BINARY_H__
#define __HAVE_XDEBUG_RECORDER_VAR_EXPORT_BINARY_H__

#include "section.h"
#include "lib/var.h"

void xdebug_get_zval_binary(xdebug_recorder_section *section, zval *val, xdebug_var_export_options *options);

#endif
