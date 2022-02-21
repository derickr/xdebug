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

#include "php.h"
#include "ext/standard/php_string.h"
#include "Zend/zend_closures.h"
#if PHP_VERSION_ID >= 80100
# include "zend_enum.h"
#endif

#include "php_xdebug.h"

#include "section.h"
#include "var_export_binary.h"

static void xdebug_var_export_binary(xdebug_recorder_section *section, zval **struc, xdebug_var_export_options *options);

ZEND_EXTERN_MODULE_GLOBALS(xdebug)

static int xdebug_array_element_export_binary(zval *zv_nptr, zend_ulong index_key, zend_string *hash_key, xdebug_recorder_section *section, xdebug_var_export_options *options)
{
	zval **zv = &zv_nptr;

	xdebug_recorder_add_unum(section, HASH_KEY_IS_NUMERIC(hash_key));
	if (HASH_KEY_IS_NUMERIC(hash_key)) { /* numeric key */
		xdebug_recorder_add_unum(section, index_key);
	} else { /* string key */
		xdebug_recorder_add_string(section, ZSTR_LEN(hash_key), ZSTR_VAL(hash_key));
	}
	xdebug_var_export_binary(section, zv, options);

	return 0;
}

#if 0
static int xdebug_object_element_export_text_ansi(zval *object, zval *zv_nptr, zend_ulong index_key, zend_string *hash_key, int level, int mode, xdebug_str *str, int debug_zval, xdebug_var_export_options *options)
{
	zval **zv = &zv_nptr;

	if (options->runtime[level].current_element_nr >= options->runtime[level].start_element_nr &&
		options->runtime[level].current_element_nr < options->runtime[level].end_element_nr)
	{
		xdebug_str_add_fmt(str, "%*s", (level * 2), "");

		if (!HASH_KEY_IS_NUMERIC(hash_key)) {
			char       *class_name;
			xdebug_str *property_name;
			const char *modifier;
			xdebug_str *property_type = NULL;

#if PHP_VERSION_ID >= 70400
			property_type = xdebug_get_property_type(object, zv_nptr);
#endif

			property_name = xdebug_get_property_info((char*) HASH_APPLY_KEY_VAL(hash_key), HASH_APPLY_KEY_LEN(hash_key), &modifier, &class_name);
			xdebug_str_add_fmt(
				str, "%s%s%s%s%s%s%s $",
				ANSI_COLOR_MODIFIER, ANSI_COLOR_BOLD, modifier, ANSI_COLOR_BOLD_OFF, property_type ? " " : "", property_type ? property_type->d : "", ANSI_COLOR_RESET
			);
			xdebug_str_add_str(str, property_name);
			xdebug_str_add_fmt(str, " %s=>%s\n", ANSI_COLOR_POINTER, ANSI_COLOR_RESET);

			if (property_type) {
				xdebug_str_free(property_type);
			}
			xdebug_str_free(property_name);
			xdfree(class_name);
		} else {
			xdebug_str_add_fmt(str, "%s%spublic%s%s ${" XDEBUG_INT_FMT "} %s=>%s\n",
				ANSI_COLOR_MODIFIER, ANSI_COLOR_BOLD, ANSI_COLOR_BOLD_OFF, ANSI_COLOR_RESET,
				index_key, ANSI_COLOR_POINTER, ANSI_COLOR_RESET
			);
		}
		xdebug_var_export_text_ansi(zv, str, mode, level + 1, debug_zval, options);
	}
	if (options->runtime[level].current_element_nr == options->runtime[level].end_element_nr) {
		xdebug_str_add_fmt(str, "\n%*s(more elements)...\n", (level * 2), "");
	}
	options->runtime[level].current_element_nr++;
	return 0;
}

static void handle_closure(xdebug_str *str, zval *obj, int level, int mode)
{
	const zend_function *closure_function;

	if (Z_TYPE_P(obj) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(obj), zend_ce_closure)) {
		return;
	}

#if PHP_VERSION_ID >= 80000
	closure_function = zend_get_closure_method_def(Z_OBJ_P(obj));
#else
	closure_function = zend_get_closure_method_def(obj);
