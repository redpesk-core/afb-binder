/*
 * Copyright (C) 2015-2025 IoT.bzh Company
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <argp.h>

#include <json-c/json.h>
#if !defined(JSON_C_TO_STRING_NOSLASHESCAPE)
#define JSON_C_TO_STRING_NOSLASHESCAPE 0
#endif

#include <rp-utils/rp-jsonc.h>

#if WITH_AFB_HOOK
#include <libafb/core/afb-hook-flags.h>
#endif

#include "afb-binder-defaults.h"
#include "afb-binder-opts.h"
#include "afb-binder-config.h"
#include "afb-binder-utils.h"
#include <libafb/extend/afb-extend.h>
#include <libafb/misc/afb-verbose.h>
#include <libafb/afb-utils.h>

#define _d2s_(x)  #x
#define d2s(x)    _d2s_(x)

#if !defined(AFB_BINDER_VERSION)
#error "you should define AFB_BINDER_VERSION"
#endif

// Define command line option
#define SET_BACKGROUND       1
#define SET_FOREGROUND       2
#define SET_ROOT_DIR         3

#define SET_API_TIMEOUT      4
#define SET_SESSION_TIMEOUT  5

#define SET_SESSIONMAX       6

#define ADD_WS_CLIENT        7
#define ADD_WS_SERVICE       8

#define SET_TRACEEVT         9
#define SET_TRACESES        10
#define SET_TRACEREQ        11
#define SET_TRACEAPI        12
#define SET_TRACEGLOB       13

#define SET_TRAP_FAULTS     14
#define ADD_CALL            15

#if WITH_DYNAMIC_BINDING && WITH_DIRENT
#define ADD_LDPATH          16
#define ADD_WEAK_LDPATH     17
#endif

#if WITH_LIBMICROHTTPD
#define SET_ROOT_BASE       18
#define SET_ROOT_API        19
#define ADD_ALIAS           20
#define SET_CACHE_TIMEOUT   21
#define SET_ROOT_HTTP       22
#define SET_NO_HTTPD        23
#define SET_HTTPS           24
#define SET_HTTPS_CERT      25
#define SET_HTTPS_KEY       26
#endif

#define ADD_RPC_CLIENT      27
#define ADD_RPC_SERVICE     28

#define SET_WSMAXLEN        29

#define ADD_AUTO_API       'A'
#if WITH_DYNAMIC_BINDING
# define ADD_BINDING       'b'
#endif
#define SET_CONFIG         'C'
#define SET_COLOR          'c'
#define SET_DAEMON         'D'
#define SET_EXEC           'e'
#define SET_NO_TRAP_FAULTS 'F'
#define GET_HELP           'h'
#define ADD_INTERFACE      'i'
#define SET_JOB_MAX        'j'
#define SET_LOG            'l'
#if WITH_MONITORING
# define SET_MONITORING    'M'
#endif
#define SET_NAME           'n'
#define SET_OUTPUT         'o'
#define SET_PORT           'p'
#define SET_QUIET          'q'
#define ADD_SET            's'
#define SET_THR_MAX        't'
#define SET_THR_INIT       'T'
#if WITH_LIBMICROHTTPD
# define SET_UPLOAD_DIR    'u'
#endif
#define SET_UUID           'U'
#define GET_VERSION        'V'
#define SET_VERBOSE        'v'
#define SET_WORK_DIR       'w'
#define ADD_EXTENSION      'x'
#define ADD_EXTPATH        'X'
#define DUMP_CONFIG_FINAL  'z'
#define DUMP_CONFIG        'Z'

#if WITH_TLS
#define SET_TLS_CERT       128
#define SET_TLS_KEY        129
#define ADD_TLS_TRUST      130
#endif

/* definition of options */
static const struct argp_option optdefs[] = {
/* *INDENT-OFF* */
	{ .name="verbose",     .key=SET_VERBOSE,         .arg=0, .doc="Verbose Mode, repeat to increase verbosity" },
	{ .name="color",       .key=SET_COLOR,           .arg="VALUE", .flags=OPTION_ARG_OPTIONAL, .doc="Colorize the ouput" },
	{ .name="quiet",       .key=SET_QUIET,           .arg=0, .doc="Quiet Mode, repeat to decrease verbosity" },
	{ .name="log",         .key=SET_LOG,             .arg="LOGSPEC", .doc="Tune log level" },

	{ .name="foreground",  .key=SET_FOREGROUND,      .arg=0, .doc="Get all in foreground mode" },
	{ .name="background",  .key=SET_BACKGROUND,      .arg=0, .doc="Get all in background mode" },
	{ .name="daemon",      .key=SET_DAEMON,          .arg=0, .doc="Get all in background mode" },

	{ .name="name",        .key=SET_NAME,            .arg="NAME", .doc="Set the visible name" },

#if WITH_LIBMICROHTTPD
	{ .name="no-httpd",    .key=SET_NO_HTTPD,        .arg=0, .doc="Forbid HTTP service" },
	{ .name="port",        .key=SET_PORT,            .arg="PORT", .doc="HTTP listening TCP port  [default " d2s(DEFAULT_HTTP_PORT) "]" },
	{ .name="interface",   .key=ADD_INTERFACE,       .arg="INTERFACE", .doc="Add HTTP listening interface (ex: tcp:localhost:8080)" },
	{ .name="roothttp",    .key=SET_ROOT_HTTP,       .arg="DIRECTORY", .doc="HTTP Root Directory [default no root http (files not served but apis still available)]" },
	{ .name="rootbase",    .key=SET_ROOT_BASE,       .arg="PATH", .doc="Angular Base Root URL [default /opa]" },
	{ .name="rootapi",     .key=SET_ROOT_API,        .arg="PATH", .doc="HTML Root API URL [default /api]" },
	{ .name="alias",       .key=ADD_ALIAS,           .arg="ALIAS", .doc="Multiple url map outside of rootdir [eg: --alias=/icons:/usr/share/icons]" },
	{ .name="uploaddir",   .key=SET_UPLOAD_DIR,      .arg="DIRECTORY", .doc="Directory for uploading files [default: workdir] relative to workdir" },
	{ .name="cache-eol",   .key=SET_CACHE_TIMEOUT,   .arg="TIMEOUT", .doc="Client cache end of live [default " d2s(DEFAULT_CACHE_TIMEOUT) "]" },
	{ .name="https",       .key=SET_HTTPS,           .arg=0, .doc="Activates HTTPS" },
	{ .name="https-cert",  .key=SET_HTTPS_CERT,      .arg="FILE", .doc="File containing the certificate (pem)" },
	{ .name="https-key",   .key=SET_HTTPS_KEY,       .arg="KEY", .doc="File containing the private key (pem)" },
#endif

	{ .name="apitimeout",  .key=SET_API_TIMEOUT,     .arg="TIMEOUT", .doc="Binding API timeout in seconds [default " d2s(DEFAULT_API_TIMEOUT) "]" },
	{ .name="cntxtimeout", .key=SET_SESSION_TIMEOUT, .arg="TIMEOUT", .doc="Client Session Context Timeout [default " d2s(DEFAULT_SESSION_TIMEOUT) "]" },

	{ .name="workdir",     .key=SET_WORK_DIR,        .arg="DIRECTORY", .doc="Set the working directory [default: $PWD or current working directory]" },
	{ .name="rootdir",     .key=SET_ROOT_DIR,        .arg="DIRECTORY", .doc="Root Directory of the application [default: workdir] relative to workdir" },

#if WITH_DYNAMIC_BINDING
	{ .name="binding",     .key=ADD_BINDING,         .arg="SPEC", .doc="Load the binding of SPEC where SPEC is BINDING[:[UID:]CONFIG], "
	                                                                   "the path of the BINDING, the path of the CONFIG, the UID" },
#if WITH_DIRENT
	{ .name="ldpaths",     .key=ADD_LDPATH,          .arg="PATHSET", .doc="Load bindings from dir1:dir2:..." },
	{ .name="weak-ldpaths",.key=ADD_WEAK_LDPATH,     .arg="PATHSET", .doc="Same as --ldpaths but errors are not fatal" },
#endif
#endif

#if WITH_EXTENSION
	{ .name="extension",   .key=ADD_EXTENSION,       .arg="SPEC", .doc="Load the extension of SPEC where SPEC is EXTENSION[:[UID:]CONFIG], "
	                                                                   "the path of the EXTENSION, the path of the CONFIG, the UID" },
#if WITH_DIRENT
	{ .name="extpaths",    .key=ADD_EXTPATH,         .arg="PATHSET", .doc="Load extensions from dir1:dir2:..."},
#endif
#endif

	{ .name="ws-client",   .key=ADD_WS_CLIENT,       .arg="SOCKSPEC", .doc="Bind to an afb service through websocket" },
	{ .name="ws-server",   .key=ADD_WS_SERVICE,      .arg="SOCKSPEC", .doc="Provide an afb service through websockets" },
	{ .name="ws-maxlen",   .key=SET_WSMAXLEN,        .arg="LENGTH",   .doc="set max length of websockets [default " d2s(WEBSOCKET_DEFAULT_MAXLENGTH) "]" },

	{ .name="rpc-client",   .key=ADD_RPC_CLIENT,     .arg="SOCKSPEC", .doc="Bind to an afb service through websocket" },
	{ .name="rpc-server",   .key=ADD_RPC_SERVICE,    .arg="SOCKSPEC", .doc="Provide an afb service through websockets" },

	{ .name="auto-api",    .key=ADD_AUTO_API,        .arg="DIRECTORY", .doc="Automatic load of api of the given directory" },

	{ .name="session-max", .key=SET_SESSIONMAX,      .arg="COUNT", .doc="Max count of session simultaneously [default " d2s(DEFAULT_MAX_SESSION_COUNT) "]" },

#if WITH_AFB_HOOK
	{ .name="tracereq",    .key=SET_TRACEREQ,        .arg="VALUE", .doc="Log the requests: none, common, extra, all" },
	{ .name="traceevt",    .key=SET_TRACEEVT,        .arg="VALUE", .doc="Log the events: none, common, extra, all" },
	{ .name="traceses",    .key=SET_TRACESES,        .arg="VALUE", .doc="Log the sessions: none, all" },
	{ .name="traceapi",    .key=SET_TRACEAPI,        .arg="VALUE", .doc="Log the apis: none, common, api, event, all" },
	{ .name="traceglob",   .key=SET_TRACEGLOB,       .arg="VALUE", .doc="Log the globals: none, all" },
#endif

	{ .name="call",        .key=ADD_CALL,            .arg="CALLSPEC", .doc="Call at start, format of val: API/VERB:json-args" },

	{ .name="exec",        .key=SET_EXEC,            .arg=0, .flags=OPTION_NO_USAGE, .doc="Execute the remaining arguments" },

#if WITH_MONITORING
	{ .name="monitoring",  .key=SET_MONITORING,      .arg=0, .doc="Enable HTTP monitoring at <ROOT>/monitoring/" },
#endif

	{ .name="config",      .key=SET_CONFIG,          .arg="FILENAME", .doc="Load options from the given config file" },
	{ .name="dump-config", .key=DUMP_CONFIG,         .arg=0, .doc="Dump the config to stdout and exit" },
	{ .name="dump-final-config", .key=DUMP_CONFIG_FINAL,   .arg=0, .doc="Dump the config after expansion to stdout and exit" },

	{ .name="set",         .key=ADD_SET,             .arg="VALUE", .doc="Set parameters ([API]/[KEY]:JSON or {\"API\":{\"KEY\":JSON}}"  },
	{ .name="output",      .key=SET_OUTPUT,          .arg="FILENAME", .doc="Redirect stdout and stderr to output file (when --daemon)" },

	{ .name="trap-faults", .key=SET_TRAP_FAULTS,     .arg="VALUE", .doc="Trap faults: on, off, yes, no, true, false, 1, 0 (default: true)" },
	{ .name="fail",        .key=SET_NO_TRAP_FAULTS,  .arg=0,       .doc="Shortcut for --trap-faults=no" },

	{ .name="jobs-max",    .key=SET_JOB_MAX,         .arg="VALUE", .doc="Maximum count of jobs that can be queued  [default " d2s(DEFAULT_JOBS_MAX) "]" },
	{ .name="threads-max", .key=SET_THR_MAX,         .arg="VALUE", .doc="Maximum count of parallel threads held [default " d2s(DEFAULT_THREADS_MAX) "]" },
	{ .name="threads-init", .key=SET_THR_INIT,       .arg="VALUE", .doc="Initial count of threads [default " d2s(DEFAULT_THREADS_INIT) "]" },

	{ .name="uuid",        .key=SET_UUID,            .arg="VALUE", .doc="Set the session UUID of the binder, random by default" },

#if WITH_TLS
	{ .name="tls-cert",    .key=SET_TLS_CERT,        .arg="FILENAME", .doc="Set the certificat of TLS connexions" },
	{ .name="tls-key",     .key=SET_TLS_KEY,         .arg="FILENAME", .doc="Set the private key for TLS connexions" },
	{ .name="tls-trust",   .key=ADD_TLS_TRUST,       .arg="PATH",     .doc="Add trust file or directory" },
#endif

	{ .name=0,             .key=0,                   .arg=0, .doc=0 }
/* *INDENT-ON* */
};

