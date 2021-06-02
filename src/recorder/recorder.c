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
#include "php.h"
#include "ext/standard/php_string.h"

#include <unistd.h>
#include <fcntl.h>

#include "php_xdebug.h"
#include "recorder_private.h"

#include "lib/compat.h"
#include "lib/hash.h"
#include "lib/log.h"
#include "lib/str.h"
#include "lib/var_export_line.h"

ZEND_EXTERN_MODULE_GLOBALS(xdebug);

static FILE *xdebug_recorder_open_file(char **used_fname)
{
	FILE *file;
	char *filename_to_use;
	char *generated_filename = NULL;
	char *output_dir = xdebug_lib_get_output_dir(); /* not duplicated */

	if (!strlen(XINI_RECORDER(recorder_output_name)) ||
		xdebug_format_output_filename(&generated_filename, XINI_RECORDER(recorder_output_name), NULL) <= 0
	) {
		/* Invalid or empty xdebug.recorder_output_name */
		return NULL;
	}

	/* Add a slash if none is present in the output_dir setting */
	output_dir = xdebug_lib_get_output_dir(); /* not duplicated */

	if (IS_SLASH(output_dir[strlen(output_dir) - 1])) {
		filename_to_use = xdebug_sprintf("%s%s", output_dir, generated_filename);
	} else {
		filename_to_use = xdebug_sprintf("%s%c%s", output_dir, DEFAULT_SLASH, generated_filename);
	}

	file = xdebug_fopen(filename_to_use, "w", "xdebug", used_fname);

	if (!file) {
		xdebug_log_diagnose_permissions(XLOG_CHAN_RECORDER, output_dir, generated_filename);
	}

	if (generated_filename) {
		xdfree(generated_filename);
	}
	xdfree(filename_to_use);

	return file;
}

/* Sections */
struct _xdebug_recorder_section {
	size_t   size;
	uint8_t *ptr;
	uint8_t  data[1];
};
typedef struct _xdebug_recorder_section xdebug_recorder_section;

#define REF_LIST_VERSION                       1

#define SECTION_HEADER                         0x01
#define SECTION_HEADER_VERSION                 1
#define SECTION_FILE_REF_LIST                  0x02
#define SECTION_FILE_REF_LIST_VERSION          REF_LIST_VERSION
#define SECTION_FILE                           0x03
#define SECTION_FILE_VERSION                   1
#define SECTION_FUNC_REF_LIST                  0x04
#define SECTION_FUNC_REF_LIST_VERSION          REF_LIST_VERSION
#define SECTION_VAR_REF_LIST                   0x05
#define SECTION_VAR_REF_LIST_VERSION           REF_LIST_VERSION

static inline size_t unum_size(uint64_t value)
{
	size_t   bytes = 0;
	uint64_t x = value;

	do {
		x >>= 7;
		++bytes;
	} while (x);
//printf("UNUM_SIZE(%ld) = %ld\n", value, bytes);
	return bytes;
}

static void add_unum(xdebug_recorder_section *section, uint64_t value)
{
	uint64_t x = value;

//printf("ADD_UNUM(%ld): ", value);
	do {
		*(uint8_t*)section->ptr = x & 0x7FU;
		if (x >>= 7) {
			*(uint8_t*)section->ptr |= 0x80U;
		}
//printf("%02X", *(uint8_t*)section->ptr);
		++section->ptr;
	} while (x);
//printf("\n");
}

static inline size_t string_size(size_t length)
{
	return unum_size(length) + length;
}

/* Reference lists */
static void xdebug_ref_list_dtor(xdebug_ref_list_entry *entry)
{
	xdfree(entry->name);
	xdfree(entry);
}

uint64_t get_func_ref(xdebug_recorder_context *context, const char *name, size_t name_len)
{
	xdebug_ref_list_entry *entry;

	if (xdebug_hash_find(context->func_ref_list, name, name_len, (void*) &entry)) {
		return entry->idx;
	} else {
		entry = xdmalloc(sizeof(xdebug_ref_list_entry));
		entry->idx = context->func_ref_list->size;
		entry->name = xdstrdup(name);
		entry->name_len = name_len;

		xdebug_hash_add(context->func_ref_list, name, name_len, (void*) entry);

		return entry->idx;
	}
}

