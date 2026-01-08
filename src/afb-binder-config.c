/*
 * Copyright (C) 2015-2026 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 */

#include "binder-settings.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#include <json-c/json.h>

#include <rp-utils/rp-jsonc.h>
#include <rp-utils/rp-expand-vars.h>
#include <rp-utils/rp-jsonc-expand.h>
#include <rp-utils/rp-jsonc-path.h>
#include <rp-utils/rp-path-search.h>
#include <rp-utils/rp-yaml.h>

#include <libafb/misc/afb-verbose.h>

#include "afb-binder-config.h"

/**
 * callback data for expanding references
 */
struct expref
{
	/** found error code */
	int error_code;

	/** target is the resulting object */
	struct json_object *target;

	/** accept directories */
	int accept_dirs;

	/** count of read files */
	int count;

	/** found error code when expanding */
	int expand_error_code;

	/** merge option */
	enum rp_jsonc_merge_option merge_option;

	/** path of the reference object */
	rp_jsonc_expand_path_t expand_path;

	/** for searching entries */
	rp_path_search_t *pathsearch;

	/** the searched item */
	const char *searched_item;

	/** the length of searched item */
	size_t searched_item_length;

	/** found file count when searching entries */
	int found_file;

	/** found directory count when searching entries */
	int found_directory;

	/** path to a file or directory */
	char filename[PATH_MAX + 1];
};

/**
 * Emits an error for a given object of within path of ref
 *
 * @param object the object leading to an error
 * @param path   path to the expanded reference or NULL
 * @param format the message as in printf
 * @param ...    argument of the printf like message
 */
static void error_at_object(struct json_object *object, rp_jsonc_expand_path_t expand_path, const char *format, ...)
{
	char *jpath;
	char *msg = NULL;
	va_list ap;
	int rc;

	/* string for the message */
	va_start(ap, format);
	rc = vasprintf(&msg, format, ap);
	va_end(ap);

	/* locating object */
	jpath = expand_path ? rp_jsonc_path(rp_jsonc_expand_path_get(expand_path, 0), object) : NULL;

	/* emit the error */
	LIBAFB_ERROR("%s (json-path %s)", rc > 0 ? msg : "json expansion error", jpath ? jpath : "?");
	free(jpath);
	free(msg);
}

static int locate_subpath(void *closure, const rp_path_search_entry_t *item)
{
	struct expref *expref = closure;
	char path[PATH_MAX + 1];
	struct stat st;
	int rc;

	if (item->pathlen + expref->searched_item_length < PATH_MAX) {
		memcpy(path, item->path, item->pathlen);
		path[item->pathlen] = '/';
		memcpy(&path[item->pathlen+1], expref->searched_item, expref->searched_item_length + 1);
		rc = stat(path, &st);
		if (rc == 0) {
			strcpy(expref->filename, path);
			if ((st.st_mode & S_IFMT) == S_IFDIR)
				expref->found_directory++;
			else
				expref->found_file++;
		}
	}
	return 0;
}

static int locate_entry(void *closure, const rp_path_search_entry_t *item)
{
	struct expref *expref = closure;
	if (item->name != NULL && strcmp(expref->searched_item, item->name))  {
		strcpy(expref->filename, item->path);
		if (item->isDir)
			expref->found_directory++;
		else
			expref->found_file++;
	}
	return 0;
}

/**
 * search the file of name 'filename'
 * 
 * @return 1 if filename is a directory, 0 if it is a file, a negative value otherwise
 */
static int search_path(struct expref *expref, const char *filename)
{
	int rc;
	struct stat st;
	int psflags;
	int (*callback)(void*,const rp_path_search_entry_t*);

	/* no path search if absolute path or unavailable */
	if (filename[0] == '/' || expref->pathsearch == NULL) {
		strncpy(expref->filename, filename, PATH_MAX);
		expref->filename[PATH_MAX] = 0;
		rc = stat(expref->filename, &st);
		return rc < 0 ? rc : (st.st_mode & S_IFMT) == S_IFDIR;
	}

	/* TODO: add PATH search HERE */
	if (strchr(filename, '/')) {
		psflags = RP_PATH_SEARCH_RECURSIVE|RP_PATH_SEARCH_DIRECTORY;
		callback = locate_subpath;
	}
	else {
		psflags = RP_PATH_SEARCH_RECURSIVE|RP_PATH_SEARCH_DIRECTORY|RP_PATH_SEARCH_FILE;
		callback = locate_entry;
	}
	expref->searched_item = filename;
	expref->searched_item_length = strlen(filename);
	expref->found_file = 0;
	expref->found_directory = 0;
	rp_path_search(expref->pathsearch, psflags, callback, expref);
	if (expref->found_file + expref->found_directory == 0)
		return -ENOENT;
	if (expref->found_file + expref->found_directory > 1)
		return -ENFILE;
	return expref->found_directory;
}

/**
 * Load the JSON object from the file of 'filename' in data of 'expref'
 *
 * @param expref the structure containing the data for merging
 * @param filename the file to be loaded relative to current context
 * @param path the file to be loaded but for printing
 */