#if WITH_MONITORING
static const char MONITORING_ALIAS[] = "/monitoring:"MONITORING_INSTALL_DIR;
#endif

static const struct {
	int optid;
	int valdef;
} default_optint_values[] = {
#if WITH_LIBMICROHTTPD
	{ SET_CACHE_TIMEOUT,	DEFAULT_CACHE_TIMEOUT },
#endif
	{ SET_API_TIMEOUT,	DEFAULT_API_TIMEOUT },
	{ SET_SESSION_TIMEOUT,	DEFAULT_SESSION_TIMEOUT },
	{ SET_SESSIONMAX,	DEFAULT_MAX_SESSION_COUNT },
	{ SET_JOB_MAX,          DEFAULT_JOBS_MAX },
	{ SET_THR_MAX,          DEFAULT_THREADS_MAX },
	{ SET_THR_INIT,         DEFAULT_THREADS_INIT }
};

static const struct {
	int optid;
	const char *valdef;
} default_optstr_values[] = {
#if WITH_LIBMICROHTTPD
	{ SET_UPLOAD_DIR,	"." },
	{ SET_ROOT_BASE,	"/opa" },
	{ SET_ROOT_API,		"/api" },
#endif
	{ SET_WORK_DIR,		"." },
	{ SET_ROOT_DIR,		"." }
};