uint64_t get_var_ref(xdebug_recorder_context *context, const char *name, size_t name_len)
{
	xdebug_ref_list_entry *entry;

	if (xdebug_hash_find(context->var_ref_list, name, name_len, (void*) &entry)) {
		return entry->idx;
	} else {
		entry = xdmalloc(sizeof(xdebug_ref_list_entry));
		entry->idx = context->var_ref_list->size;
		entry->name = xdstrdup(name);
		entry->name_len = name_len;

		xdebug_hash_add(context->var_ref_list, name, name_len, (void*) &entry);

		return entry->idx;
	}
}

/* Data addition */
static void add_string(xdebug_recorder_section *section, size_t length, const char *str)
{
	add_unum(section, length);

	memcpy(section->ptr, str, length);
	section->ptr += length;
}


static void add_data(xdebug_recorder_section *section, size_t length, uint8_t *data)
{
	memcpy(section->ptr, data, length);
	section->ptr += length;
}

static xdebug_recorder_section *section_create(uint8_t type, uint8_t version, size_t size)
{
	size_t len = sizeof(xdebug_recorder_section) + unum_size(type) + unum_size(version) + size + unum_size(0x7F);

	xdebug_recorder_section *tmp = xdmalloc(len);
	tmp->ptr = &tmp->data[0];
	tmp->size = len;

	add_unum(tmp, type);
	add_unum(tmp, version);

	return tmp;
}

static void recorder_write_section(xdebug_recorder_context *context, xdebug_recorder_section *section)
{
	add_unum(section, 0x7F);

	fwrite(section->data, section->size - sizeof(xdebug_recorder_section), 1, context->recorder_file);
	fflush(context->recorder_file);
}

/* The "handler" */
static void *xdebug_recorder_init(void)
{
	xdebug_recorder_context *tmp_recorder_context;
	char *used_fname;

	tmp_recorder_context = xdmalloc(sizeof(xdebug_recorder_context));
	tmp_recorder_context->recorder_file = xdebug_recorder_open_file((char**) &used_fname);
	tmp_recorder_context->recorder_filename = used_fname;
	tmp_recorder_context->file_ref_list = xdebug_hash_alloc(256, (xdebug_hash_dtor_t) xdebug_ref_list_dtor);
	tmp_recorder_context->func_ref_list = xdebug_hash_alloc(256, (xdebug_hash_dtor_t) xdebug_ref_list_dtor);
	tmp_recorder_context->var_ref_list = xdebug_hash_alloc(256, (xdebug_hash_dtor_t) xdebug_ref_list_dtor);

	return tmp_recorder_context->recorder_file ? tmp_recorder_context : NULL;
}

static void xdebug_recorder_deinit(void *ctxt)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;

	fclose(context->recorder_file);
	context->recorder_file = NULL;
	xdfree(context->recorder_filename);
	xdebug_hash_destroy(context->file_ref_list);
	xdebug_hash_destroy(context->func_ref_list);
	xdebug_hash_destroy(context->var_ref_list);

	xdfree(context);
}

static void xdebug_recorder_write_header(void *ctxt)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;
	xdebug_recorder_section *section;
	size_t                   data_size = 0;

	fwrite("XDEBUG", strlen("XDEBUG"), 1, context->recorder_file);

	data_size += unum_size(XDEBUG_BUILD_ID);
	data_size += unum_size(PHP_VERSION_ID);
	data_size += unum_size(1); // ZTS flag
	data_size += unum_size(PHP_DEBUG);

	section = section_create(SECTION_HEADER, SECTION_HEADER_VERSION, sizeof(uint32_t) + 5*sizeof(uint8_t));
	add_unum(section, XDEBUG_BUILD_ID);
	add_unum(section, PHP_VERSION_ID);
#ifdef ZTS
	add_unum(section, 1);
#else
	add_unum(section, 0);
#endif
	add_unum(section, PHP_DEBUG);

	recorder_write_section(context, section);
}



static void ref_list_size_counter(void *vs, xdebug_hash_element *he)
{
	size_t                *size = (size_t*) vs;
	xdebug_ref_list_entry *entry = (xdebug_ref_list_entry*) he->ptr;

	*size += unum_size(entry->idx) + string_size(entry->name_len);
}

static void ref_list_adder(void *s, xdebug_hash_element *he)
{
	xdebug_recorder_section *section = (xdebug_recorder_section*) s;
	xdebug_ref_list_entry *entry = (xdebug_ref_list_entry*) he->ptr;

	add_unum(section, entry->idx);
	add_string(section, entry->name_len, entry->name);
}