static void merge_one_file(struct expref *expref, const char *filename, const char *path)
{
	int rc;
	struct json_object *obj;
	
	LIBAFB_NOTICE("Loading config file %s", path);

	/* read the object in obj */
	rc = read_config_file(&obj, filename);
	if (rc < 0) {
		LIBAFB_ERROR("Can't process json file %s: %s", path, strerror(-rc));
		expref->expand_error_code = rc;
	}
	else {
		expref->count++;
		if (expref->target == NULL)
			expref->target = obj;
		else {
			rp_jsonc_object_merge(expref->target, obj, expref->merge_option);
			json_object_put(obj);
		}
	}
}

/**
 * Load the JSON object from the file of 'filename' in data of 'expref'
 *
 * @param expref the structure containing the data for merging
 * @param filename the file to be loaded
 */
static void expand_a_file(struct expref *expref, const char *filename)
{
	merge_one_file(expref, filename, filename);
}

/**
 * compare entries for fts, alphabetical sort
 */
static int cmpent(const FTSENT **a, const FTSENT **b)
{
	return strcmp((**a).fts_path, (**b).fts_path);
}

/**
 * Load the JSON objects from the files in the directory referenced by 'expref'
 */
static void expand_a_directory(struct expref *expref)
{
	FTSENT *ent;
	FTS *fts;
	char *dirs[] = { (char*)expref->filename, NULL };

	/* use fts exporation */
	fts = fts_open(dirs, FTS_LOGICAL, cmpent);
	if (fts == NULL) {
		LIBAFB_ERROR("Can't process directory %s: %s", expref->filename, strerror(errno));
		expref->expand_error_code = -errno;
	}
	else {
		while((ent = fts_read(fts)) != NULL) {
			if (ent->fts_info & FTS_F)
				merge_one_file(expref, ent->fts_accpath, ent->fts_path);
		}
		fts_close(fts);
	}
}

/**
 * Called for each object referenced by "$ref", must be a string.
 * The string is then loaded.
 *
 * @param closure callback closure pointing a expref
 * @param the object referencing what to expand
 */
static void expand_ref(void *closure, struct json_object *object)
{
	struct expref *expref = closure;
	int rc;

	if (!json_object_is_type(object, json_type_string)) {
		error_at_object(object, expref->expand_path,
			"Expected a string but got %s",
			json_object_get_string(object));
		expref->error_code = -EINVAL;
	}
	else {
		rc = search_path(expref, json_object_get_string(object));
		if (rc < 0) {
			LIBAFB_ERROR("File not found %s", json_object_get_string(object));
			expref->error_code = rc;
		}
		else if (rc == 0) {
			expand_a_file(expref, expref->filename);
		}
		else {
			if (expref->accept_dirs)
				expand_a_directory(expref);
			else {
				error_at_object(object, expref->expand_path,
					"Not allowed to explore directory %s",
					json_object_get_string(object));
				expref->error_code = -EINVAL;
			}
		}
	}
}

/**
 * Check if the object is to be expanded
 * If yes the return its expansion, otherwise, returns the object
 * @see expand_json
 * $ref it accepted to be a string or an array of strings
 *
 * @param closure pointer to an integer for storing erreors
 * @param object  the object to expand
 * @param path    the path from root
 *
 * @return either the given object or its expansion
 */
static struct json_object *expand_object(void *closure, struct json_object* object, rp_jsonc_expand_path_t expand_path)
{
	struct expref *expref = closure;
	struct json_object *ref;

	/* if there is a "$ref" the object needs expansion */
	if (json_object_object_get_ex(object, "$ref", &ref)) {
		expref->target = NULL;
		expref->expand_path = expand_path;
		expref->accept_dirs = 0;
		expref->count = 0;
		expref->expand_error_code = 0;
		expref->merge_option = rp_jsonc_merge_option_replace;
		rp_jsonc_optarray_for_all(ref, expand_ref, expref);
		if (expref->count == 0 && expref->expand_error_code == 0) {
			error_at_object(object, expand_path, "No refererence found in %s", json_object_get_string(object));
			expref->expand_error_code = -EINVAL;
		}
		if (expref->expand_error_code == 0)
			object = expref->target;
		else {
			json_object_put(expref->target);
			if (expref->error_code == 0)
				expref->error_code = expref->expand_error_code;
		}
	}
	return object;
}

/**
 * search the variable 'name' of 'length'
 *
 * @param closure 
 * @param name of the variable to be resolved
 * @param len lan of the name
 * @param vres the data
 *
 * @return a boolean indicating if found (true) or not (false)
 */
static int get_var(void *closure, const char *name, size_t len, rp_expand_vars_result_t *vres)
{
	struct expref *expref = closure;
	struct json_object *obj, *itm;
	const char *result;
	int idx;
	char *key;
	rp_jsonc_expand_path_t expand_path;

	/* init */
	result = NULL;

	/* this loop searches a neighbor key in parents */
	if (len > 1 && name[0] == '@') {
		key = alloca(len);
		memcpy(key, &name[1], len - 1);
		key[len - 1] = 0;
		expand_path = expref->expand_path;
		idx = rp_jsonc_expand_path_length(expand_path);
		while (result == NULL && idx > 0) {
			if (rp_jsonc_expand_path_is_object(expand_path, --idx)) {
				obj = rp_jsonc_expand_path_get(expand_path, idx);
				if (json_object_object_get_ex(obj, key, &itm))
					result = json_object_get_string(itm);
			}
		}
	}

	/* else search environment */
	if (result == NULL)
		result = rp_expand_vars_search_env(name, len);

	vres->value = result;
	return result != NULL;
}