/**********************************
* preparing items
***********************************/

static void *oomchk(void *ptr)
{
	if (!ptr) {
		LIBAFB_ERROR("Out of memory");
		exit(1);
	}
	return ptr;
}

static const char *name_of_optid(int optid)
{
	const struct argp_option *iter = optdefs;
	while(iter->key != optid)
		iter++;
	return iter->name;
}

/*----------------------------------------------------------
 | printversion
 |   print version and copyright
 +--------------------------------------------------------- */
static const char version[] =
	"\n"
	AFB_BINDER_VERSION
	" ["
#if WITH_MONITORING
	"+MONITOR"
#endif
#if WITH_SUPERVISION
	"+SUPERVISION"
#endif
#if WITH_AFB_HOOK
	"+HOOK"
#endif
#if WITH_AFB_TRACE
	"+TRACE"
#endif
#if WITH_TLS
	"+TLS"
#endif
#if WITH_DYNAMIC_BINDING
	"+BINDING"
	"-V3"
	"-V4"
#endif
	"]\nCopyright (C) 2015-2025 IoT.bzh Company\n"
;

static const char docstring[] =
	"\n"
	"Options:\n"
;
/**********************************
 * dump config
 *********************************/
static void dump(struct json_object *config, FILE *file, const char *prefix, const char *title)
{
	const char *head, *tail;

	if (title)
		fprintf(file, "%s----BEGIN OF %s-----\n", prefix ?: "", title);

	head = json_object_to_json_string_ext(config, JSON_C_TO_STRING_PRETTY
		|JSON_C_TO_STRING_SPACED|JSON_C_TO_STRING_NOSLASHESCAPE);

	if (!prefix)
		fprintf(file, "%s\n", head);
	else {
		while(*head) {
			for (tail = head ; *tail && *tail != '\n' ; tail++);
			fprintf(file, "%s %.*s\n", prefix, (int)(tail - head), head);
			head = tail + !!*tail;
		}
	}

	if (title)
		fprintf(file, "%s----END OF %s-----\n", prefix ?: "", title);
}

/**********************************
* json helpers
***********************************/