static void add_file_ref_list(xdebug_recorder_context *context)
{
	xdebug_recorder_section *section;
	size_t                   array_size = unum_size(context->file_ref_list->size);

	xdebug_hash_apply(context->file_ref_list, (void*) &array_size, ref_list_size_counter);

	section = section_create(SECTION_FILE_REF_LIST, SECTION_FILE_REF_LIST_VERSION, array_size);

	add_unum(section, context->file_ref_list->size);
	xdebug_hash_apply(context->file_ref_list, (void*) section, ref_list_adder);

	recorder_write_section(context, section);
}

static void add_func_ref_list(xdebug_recorder_context *context)
{
	xdebug_recorder_section *section;
	size_t                   array_size = unum_size(context->func_ref_list->size);

	xdebug_hash_apply(context->func_ref_list, (void*) &array_size, ref_list_size_counter);

	section = section_create(SECTION_FUNC_REF_LIST, SECTION_FUNC_REF_LIST_VERSION, array_size);

	add_unum(section, context->func_ref_list->size);
	xdebug_hash_apply(context->func_ref_list, (void*) section, ref_list_adder);

	recorder_write_section(context, section);
}

static void add_var_ref_list(xdebug_recorder_context *context)
{
	xdebug_recorder_section *section;
	size_t                   array_size = unum_size(context->var_ref_list->size);

	xdebug_hash_apply(context->var_ref_list, (void*) &array_size, ref_list_size_counter);

	section = section_create(SECTION_VAR_REF_LIST, SECTION_VAR_REF_LIST_VERSION, array_size);

	add_unum(section, context->var_ref_list->size);
	xdebug_hash_apply(context->var_ref_list, (void*) section, ref_list_adder);

	recorder_write_section(context, section);
}

static void xdebug_recorder_write_footer(void *ctxt)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;

	add_file_ref_list(context);
	add_func_ref_list(context);
	add_var_ref_list(context);

	fflush(context->recorder_file);
}

static char *xdebug_recorder_get_filename(void *ctxt)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;

	return context->recorder_filename;
}

static void xdebug_recorder_function_entry(void *ctxt, function_stack_entry *fse, int function_nr)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;
	char *tmp_name = xdebug_show_fname(fse->function, XDEBUG_SHOW_FNAME_TODO);
	get_func_ref(context, tmp_name, strlen(tmp_name));
	xdfree(tmp_name);

//	fflush(context->recorder_file);
}

static void xdebug_recorder_function_exit(void *ctxt, function_stack_entry *fse, int function_nr)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;

	fflush(context->recorder_file);
}

static void xdebug_recorder_function_return_value(void *ctxt, function_stack_entry *fse, int function_nr, zval *return_value)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;

	fflush(context->recorder_file);
}

void xdebug_recorder_generator_return_value(void *ctxt, function_stack_entry *fse, int function_nr, zend_generator *generator)
{
	xdebug_recorder_context *context = (xdebug_recorder_context*) ctxt;

	fflush(context->recorder_file);
}

static uint64_t recorder_add_file_to_index(xdebug_recorder_context *context, const char *filename)
{
	xdebug_ref_list_entry *tmp = xdmalloc(sizeof(xdebug_ref_list_entry));

	tmp->idx = context->file_ref_list->size;
	tmp->name = xdstrdup(filename);
	tmp->name_len = strlen(filename);

	xdebug_hash_add(context->file_ref_list, tmp->name, tmp->name_len, (void*) tmp);

	return tmp->idx;
}

void xdebug_recorder_add_file(xdebug_recorder_context *context, const char *filename)
{
	struct stat sinfo;
	xdebug_recorder_section *section;
	uint64_t file_index;
	uint32_t file_size;
	uint8_t flags = 0x00;
	int fp = open(filename, O_RDONLY);
	uint8_t buffer[1024];
	ssize_t read_data;

	if (!fp) {
		return;
	}

	if (fstat(fp, &sinfo) != 0) {
		close(fp);
		return;
	}

	file_index = recorder_add_file_to_index(context, filename);
	file_size  = sinfo.st_size;

	/* file_index + flags + name + file size + bytes */
	section = section_create(
		SECTION_FILE, SECTION_FILE_VERSION,
		unum_size(file_index) + unum_size(flags) + unum_size(file_size) + file_size);
	add_unum(section, file_index);
	add_unum(section, flags);
	add_unum(section, file_size);

	do {
		read_data = read(fp, buffer, sizeof(buffer));
//		printf("READ: %ld\n", read_data);
		add_data(section, read_data, buffer);
	} while (read_data > 0);

	close(fp);

	recorder_write_section(context, section);
}