/**
 * callback for expanding strings
 *
 * @param closure the closure
 * @param object the string object to be exanded
 * @param path path of the string to expand
 *
 * @return the object resulting of expanding the string or the given object
 * if no expansion has been done
 */
static struct json_object *expand_string(void *closure, struct json_object* object, rp_jsonc_expand_path_t expand_path)
{
	struct expref *expref = closure;
	char *subst;
	
	expref->expand_path = expand_path;
	subst = rp_expand_vars_function(json_object_get_string(object), 0, get_var, expref);
	if (subst) {
		struct json_object *obj = NULL;
		double dval;
		long lval;
		char *end, *beg;

		/* try if true */
		if (strcasecmp(subst, "true") == 0)
			obj = json_object_new_boolean(1);
		/* try if false */
		if (obj == NULL && strcasecmp(subst, "false") == 0)
			obj = json_object_new_boolean(0);
		/* try if integer */
		if (obj == NULL) {
			errno = 0;
			lval = strtol(subst, &end, 10);
			if (*end == 0 && end != subst && errno == 0 && lval <= INT32_MAX && lval >= INT32_MIN)
				obj = json_object_new_int((int32_t)lval);
		}
		/* try if hexa integer */
		if (obj == NULL && subst[0] == '0' && (subst[1] | 32) == 'x') {
			errno = 0;
			beg = &subst[2];
			lval = strtol(beg, &end, 16);
			if (*end == 0 && end != beg && errno == 0 && lval <= INT32_MAX && lval >= INT32_MIN)
				obj = json_object_new_int((int32_t)lval);
		}
		/* try if binary integer */
		if (obj == NULL && subst[0] == '0' && (subst[1] | 32) == 'b') {
			errno = 0;
			beg = &subst[2];
			lval = strtol(beg, &end, 2);
			if (*end == 0 && end != beg && errno == 0 && lval <= INT32_MAX && lval >= INT32_MIN)
				obj = json_object_new_int((int32_t)lval);
		}
		/* try if octal integer */
		if (obj == NULL && subst[0] == '0') {
			errno = 0;
			beg = &subst[1 + ((subst[1] | 32) == 'o')];
			lval = strtol(beg, &end, 8);
			if (*end == 0 && end != beg && errno == 0 && lval <= INT32_MAX && lval >= INT32_MIN)
				obj = json_object_new_int((int32_t)lval);
		}
		/* try if floating point */
		if (obj == NULL) {
			errno = 0;
			dval = strtod(subst, &end);
			if (*end == 0 && end != subst && errno == 0)
				obj = json_object_new_double_s(dval, subst);
		}
		/* if done */
		if (obj != NULL && strcmp(subst, "null") == 0)
			object = obj;
		else {
			/* else default to string */
			obj = json_object_new_string(subst);
			if (obj == NULL)
				expref->error_code = -ENOMEM;
			else
				object = obj;
		}
	}
	return object;
}

int expand_config(struct json_object **config, int readrefs)
{
	struct json_object *obj;
	struct expref expref;

	expref.pathsearch = NULL;
	expref.error_code = 0;
	obj = rp_jsonc_expand(*config, &expref, readrefs ? expand_object : NULL, expand_string);
	if (obj != *config) {
		json_object_put(*config);
		*config = obj;
#if 0
		ERROR("expansion of config changed the root object! Please use option --config instead.");
		if (expref.error_code == 0)
			expref.error_code = -ECANCELED;
#endif
	}
	return expref.error_code;
}

static int is_json_filename(const char *filename)
{
	/* check extension */
	static char json[] = ".json";
	int st = 0, idx = 0;
	while (filename[idx]) {
		st = filename[idx] == json[st]
			? st + 1
			: filename[idx] == json[0];
		idx++;
	}
	return st >= 4;
}

static int read_json_file(struct json_object **obj, const char *filename)
{
	*obj = json_object_from_file(filename);
	return *obj ? 0 : -1;
}

static int read_yaml_file(struct json_object **obj, const char *filename)
{
	return rp_yaml_path_to_json_c(obj, filename, NULL);
}


/* read a config file */
int read_config_file(struct json_object **obj, const char *filename)
{
	int rc;
	if (access(filename, R_OK) < 0) {
		LIBAFB_ERROR("Can't read file %s: %s", filename, strerror(errno));
		rc = -1;
	}
	else if (is_json_filename(filename)) {
		rc = read_json_file(obj, filename);
		if (rc < 0)
			LIBAFB_ERROR("Bad JSON file %s", filename);
	}
	else {
		rc = read_yaml_file(obj, filename);
		if (rc < 0)
			LIBAFB_ERROR("Bad YAML file %s", filename);
	}
	return rc;
}