static struct json_object *joomchk(struct json_object *value)
{
	return oomchk(value);
}

static struct json_object *to_jstr(const char *value)
{
	return joomchk(json_object_new_string(value));
}

/** create a string json-object of given length */
static struct json_object *to_jstr_len(const char *value, size_t len)
{
	return joomchk(json_object_new_string_len(value,(int)len));
}

static struct json_object *to_jint(int value)
{
	return joomchk(json_object_new_int(value));
}

static struct json_object *to_jbool(int value)
{
	return joomchk(json_object_new_boolean(value));
}

/**********************************
* arguments helpers
***********************************/

static int string_to_bool(const char *value)
{
	static const char true_names[] = "1\0yes\0true\0on";
	static const char false_names[] = "0\0no\0false\0off";
	size_t pos;

	pos = 0;
	while (pos < sizeof true_names)
		if (strcasecmp(value, &true_names[pos]))
			pos += 1 + strlen(&true_names[pos]);
		else
			return 1;

	pos = 0;
	while (pos < sizeof false_names)
		if (strcasecmp(value, &false_names[pos]))
			pos += 1 + strlen(&false_names[pos]);
		else
			return 0;

	return -1;
}

__attribute__((unused))
static void config_del(struct json_object *config, int optid)
{
	return json_object_object_del(config, name_of_optid(optid));
}

static int config_has(struct json_object *config, int optid)
{
	return json_object_object_get_ex(config, name_of_optid(optid), NULL);
}

__attribute__((unused))
static int config_has_bool(struct json_object *config, int optid)
{
	struct json_object *x;
	return json_object_object_get_ex(config, name_of_optid(optid), &x)
		&& json_object_get_boolean(x);
}

__attribute__((unused))
static int config_has_str(struct json_object *config, int optid, const char *val)
{
	int i, n;
	struct json_object *a;

	if (!json_object_object_get_ex(config, name_of_optid(optid), &a))
		return 0;

	if (!json_object_is_type(a, json_type_array))
		return !strcmp(val, json_object_get_string(a));

	n = (int)json_object_array_length(a);
	for (i = 0 ; i < n ; i++) {
		if (!strcmp(val, json_object_get_string(json_object_array_get_idx(a, i))))
			return 1;
	}
	return 0;
}

static void config_set(struct json_object *config, int optid, struct json_object *val)
{
	json_object_object_add(config, name_of_optid(optid), val);
}

static void config_set_str(struct json_object *config, int optid, const char *val)
{
	config_set(config, optid, to_jstr(val));
}

static void config_set_int(struct json_object *config, int optid, int value)
{
	config_set(config, optid, to_jint(value));
}

static void config_set_bool(struct json_object *config, int optid, int value)
{
	config_set(config, optid, to_jbool(value));
}

static void config_add(struct json_object *config, int optid, struct json_object *val)
{
	struct json_object *a;
	if (!json_object_object_get_ex(config, name_of_optid(optid), &a)) {
		a = joomchk(json_object_new_array());
		json_object_object_add(config, name_of_optid(optid), a);
	}
	else if (!json_object_is_type(a, json_type_array)) {
		LIBAFB_ERROR("The configuration item %s MUST be an array", name_of_optid(optid));
		exit(1);
	}
	json_object_array_add(a, val);
}

static void config_add_str(struct json_object *config, int optid, const char *val)
{
	config_add(config, optid, to_jstr(val));
}

static void config_mix2_cb(void *closure, struct json_object *obj, const char *name)
{
	struct json_object *dest, *base = closure;

	if (!name)
		name = "";

	if (!json_object_object_get_ex(base, name, &dest)) {
		dest = joomchk(json_object_new_object());
		json_object_object_add(base, name, dest);
	}
	if (json_object_is_type(obj, json_type_object))
		rp_jsonc_object_merge(dest, obj, rp_jsonc_merge_option_join_or_replace);
	else
		json_object_object_add(dest, "", json_object_get(obj));
}

static void config_mix2(struct json_object *config, int optid, struct json_object *val)
{
	struct json_object *obj;

	if (!json_object_object_get_ex(config, name_of_optid(optid), &obj)) {
		obj = joomchk(json_object_new_object());
		json_object_object_add(config, name_of_optid(optid), obj);
	}
	rp_jsonc_for_all(val, config_mix2_cb, obj);
}

static void config_mix2_str(struct json_object *config, int optid, const char *val)
{
	size_t st1, st2;
	const char *api, *key;
	struct json_object *obj, *sub;
	enum json_tokener_error jerr;
	char *tmp;

	st1 = strcspn(val, "/:{[\"");
	st2 = strcspn(&val[st1], ":{[\"");
	if (val[st1] != '/' || val[st1 + st2] != ':') {
		obj = json_tokener_parse_verbose(val, &jerr);
		if (jerr != json_tokener_success)
			obj = json_object_new_string(val);
	} else {
		if (st1 == 0)
			api = "*";
		else {
			tmp = alloca(st1 + 1);
			memcpy(tmp, val, st1);
			tmp[st1] = 0;
			api = tmp;
		}
		val += st1 + 1;
		if (st2 <= 1 || (st2 == 2 && *val == '*'))
			key = NULL;
		else {
			tmp = alloca(st2);
			memcpy(tmp, val, st2 - 1);
			tmp[st2 - 1] = 0;
			key = tmp;
		}
		val += st2;
		sub = json_tokener_parse_verbose(val, &jerr);
		if (jerr != json_tokener_success)
			sub = json_object_new_string(val);

		if (key) {
			obj = json_object_new_object();
			json_object_object_add(obj, key, sub);
			sub = obj;
		}
		obj = json_object_new_object();
		json_object_object_add(obj, api, sub);
	}
	config_mix2(config, optid, obj);
	json_object_put(obj);
}