/* Plumbing */
static char* xdebug_start_recorder(void)
{
	if (XG_RECORDER(recorder_context)) {
		return NULL;
	}

	XG_RECORDER(recorder_context) = xdebug_recorder_init();

	if (!XG_RECORDER(recorder_context)) {
		return NULL;
	}

	xdebug_recorder_write_header(XG_RECORDER(recorder_context));
	return xdstrdup(xdebug_recorder_get_filename(XG_RECORDER(recorder_context)));
}

static void xdebug_stop_recorder(void)
{
	if (!XG_RECORDER(recorder_context)) {
		return;
	}

	xdebug_recorder_write_footer(XG_RECORDER(recorder_context));
	xdebug_recorder_deinit(XG_RECORDER(recorder_context));
	XG_RECORDER(recorder_context) = NULL;
}

char *xdebug_get_recorder_filename(void)
{
	if (!XG_RECORDER(recorder_context)) {
		return NULL;
	}

	return xdebug_recorder_get_filename(XG_RECORDER(recorder_context));
}


/* Extension hooks */

void xdebug_init_recorder_globals(xdebug_recorder_globals_t *xg)
{
	xg->recorder_context = NULL;
}

void xdebug_recorder_minit(INIT_FUNC_ARGS)
{
}

void xdebug_recorder_register_constants(INIT_FUNC_ARGS)
{
}

void xdebug_recorder_rinit(void)
{
	XG_RECORDER(recorder_context) = NULL;
}

void xdebug_recorder_post_deactivate(void)
{
	if (XG_RECORDER(recorder_context)) {
		xdebug_stop_recorder();
	}

	XG_RECORDER(recorder_context) = NULL;
}

void xdebug_recorder_init_if_requested(void)
{
	if (xdebug_lib_start_with_request(XDEBUG_MODE_RECORDER) || xdebug_lib_start_with_trigger(XDEBUG_MODE_RECORDER, NULL)) {
		/* In case we do an auto-trace we are not interested in the return
		 * value, but we still have to free it. */
		xdfree(xdebug_start_recorder());
	}
}

void xdebug_recorder_execute_ex(int function_nr, function_stack_entry *fse)
{
	if (!XG_RECORDER(recorder_context)) {
		return;
	}

	xdebug_recorder_function_entry(XG_RECORDER(recorder_context), fse, function_nr);
}

void xdebug_recorder_execute_ex_end(int function_nr, function_stack_entry *fse, zend_execute_data *execute_data)
{
	zend_op_array *op_array;

	if (!XG_RECORDER(recorder_context)) {
		return;
	}

	xdebug_recorder_function_exit(XG_RECORDER(recorder_context), fse, function_nr);

	op_array = &(execute_data->func->op_array);

	if (!execute_data || !execute_data->return_value) {
		return;
	}

	if (op_array->fn_flags & ZEND_ACC_GENERATOR) {
		xdebug_recorder_generator_return_value(XG_RECORDER(recorder_context), fse, function_nr, (zend_generator*) execute_data->return_value);
	} else {
		xdebug_recorder_function_return_value(XG_RECORDER(recorder_context), fse, function_nr, execute_data->return_value);
	}
}

int xdebug_recorder_execute_internal(int function_nr, function_stack_entry *fse)
{
	if (!XG_RECORDER(recorder_context)) {
		return 0;
	}

	if (fse->function.type != XFUNC_ZEND_PASS) {
		xdebug_recorder_function_entry(XG_RECORDER(recorder_context), fse, function_nr);
		return 1;
	}

	return 0;
}

void xdebug_recorder_execute_internal_end(int function_nr, function_stack_entry *fse, zval *return_value)
{
	if (!XG_RECORDER(recorder_context)) {
		return;
	}

	if (fse->function.type != XFUNC_ZEND_PASS) {
		xdebug_recorder_function_exit(XG_RECORDER(recorder_context), fse, function_nr);
	}

	/* Store return value in the trace file */
	if (fse->function.type != XFUNC_ZEND_PASS && return_value) {
		xdebug_recorder_function_return_value(XG_RECORDER(recorder_context), fse, function_nr, return_value);
	}
}

void xdebug_recorder_compile_file(zend_file_handle *handle)
{
	if (!XG_RECORDER(recorder_context)) {
		return;
	}

	xdebug_recorder_add_file(XG_RECORDER(recorder_context), handle->filename);
}

#if 0
void xdebug_tracing_save_trace_context(void **original_trace_context)
{
	*original_trace_context = XG_TRACE(trace_context);
	XG_TRACE(trace_context) = NULL;
}

void xdebug_tracing_restore_trace_context(void *original_trace_context)
{
	XG_TRACE(trace_context) = original_trace_context;
}
#endif
