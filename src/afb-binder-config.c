/*
 * Copyright (C) 2015-2021 IoT.bzh Company
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

#include "afb-binder.h"

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

#include <libafb/utils/expand-vars.h>
#include <libafb/utils/expand-json.h>
#include <libafb/utils/json-locator.h>
#include <libafb/utils/wrap-json.h>
#include <libafb/utils/path-search.h>

#include <libafb/sys/verbose.h>

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
	enum wrap_json_merge_option merge_option;

	/** path of the reference object */
	expand_json_path_t expand_path;

	/** for searching entries */
	struct path_search *pathsearch;

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
 * Emits an error for a given object of witin path of ref
 *
 * @param object the object leading to an error
 * @param path   path to the expanded reference or NULL
 * @param format the message as in printf
 * @param ...    argument of the printf like message
 */
static void error_at_object(struct json_object *object, expand_json_path_t expand_path, const char *format, ...)
{
	char *jpath;
	const char *file;
	char *msg;
	unsigned line;
	va_list ap;

	/* string for the message */
	va_start(ap, format);
	vasprintf(&msg, format, ap);
	va_end(ap);

	/* locating object */
	file = json_locator_locate(object, &line);
	jpath = expand_path ? json_locator_search_path(expand_json_path_get(expand_path, 0), object) : NULL;

	/* emit the error */
	ERROR("%s (file %s line %u json-path %s", msg, file, line, jpath);
	free(jpath);
	free(msg);
}

static int locate_subpath(void *closure, struct path_search_item *item)
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

static int locate_entry(void *closure, struct path_search_item *item)
{
	struct expref *expref = closure;
	if (strcmp(expref->searched_item, item->name))  {
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
	int (*callback)(void*,struct path_search_item*);

	/* no path search if absolute path or unavailable */
	if (filename[0] == '/' || expref->pathsearch == NULL) {
		strncpy(expref->filename, filename, PATH_MAX);
		expref->filename[PATH_MAX] = 0;
		rc = stat(expref->filename, &st);
		return rc < 0 ? rc : (st.st_mode & S_IFMT) == S_IFDIR;
	}

	/* TODO: add PATH search HERE */
	if (strchr(filename, '/')) {
		psflags = PATH_SEARCH_RECURSIVE|PATH_SEARCH_DIRECTORY;
		callback = locate_subpath;
	}
	else {
		psflags = PATH_SEARCH_RECURSIVE|PATH_SEARCH_DIRECTORY|PATH_SEARCH_FILE;
		callback = locate_entry;
	}
	expref->searched_item = filename;
	expref->searched_item_length = strlen(filename);
	expref->found_file = 0;
	expref->found_directory = 0;
	path_search(expref->pathsearch, psflags, callback, expref);
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
	
	NOTICE("Loading config file %s", path);

	/* read the object in obj */
#if 0
	obj = json_object_from_file(filename);
	rc = obj ? 0 : -errno;
#else
	/* use the locator to keep track of the lines and of the file */
	rc = json_locator_from_file(&obj, filename);
#endif
	if (rc < 0) {
		ERROR("Can't process json file %s: %s", path, strerror(-rc));
		expref->expand_error_code = rc;
	}
	else {
		expref->count++;
		if (expref->target == NULL)
			expref->target = obj;
		else {
			wrap_json_object_merge(expref->target, obj, expref->merge_option);
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
		ERROR("Can't process directory %s: %s", expref->filename, strerror(errno));
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
			ERROR("File not found %s", json_object_get_string(object));
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
static struct json_object *expand_object(void *closure, struct json_object* object, expand_json_path_t expand_path)
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
		expref->merge_option = wrap_json_merge_option_replace;
		wrap_json_optarray_for_all(ref, expand_ref, expref);
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
 *
 * @return the value to be substituted to the variable or NULL
 */
static const char *get_var(void *closure, const char *name, size_t len)
{
	struct expref *expref = closure;
	struct json_object *obj, *itm;
	const char *result;
	int idx;
	char *key;
	expand_json_path_t expand_path;

	/* init */
	result = NULL;

	/* this loop searches a neighbor key in parents */
	if (len > 1 && name[0] == '@') {
		key = strndupa(&name[1], len - 1);
		expand_path = expref->expand_path;
		idx = expand_json_path_length(expand_path);
		while (result == NULL && idx > 0) {
			if (expand_json_path_is_object(expand_path, --idx)) {
				obj = expand_json_path_get(expand_path, idx);
				if (json_object_object_get_ex(obj, key, &itm))
					result = json_object_get_string(itm);
			}
		}
	}

	/* else search environment */
	if (result == NULL)
		result = expand_vars_search_env(name, len);
	return result;
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
static struct json_object *expand_string(void *closure, struct json_object* object, expand_json_path_t expand_path)
{
	struct expref *expref = closure;
	struct json_object* obj;
	char *subst;
	
	expref->expand_path = expand_path;
	subst = expand_vars_function(json_object_get_string(object), 0, get_var, expref);
	if (subst) {
		obj = json_object_new_string(subst);
		if (obj == NULL)
			expref->error_code = -ENOMEM;
		else {
			json_locator_copy(object, obj);
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
	obj = expand_json(*config, &expref, readrefs ? expand_object : NULL, expand_string);
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