static int get_arg_bool(int optid, char *svalue)
{
	int value = string_to_bool(svalue);
	if (value < 0) {
		LIBAFB_ERROR("option --%s needs a boolean value: yes/no, true/false, on/off, 1/0",
				name_of_optid(optid));
		exit(1);
	}
	return value;
}

static void config_set_optstr(struct json_object *config, int optid, const char *value)
{
	config_set_str(config, optid, value);
}

static void config_set_optint_base(struct json_object *config, int optid, char *value, int mini, int maxi, int base)
{
	const char *beg, *end;
	long int val;

	beg = value;
	val = strtol(beg, (char**)&end, base);
	if (*end || end == beg) {
		LIBAFB_ERROR("option --%s requires a valid integer (found %s)",
			name_of_optid(optid), beg);
		exit(1);
	}
	if (val < (long int)mini || val > (long int)maxi) {
		LIBAFB_ERROR("option --%s value %ld out of bounds (not in [%d , %d])",
			name_of_optid(optid), val, mini, maxi);
		exit(1);
	}
	config_set_int(config, optid, (int)val);
}

static void config_set_optint(struct json_object *config, int optid, char *value, int mini, int maxi)
{
	return config_set_optint_base(config, optid, value, mini, maxi, 10);
}

__attribute__((unused))
static void config_set_optflags(struct json_object *config, int optid,
				char *value, int (*func)(const char*,unsigned*))
{

	if (func(value, 0) < 0) {
		LIBAFB_ERROR("option --%s bad value (found %s)",
			name_of_optid(optid), value);
		exit(1);
	}
	config_set_str(config, optid, value);
}

static void config_add_optstr(struct json_object *config, int optid, char *value)
{
	config_add_str(config, optid, value);
}

/**
 * Add a binding to the configuration. The string defining the binding
 * is interpreted as colon separated fields of the grammar "PATH[:[UID:]CONFIG]"
 * The function creates a json object made as below, reflecting that string:
 *  { "path": "PATH", "uid": "UID", "config": { "$ref": "CONFIG" } }
 * where fields "uid" and "config" are optionnals
 */
static void config_add_path_conf_uid(struct json_object *config, int optid, char *value, int prefixed)
{
	const char separator = ':';
	struct json_object *obj, *str, *ref = NULL;

	size_t pathlen, uidlen = 0, conflen = 0;
	char *pathstr, *uidstr = NULL, *confstr = NULL;

	/* get sizes and strings */
	pathstr = value;
	if (prefixed) /* skip prefix part */
		value += scan_export_prefix(value, NULL);
	while(*value && *value != separator) value++;
	pathlen = value - pathstr;
	if (*value) {
		for (uidstr = ++value; *value && *value != separator ; value++);
		if (*value) {
			uidlen = value - uidstr;
			confstr = ++value;
			conflen = strlen(confstr);
		}
		else {
			confstr = uidstr;
			conflen = value - confstr;
			uidstr = NULL;
		}
	}

	/* create the object */
	obj = oomchk(json_object_new_object());

	str = to_jstr_len(pathstr, pathlen);
	json_object_object_add(obj, "path", str);

	if (uidstr != NULL && uidlen != 0) {
		str = to_jstr_len(uidstr, uidlen);
		json_object_object_add(obj, "uid", str);
	}

	if (confstr != NULL && conflen != 0) {
		ref = oomchk(json_object_new_object());
		str = to_jstr_len(confstr, conflen);
		json_object_object_add(ref, "$ref", str);
		json_object_object_add(obj, "config", ref);
	}

	/* add the computed object */
	config_add(config, optid, obj);
}

static void config_mix2_optstr(struct json_object *config, int optid, char *value)
{
	config_mix2_str(config, optid, value);
}

/*---------------------------------------------------------
 |   set and get the log levels
 +--------------------------------------------------------- */

static void set_log(const char *args)
{
	char o = 0, s, *p, *i = strdupa(args);
	int lvl;

	for(;;) switch (*i) {
	case 0:
		return;
	case '+':
	case '-':
	case '@':
		o = *i;
		/*@fallthrough@*/
	case ' ':
	case ',':
		i++;
		break;
	default:
		p = i;
		while (isalpha(*p)) p++;
		s = *p;
		*p = 0;
		lvl = afb_verbose_level_of_name(i);
		if (lvl < 0) {
			i = strdupa(i);
			*p = s;
			LIBAFB_ERROR("Bad log name '%s' in %s", i, args);
			exit(1);
		}
		*p = s;
		i = p;
		if (o == '-')
			afb_verbose_sub(lvl);
		else {
			if (!o) {
				afb_verbose_clear();
				o = '+';
			}
			afb_verbose_add(lvl);
			if (o == '@')
				while(lvl)
					afb_verbose_add(--lvl);
		}
		break;
	}
}

static char *get_log()
{
	int level;
	char buffer[50];
	size_t i;
	const char *name;

	i = 0;
	for (level = afb_Log_Level_Error ; level <= afb_Log_Level_Debug ; level++) {
		if (afb_verbose_wants(level)) {
			if (i > 0 && i < sizeof buffer - 1)
				buffer[i++] = '+';
			name = afb_verbose_name_of_level(level);
			while (*name && i < sizeof buffer - 1)
				buffer[i++] = *name++;
		}
	}
	buffer[i] = 0;
	return strdup(buffer);
}

/*---------------------------------------------------------
 |   manage color value
 +--------------------------------------------------------- */

static const char color_values[] = "yes\0no\0auto";