#endif

	xdebug_str_add_fmt(
		str, "%*s%s%svirtual%s $closure =>\n%*s\"",
		(level * 4) - 2, "",
		ANSI_COLOR_MODIFIER, ANSI_COLOR_BOLD, ANSI_COLOR_RESET,
		(level * 4) - 2, ""
	);

	if (closure_function->common.scope) {
		if (closure_function->common.fn_flags & ZEND_ACC_STATIC) {
			xdebug_str_add_fmt(str, "%s", ANSI_COLOR_OBJECT);
			xdebug_str_add_zstr(str, closure_function->common.scope->name);
			xdebug_str_add_fmt(str, "%s::", ANSI_COLOR_RESET);
		} else {
			xdebug_str_add_fmt(str, "%s$this%s->", ANSI_COLOR_OBJECT, ANSI_COLOR_RESET);
		}
	}

	xdebug_str_add_fmt(str, "%s", ANSI_COLOR_STRING);
	xdebug_str_add_zstr(str, closure_function->common.function_name);
	xdebug_str_add_fmt(str, "%s\"\n", ANSI_COLOR_RESET);
}
#endif

static void xdebug_var_export_binary(xdebug_recorder_section *section, zval **struc, xdebug_var_export_options *options)
{
	HashTable *myht;
#if 0
	char*     tmp_str;
	int       tmp_len;
#if PHP_VERSION_ID < 70400
	int       is_temp;
#endif
#endif
	zend_ulong num;
	zend_string *key;
	zval *val;
	zval *tmpz;
	int   z_type;

	if (!struc || !(*struc)) {
		return;
	}

	z_type = Z_TYPE_P(*struc);

	if (z_type == IS_INDIRECT) {
		tmpz = Z_INDIRECT_P(*struc);
		struc = &tmpz;
		z_type = Z_TYPE_P(*struc);
	}
	if (z_type == IS_REFERENCE) {
		tmpz = &((*struc)->value.ref->val);
		struc = &tmpz;
		z_type = Z_TYPE_P(*struc);
	}

	fprintf(stderr, "TYPE: %0d\n", z_type);
	xdebug_recorder_add_unum(section, (uint64_t) z_type);

	switch (z_type) {
		case IS_TRUE:
		case IS_FALSE:
		case IS_NULL:
			break;

		case IS_LONG:
			xdebug_recorder_add_unum(section, (uint64_t) Z_LVAL_P(*struc));
			break;

		case IS_DOUBLE:
			xdebug_recorder_add_data(section, sizeof(double), (uint8_t*) &Z_DVAL_P(*struc));
			break;

		case IS_STRING:
			xdebug_recorder_add_string(section, Z_STRLEN_P(*struc), Z_STRVAL_P(*struc));
			break;

		case IS_ARRAY:
			myht = Z_ARRVAL_P(*struc);

			if (!xdebug_zend_hash_is_recursive(myht)) {
				xdebug_recorder_add_unum(section, myht->nNumOfElements);

				xdebug_zend_hash_apply_protection_begin(myht);

				ZEND_HASH_FOREACH_KEY_VAL_IND(myht, num, key, val) {
					xdebug_array_element_export_binary(val, num, key, section, options);
				} ZEND_HASH_FOREACH_END();

				xdebug_zend_hash_apply_protection_end(myht);
#if 0
			} else {
				xdebug_str_add_fmt(str, "&%sarray%s", ANSI_COLOR_BOLD, ANSI_COLOR_BOLD_OFF);
#endif
			}
			break;

#if 0
		case IS_OBJECT: {
#if PHP_VERSION_ID >= 80100
			zend_class_entry *ce = Z_OBJCE_P(*struc);
			if (ce->ce_flags & ZEND_ACC_ENUM) {
				zval *case_name_zval = zend_enum_fetch_case_name(Z_OBJ_P(*struc));
				xdebug_str_add_fmt(
					str, "%senum%s %s%s%s::%s%s%s",
					ANSI_COLOR_BOLD, ANSI_COLOR_BOLD_OFF,
					ANSI_COLOR_OBJECT, ZSTR_VAL(ce->name), ANSI_COLOR_RESET,
					ANSI_COLOR_STRING, Z_STRVAL_P(case_name_zval), ANSI_COLOR_RESET
				);
				if (ce->enum_backing_type != IS_UNDEF) {
					zval *value = zend_enum_fetch_case_value(Z_OBJ_P(*struc));

					if (ce->enum_backing_type == IS_LONG) {
						xdebug_str_add_fmt(
							str, " : %s%s%s(%s%d%s)",
							ANSI_COLOR_BOLD, "int", ANSI_COLOR_RESET,
							ANSI_COLOR_LONG, Z_LVAL_P(value), ANSI_COLOR_RESET
						);
					}

					if (ce->enum_backing_type == IS_STRING) {
						xdebug_str_add_fmt(
							str, " : %s%s%s(%s\"%s\"%s)",
							ANSI_COLOR_BOLD, "string", ANSI_COLOR_RESET,
							ANSI_COLOR_STRING, Z_STRVAL_P(value), ANSI_COLOR_RESET
						);
					}
				}
				xdebug_str_addc(str, ';');
				break;
			}
#endif

#if PHP_VERSION_ID >= 70400
			myht = xdebug_objdebug_pp(struc);
#else
			myht = xdebug_objdebug_pp(struc, &is_temp);
#endif

			if (!myht || !xdebug_zend_hash_is_recursive(myht)) {
				xdebug_str_add_fmt(
					str, "%sclass%s %s%s%s#%d (%s%d%s) {\n",
					ANSI_COLOR_BOLD, ANSI_COLOR_BOLD_OFF,
					ANSI_COLOR_OBJECT, STR_NAME_VAL(Z_OBJCE_P(*struc)->name), ANSI_COLOR_RESET,
					Z_OBJ_HANDLE_P(*struc),
					ANSI_COLOR_LONG, myht ? myht->nNumOfElements : 0, ANSI_COLOR_RESET
				);

				handle_closure(str, *struc, level, mode);

				if (myht && (level <= options->max_depth)) {
					options->runtime[level].current_element_nr = 0;
					options->runtime[level].start_element_nr = 0;
					options->runtime[level].end_element_nr = options->max_children;

					xdebug_zend_hash_apply_protection_begin(myht);

					ZEND_HASH_FOREACH_KEY_VAL(myht, num, key, val) {
						xdebug_object_element_export_text_ansi(*struc, val, num, key, level, mode, str, debug_zval, options);
					} ZEND_HASH_FOREACH_END();

					xdebug_zend_hash_apply_protection_end(myht);
				} else {
					xdebug_str_add_fmt(str, "%*s...\n", (level * 2), "");
				}
				xdebug_str_add_fmt(str, "%*s}", (level * 2) - 2, "");
			} else {
				xdebug_str_add_fmt(str, "%*s...\n", (level * 2), "");
			}
#if PHP_VERSION_ID >= 70400
			zend_release_properties(myht);
#else
			xdebug_var_maybe_destroy_ht(myht, is_temp);
#endif
			break;
		}

		case IS_RESOURCE: {
			char *type_name;

			type_name = (char *) zend_rsrc_list_get_rsrc_type(Z_RES_P(*struc));
			xdebug_str_add_fmt(
				str, "%sresource%s(%s%ld%s) of type (%s)",
				ANSI_COLOR_BOLD, ANSI_COLOR_BOLD_OFF,
				ANSI_COLOR_RESOURCE, Z_RES_P(*struc)->handle, ANSI_COLOR_RESET, type_name ? type_name : "Unknown"
			);
			break;
		}
#endif
		default:
			fprintf(stderr, "Unknown type: %d\n", z_type);
			break;
	}
}

void xdebug_get_zval_binary(xdebug_recorder_section *section, zval *val, xdebug_var_export_options *options)
{
	int default_options = 0;

	if (!options) {
		options = xdebug_var_export_options_from_ini();
		default_options = 1;
	}

	xdebug_var_export_binary(section, &val, options);

	if (default_options) {
		xdfree(options->runtime);
		xdfree(options);
	}
}
