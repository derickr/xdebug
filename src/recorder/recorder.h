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
#ifndef XDEBUG_RECORDER_H
#define XDEBUG_RECORDER_H

#include "php.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#include "zend_generators.h"
#pragma GCC diagnostic pop

typedef struct _xdebug_recorder_globals_t {
	void *recorder_context;
} xdebug_recorder_globals_t;

typedef struct _xdebug_recorder_settings_t {
	char         *recorder_output_name;
} xdebug_recorder_settings_t;

void xdebug_init_recorder_globals(xdebug_recorder_globals_t *xg);
void xdebug_recorder_minit(INIT_FUNC_ARGS);
void xdebug_recorder_rinit(void);
void xdebug_recorder_post_deactivate(void);
void xdebug_recorder_register_constants(INIT_FUNC_ARGS);

void xdebug_recorder_init_if_requested(void);
void xdebug_recorder_execute_ex(int function_nr, function_stack_entry *fse);
void xdebug_recorder_execute_ex_end(int function_nr, function_stack_entry *fse, zend_execute_data *execute_data);
int xdebug_recorder_execute_internal(int function_nr, function_stack_entry *fse);
void xdebug_recorder_execute_internal_end(int function_nr, function_stack_entry *fse, zval *return_value);

#if 0
void xdebug_tracing_save_trace_context(void **old_trace_context);
void xdebug_tracing_restore_trace_context(void *old_trace_context);

char* xdebug_return_trace_stack_retval(function_stack_entry* i, int fnr, zval* retval);
char* xdebug_return_trace_stack_generator_retval(function_stack_entry* i, zend_generator* generator);
char* xdebug_return_trace_assignment(function_stack_entry *i, char *varname, zval *retval, char *op, char *file, int fileno);
#endif

void xdebug_recorder_function_begin(function_stack_entry *fse, int function_nr);
void xdebug_recorder_function_end(function_stack_entry *fse, int function_nr);

char *xdebug_get_recorder_filename(void);

void xdebug_recorder_compile_file(zend_file_handle *handle);
#endif