static const char *normal_color_value(const char *value)
{
	if (!value || !strcmp(value, &color_values[0]))
		return &color_values[0];
	if (!strcmp(value, &color_values[4]))
		return &color_values[4];
	if (!strcmp(value, &color_values[7]))
		return &color_values[7];
	return 0;
}

static void set_color_value(const char *value)
{
	afb_verbose_colorize(*value == 'y' ? 1 : *value == 'n' ? 0 : -1);
}

/*---------------------------------------------------------
 |   Parse option and launch action
 +--------------------------------------------------------- */
static void fulfill_config(struct json_object *config)
{
	char *logging;
	int i;

	for (i = 0 ; i < sizeof default_optint_values / sizeof * default_optint_values ; i++)
		if (!config_has(config, default_optint_values[i].optid))
			config_set_int(config, default_optint_values[i].optid, default_optint_values[i].valdef);

	for (i = 0 ; i < sizeof default_optstr_values / sizeof * default_optstr_values ; i++)
		if (!config_has(config, default_optstr_values[i].optid))
			config_set_str(config, default_optstr_values[i].optid, default_optstr_values[i].valdef);

	logging = get_log();
	config_set_str(config, SET_LOG, logging);
	free(logging);

#if WITH_LIBMICROHTTPD
	if (!config_has(config, SET_PORT) && !config_has(config, ADD_INTERFACE) && !config_has_bool(config, SET_NO_HTTPD))
		config_set_int(config, SET_PORT, DEFAULT_HTTP_PORT);
#endif

#if WITH_MONITORING
	if (config_has_bool(config, SET_MONITORING) && !config_has_str(config, ADD_ALIAS, MONITORING_ALIAS))
		config_add_str(config, ADD_ALIAS, MONITORING_ALIAS);
#endif
}

/*---------------------------------------------------------
 |   Manage environment
 +--------------------------------------------------------- */
#if WITH_ENVIRONMENT
static void on_environment(struct json_object *config, int optid, const char *name, void (*func)(struct json_object*, int, const char*))
{
	char *value = secure_getenv(name);

	if (value && *value)
		func(config, optid, value);
}

static void on_environment_basic(const char *name, void (*func)(const char*))
{
	char *value = secure_getenv(name);
	if (value)
		func(value);
}

__attribute__((unused))
static void on_environment_flags(struct json_object *config, int optid,
			const char *name, int (*func)(const char*,unsigned*))
{
	char *value = secure_getenv(name);

	if (value) {
		if (func(value, 0) < 0)
			LIBAFB_WARNING("Unknown value %s for environment variable %s, ignored", value, name);
		else
			config_set_str(config, optid, value);
	}
}

static void on_environment_bool(struct json_object *config, int optid, const char *name)
{
	char *value = secure_getenv(name);
	int asbool;

	if (value) {
		asbool = string_to_bool(value);
		if (asbool < 0)
			LIBAFB_WARNING("Unknown value %s for environment variable %s, ignored", value, name);
		else
			config_set_bool(config, optid, asbool);
	}
}

static void on_environment_int(struct json_object *config, int optid, const char *name)
{
	char *value = secure_getenv(name);
	int asint;

	if (value) {
		asint = atoi(value);
		config_set_int(config, optid, asint);
	}
}

static void parse_environment_initial(struct json_object *config)
{
	on_environment_basic("AFB_LOG", set_log);
	on_environment_int(config, SET_API_TIMEOUT, "AFB_APITIMEOUT");
#if WITH_AFB_HOOK
	on_environment_flags(config, SET_TRACEREQ, "AFB_TRACEREQ", afb_hook_flags_req_from_text);
	on_environment_flags(config, SET_TRACEEVT, "AFB_TRACEEVT", afb_hook_flags_evt_from_text);
	on_environment_flags(config, SET_TRACESES, "AFB_TRACESES", afb_hook_flags_session_from_text);
	on_environment_flags(config, SET_TRACEAPI, "AFB_TRACEAPI", afb_hook_flags_api_from_text);
	on_environment_flags(config, SET_TRACEGLOB, "AFB_TRACEGLOB", afb_hook_flags_global_from_text);
#endif
#if WITH_DYNAMIC_BINDING && WITH_DIRENT
	on_environment(config, ADD_LDPATH, "AFB_LDPATHS", config_add_str);
	on_environment(config, ADD_WEAK_LDPATH, "AFB_WEAK_LDPATHS", config_add_str);
#endif
#if WITH_EXTENSION && WITH_DIRENT
	on_environment(config, ADD_EXTPATH, "AFB_EXTPATHS", config_add_str);
#endif
	on_environment(config, ADD_SET, "AFB_SET", config_mix2_str);
	on_environment_bool(config, SET_TRAP_FAULTS, "AFB_TRAP_FAULTS");
#if WITH_LIBMICROHTTPD
	on_environment_int(config, SET_PORT, "AFB_SET_PORT");
	on_environment_bool(config, SET_HTTPS, "AFB_HTTPS");
	on_environment(config, SET_HTTPS_CERT, "AFB_HTTPS_CERT", config_set_str);
	on_environment(config, SET_HTTPS_KEY, "AFB_HTTPS_KEY", config_set_str);
#endif
#if WITH_TLS
	on_environment(config, SET_TLS_CERT, "AFB_TLS_CERT", config_set_str);
	on_environment(config, SET_TLS_KEY, "AFB_TLS_KEY", config_set_str);
#endif
}

#endif

/*---------------------------------------------------------
 |   Parse options
 +--------------------------------------------------------- */

static error_t parsecb_initial(int key, char *value, struct argp_state *state)
{
	const char *cval;
	struct json_object *conf, *val;
	struct json_object *config = state->input;

	switch (key) {
	case SET_VERBOSE:
		afb_verbose_inc();
		break;

	case SET_COLOR:
		cval = normal_color_value(value);
		if (!cval) {
			LIBAFB_ERROR("Unknown color value %s", value);
			exit(1);
		}
		config_set_optstr(config, SET_COLOR, cval);
		set_color_value(cval);
		break;

	case SET_QUIET:
		afb_verbose_dec();
		break;

	case SET_LOG:
		set_log(value);
		break;

#if WITH_EXTENSION
	case ADD_EXTENSION:
		config_add_path_conf_uid(config, key, value, 0);
		break;
#if WITH_DIRENT
	case ADD_EXTPATH:
		config_add_optstr(config, key, value);
		break;
#endif
#endif

	case SET_TRAP_FAULTS:
		config_set_bool(config, key, get_arg_bool(key, value));
		break;
	case SET_NO_TRAP_FAULTS:
		config_set_bool(config, SET_TRAP_FAULTS, 0);
		break;

#if WITH_AFB_HOOK
	case SET_TRACEREQ:
		config_set_optflags(config, key, value, afb_hook_flags_req_from_text);
		break;

	case SET_TRACEEVT:
		config_set_optflags(config, key, value, afb_hook_flags_evt_from_text);
		break;

	case SET_TRACESES:
		config_set_optflags(config, key, value, afb_hook_flags_session_from_text);
		break;

	case SET_TRACEAPI:
		config_set_optflags(config, key, value, afb_hook_flags_api_from_text);
		break;

	case SET_TRACEGLOB:
		config_set_optflags(config, key, value, afb_hook_flags_global_from_text);
		break;
#endif
	case SET_CONFIG:
		if (read_config_file(&conf, value) < 0) {
			LIBAFB_ERROR("Can't read config file %s", value);
			exit(1);
		}
		if (json_object_object_get_ex(conf, "log", &val)) {
			if (json_object_get_type(val) == json_type_string)
				set_log(json_object_get_string(val));
		}
		rp_jsonc_object_merge(config, conf, rp_jsonc_merge_option_join_or_replace);
		json_object_put(conf);
		break;

	case SET_ROOT_DIR:
	case SET_WORK_DIR:
		config_set_optstr(config, key, value);
		break;

	case SET_EXEC:
		state->quoted = 1;
		break;

	default:
		break;
	}
	return 0;
}

struct children_data {
	const struct argp_option *options;
	struct json_object *config;
};

struct final_data {
	struct json_object *config;
	struct children_data *children_data;
	int nchildren;
	int dodump;
};

static error_t parsecb_final(int key, char *value, struct argp_state *state)
{
	int i;
	struct final_data *data = state->input;
	struct json_object *config = data->config;

	switch (key) {
	case ARGP_KEY_INIT:
		for (i = 0 ; i < data->nchildren ; i++)
			state->child_inputs[i] = &data->children_data[i];
		break;

	/* keys processed initially */
	case SET_VERBOSE:
	case SET_COLOR:
	case SET_QUIET:
	case SET_LOG:
#if WITH_EXTENSION
	case ADD_EXTENSION:
#if WITH_DIRENT
	case ADD_EXTPATH:
#endif
#endif
#if WITH_AFB_HOOK
	case SET_TRACEREQ:
	case SET_TRACEEVT:
	case SET_TRACESES:
	case SET_TRACEAPI:
	case SET_TRACEGLOB:
#endif
	case SET_TRAP_FAULTS:
	case SET_NO_TRAP_FAULTS:
	case SET_CONFIG:
	case SET_ROOT_DIR:
	case SET_WORK_DIR:
		break;

	/* other keys */
#if WITH_LIBMICROHTTPD
	case SET_PORT:
		config_set_optint(config, key, value, 1024, 32767);
		break;

	case SET_CACHE_TIMEOUT:
		config_set_optint(config, key, value, 0, INT_MAX);
		break;

	case SET_ROOT_HTTP:
	case SET_ROOT_BASE:
	case SET_ROOT_API:
	case SET_UPLOAD_DIR:
	case SET_HTTPS_CERT:
	case SET_HTTPS_KEY:
	case SET_OUTPUT:
	case SET_UUID:
		config_set_optstr(config, key, value);
		break;

	case ADD_ALIAS:
		config_add_optstr(config, key, value);
		break;

	case SET_HTTPS:
	case SET_NO_HTTPD:
		config_set_bool(config, key, 1);
		break;
#endif

	case SET_API_TIMEOUT:
	case SET_SESSION_TIMEOUT:
		config_set_optint(config, key, value, 0, INT_MAX);
		break;

	case SET_JOB_MAX:
	case SET_THR_MAX:
	case SET_THR_INIT:
	case SET_SESSIONMAX:
	case SET_WSMAXLEN:
		config_set_optint(config, key, value, 1, INT_MAX);
		break;

	case SET_NAME:
		config_set_optstr(config, key, value);
		break;

#if WITH_DYNAMIC_BINDING
	case ADD_BINDING:
		config_add_path_conf_uid(config, key, value, 1);
		break;
#if WITH_DIRENT
	case ADD_LDPATH:
	case ADD_WEAK_LDPATH:
#endif
#endif
	case ADD_CALL:
	case ADD_WS_CLIENT:
	case ADD_WS_SERVICE:
	case ADD_RPC_CLIENT:
	case ADD_RPC_SERVICE:
	case ADD_AUTO_API:
	case ADD_INTERFACE:
		config_add_optstr(config, key, value);
		break;

	case ADD_SET:
		config_mix2_optstr(config, key, value);
		break;

#if WITH_MONITORING
	case SET_MONITORING:
		config_set_bool(config, key, 1);
		break;
#endif

#if WITH_TLS
	case SET_TLS_CERT:
	case SET_TLS_KEY:
		config_set_optstr(config, key, value);
		break;
	case ADD_TLS_TRUST:
		config_add_optstr(config, key, value);
		break;
#endif

	case SET_FOREGROUND:
	case SET_BACKGROUND:
	case SET_DAEMON:
		config_set_bool(config, SET_DAEMON, key != SET_FOREGROUND);
		break;


	case SET_EXEC:
		state->quoted = 1;
		while (state->next < state->argc) {
			config_add_str(config, key, state->argv[state->next++]);
		}
		break;

	case DUMP_CONFIG:
	case DUMP_CONFIG_FINAL:
		data->dodump = 1 + (DUMP_CONFIG_FINAL == key);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

#if WITH_EXTENSION
static error_t parsecb_extension(int key, char *value, struct argp_state *state)
{
	struct children_data *data = state->input;
	struct json_object *config = data->config;
	const struct argp_option *options = data->options;
	struct json_object *a, *v, *na;

	if (!options)
		return 0;
	while(options->name) {
		if (options->key == key) {
			v = value ? json_object_new_string(value) : json_object_new_boolean(1);
			if (!json_object_object_get_ex(config, options->name, &a)) {
				json_object_object_add(config, options->name, v);
			}
			else if (json_object_is_type(a, json_type_array)) {
				json_object_array_add(a, v);
			}
			else {
				na = json_object_new_array();
				json_object_array_add(na, json_object_get(a));
				json_object_array_add(na, v);
				json_object_object_add(config, options->name, na);
			}
			return 0;
		}
		options++;
	}

	return ARGP_ERR_UNKNOWN;
	

}
#endif

int afb_binder_opts_parse_initial(int argc, char **argv, struct json_object **config)
{
	struct argp argp;
	int flags;

#if WITH_ENVIRONMENT
	parse_environment_initial(*config);
#endif
	argp.options = optdefs;
	argp.parser = parsecb_initial;
	argp.args_doc = "[--exec program args...]";
	argp.doc = docstring;
	argp.children = 0;
	argp.help_filter = 0;
	argp.argp_domain = 0;
	argp_program_version = version;
	flags = ARGP_IN_ORDER | ARGP_SILENT;
	argp_parse(&argp, argc, argv, flags, 0, *config);
	return expand_config(config, 1);
}

int afb_binder_opts_parse_final(int argc, char **argv, struct json_object **config)
{
	struct argp argp;
	int flags;
	int rc, next = 0;
	struct argp_child *children = NULL;
	struct argp *children_argp = NULL;
	struct children_data *children_data = NULL;
	struct final_data data;

#if WITH_EXTENSION
	struct json_object *root, *obj;
	const char **names;
	const struct argp_option **options;
	int iext;
	next = afb_extend_get_options(&options, &names);
	if (next < 0) {
		LIBAFB_ERROR("Can't get options of extensions");
		exit(1);
	}
	if (next == 0) {
		children = 0;
		children_argp = 0;
		children_data = 0;
	} else {
		children = calloc(1 + next, sizeof *children);
		children_argp = calloc(next, sizeof *children_argp);
		children_data = calloc(next, sizeof *children_data);
		if (!children || !children_argp || !children_data)
			rc = -1;
		else {
			if (!json_object_object_get_ex(*config, "@extconfig", &root)) {
				root = json_object_new_object();
				json_object_object_add(*config, "@extconfig", root);
			}
			for (rc = iext = 0 ; rc >= 0 && iext < next ; iext++) {
				children[iext].argp = &children_argp[iext];
				rc = asprintf((char**)&children[iext].header, "===== EXTENSION %s =====", names[iext]);
				children_argp[iext].options = options[iext];
				children_argp[iext].parser = parsecb_extension;
				if (!json_object_object_get_ex(root, names[iext], &obj)) {
					obj = json_object_new_object();
					json_object_object_add(root, names[iext], obj);
				}
				children_data[iext].options = children_argp[iext].options;
				children_data[iext].config = obj;
			}
		}
		if (rc < 0) {
			LIBAFB_ERROR("Unable to process options of extensions");
			exit(1);
		}
	}

#endif
	data.config = *config;
	data.children_data = children_data;
	data.nchildren = next;
	data.dodump = 0;

	argp.options = optdefs;
	argp.parser = parsecb_final;
	argp.args_doc = "[--exec program args...]";
	argp.doc = docstring;
	argp.children = children;
	argp.help_filter = 0;
	argp.argp_domain = 0;
	argp_program_version = version;
	flags = ARGP_IN_ORDER;
	argp_parse(&argp, argc, argv, flags, 0, &data);

	fulfill_config(*config);
	if (data.dodump == 1) {
		dump(*config, stdout, NULL, NULL);
		exit(0);
	}
	if (afb_verbose_wants(afb_Log_Level_Info) && !afb_verbose_wants(afb_Log_Level_Debug))
		dump(*config, stderr, "--", "CONFIG");
	rc = expand_config(config, 1);
	if (afb_verbose_wants(afb_Log_Level_Debug))
		dump(*config, stderr, "--", "CONFIG");
	if (data.dodump == 2) {
		dump(*config, stdout, NULL, NULL);
		exit(0);
	}

#if WITH_EXTENSION
	for (iext = 0 ; iext < next ; iext++)
		free((char**)children[iext].header);
#endif
	free(children);
	free(children_argp);
	free(children_data);
	return rc;
}
