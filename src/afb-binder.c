/*
 * Copyright (C) 2015-2021 IoT.bzh Company
 * Author "Fulup Ar Foll"
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <argp.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <json-c/json.h>
#include "afb-binder-opts.h"

#if WITH_CALL_PERSONALITY
#include <sys/personality.h>
#endif

#if WITH_SYSTEMD
#   include <systemd/sd-daemon.h>
#endif

#include <libafb/afb-core.h>
#include <libafb/afb-apis.h>
#include <libafb/afb-misc.h>
#include <libafb/afb-extend.h>

#if WITH_LIBMICROHTTPD
#include <libafb/afb-http.h>
#endif

#include <libafb/afb-sys.h>
#include <libafb/afb-utils.h>

#if !defined(DEFAULT_BINDER_INTERFACE)
#  define DEFAULT_BINDER_INTERFACE "*"
#endif

/*
   if SELF_PGROUP == 0 the launched command is the group leader
   if SELF_PGROUP != 0 afb-daemon is the group leader
*/
#define SELF_PGROUP 0

struct afb_apiset *afb_binder_main_apiset;
struct json_object *afb_binder_main_config;
#if WITH_LIBMICROHTTPD
struct afb_hsrv *afb_binder_http_server;
#endif

static pid_t childpid;

#if WITH_ENVIRONMENT
/*----------------------------------------------------------
 |   helpers for setting environment
 +--------------------------------------------------------- */

/**
 * Tiny helper around putenv: add the variable name=value
 *
 * @param name name of the variable to set
 * @param value value to set to the variable
 *
 * @return zero on success or -1 if error
 */
static int addenv(const char *name, const char *value)
{
	return setenv(name, value, 1);
}

/**
 * Tiny helper around addenv that export the real path
 *
 * @param name name of the variable to set
 * @param path the path value to export to the variable
 *
 * @return zero on success or -1 if error
 */
static int addenv_realpath(const char *name, const char *path)
{
	char buffer[PATH_MAX];
	char *p = realpath(path, buffer);
	return p ? addenv(name, p) : -1;
}

/**
 * Tiny helper around addenv that export an integer
 *
 * @param name name of the variable to set
 * @param value the integer value to export
 *
 * @return zero on success or -1 if error
 */
static int addenv_int(const char *name, int value)
{
	char buffer[64];
	snprintf(buffer, sizeof buffer, "%d", value);
	return addenv(name, buffer);
}
#endif

/*----------------------------------------------------------
 |   helpers for handling list of arguments
 +--------------------------------------------------------- */

static const char *run_for_config_array_opt(const char *name,
					    int (*run) (void *closure, const char *value),
					    void *closure)
{
	int i, n, rc;
	struct json_object *array, *value;

	if (json_object_object_get_ex(afb_binder_main_config, name, &array)) {
		if (!json_object_is_type(array, json_type_array))
			return json_object_get_string(array);
		n = (int)json_object_array_length(array);
		for (i = 0 ; i < n ; i++) {
			value = json_object_array_get_idx(array, i);
			rc = run(closure, json_object_get_string(value));
			if (!rc)
				return json_object_get_string(value);
		}
	}
	return NULL;
}

static int run_start(void *closure, const char *value)
{
	int (*starter) (const char *value, struct afb_apiset *declare_set, struct afb_apiset *call_set) = closure;
	return starter(value, afb_binder_main_apiset, afb_binder_main_apiset) >= 0;
}

static void apiset_start_list(const char *name,
			int (*starter) (const char *value, struct afb_apiset *declare_set, struct afb_apiset *call_set),
			const char *message)
{
	const char *item = run_for_config_array_opt(name, run_start, starter);
	if (item) {
		ERROR("can't start %s %s", message, item);
		exit(1);
	}
}

#if WITH_DYNAMIC_BINDING
/**
 * Loads the binding specified by the object 'value'
 *
 * @param closure not used
 * @param value an object describing the binding to load
 */
static void load_one_binding_cb(void *closure, struct json_object *value)
{
	struct json_object *path;
	const char *pathstr;
	int rc;

	/* mainstream type is an object { path, uid, config } */
	if (json_object_is_type(value, json_type_object)
		&& json_object_object_get_ex(value, "path", &path)) {
		pathstr = json_object_get_string(path);
	}
	/* try legacy plain strings if config files for it exist */
	else if (json_object_is_type(value, json_type_string)) {
		pathstr = json_object_get_string(value);
		value = NULL;
	}
	else {
		/* unrecognized binding specification */
		ERROR("unrecognized binding option %s", json_object_get_string(value));
		exit(1);
	}

	/* add the binding now */
	rc = afb_api_so_add_binding_config(pathstr, afb_binder_main_apiset, afb_binder_main_apiset, value);
	if (rc < 0) {
		ERROR("can't load binding %s", pathstr);
		exit(1);
	}
}

/**
 * Load the bindings within the given 'name' of the config.
 *
 * @param name name of the array of bindings to load
 */
static void load_bindings(const char *name)
{
	struct json_object *array;

	/* check if the name exists */
	if (json_object_object_get_ex(afb_binder_main_config, name, &array))
		wrap_json_optarray_for_all(array, load_one_binding_cb, NULL);
}
#endif

/*----------------------------------------------------------
 | exit_handler
 |   Handles on exit specific actions
 +--------------------------------------------------------- */
static void exit_handler()
{
	struct sigaction siga;
	pid_t pidchld = childpid;

	memset(&siga, 0, sizeof siga);
	siga.sa_handler = SIG_IGN;
	sigaction(SIGTERM, &siga, NULL);
	sigaction(SIGCHLD, &siga, NULL);

	childpid = 0;
	if (SELF_PGROUP)
		killpg(0, SIGTERM);
	else if (pidchld > 0)
		killpg(pidchld, SIGTERM);
}

static void on_sigterm(int signum, siginfo_t *info, void *uctx)
{
	NOTICE("Received SIGTERM");
	exit(0);
}

static void on_sighup(int signum, siginfo_t *info, void *uctx)
{
	NOTICE("Received SIGHUP");
	/* TODO */
}

/**
 * Set the handlers
 */
static void setup_handlers()
{
	struct sigaction siga;

	/* install signal handlers */
	memset(&siga, 0, sizeof siga);

	siga.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &siga, NULL);

	siga.sa_flags = SA_SIGINFO;

	siga.sa_sigaction = on_sigterm;
	sigaction(SIGTERM, &siga, NULL);

	siga.sa_sigaction = on_sighup;
	sigaction(SIGHUP, &siga, NULL);

	/* handle groups */
	atexit(exit_handler);
}

/*----------------------------------------------------------
 | daemonize
 |   set the process in background
 +--------------------------------------------------------- */
static void daemonize()
{
	int fd = 0, daemon, nostdin;
	const char *output;
	pid_t pid;

	daemon = 0;
	output = NULL;
	wrap_json_unpack(afb_binder_main_config, "{s?b s?s}", "daemon", &daemon, "output", &output);
	nostdin = 0;

	if (output) {
		fd = open(output, O_WRONLY | O_APPEND | O_CREAT, 0640);
		if (fd < 0) {
			ERROR("Can't open output %s", output);
			exit(1);
		}
	}

	if (daemon) {
		INFO("entering background mode");

		pid = fork();
		if (pid == -1) {
			ERROR("Failed to fork daemon process");
			exit(1);
		}
		if (pid != 0)
			_exit(0);

		nostdin = 1;
	}

	/* closes the input */
	if (output) {
		NOTICE("Redirecting output to %s", output);
		close(2);
		dup(fd);
		close(1);
		dup(fd);
		close(fd);
	}

	if (nostdin)
		close(0); /* except if 'daemon', ctrl+C still works after that */
}

/*---------------------------------------------------------
 | http server
 |   Handles the HTTP server
 +--------------------------------------------------------- */
#if WITH_LIBMICROHTTPD

static int add_alias(struct afb_hsrv *hsrv, const char *prefix, const char *alias, int priority, int relax)
{
#if WITH_OPENAT
	return afb_hsrv_add_alias(hsrv, prefix, afb_common_rootdir_get_fd(), alias, priority, relax);
#else
	return afb_hsrv_add_alias_path(hsrv, prefix, afb_common_rootdir_get_path(), alias, priority, relax);
#endif
}

static int add_alias_weak(struct afb_hsrv *hsrv, const char *prefix, const char *alias, int priority, int relax)
{
	return access(alias, R_OK) != 0 || add_alias(hsrv, prefix, alias, priority, relax);
}

static int add_alias_weak_dir(struct afb_hsrv *hsrv, const char *prefix, const char *alias, int priority, int relax)
{
	char * dirname = expand_vars_env_only(alias, 0);
	int rc = afb_hsrv_add_alias_dirname(hsrv, prefix, dirname ?: alias, priority, relax);
	free(dirname);
	return rc;
}

static int init_alias(void *closure, const char *spec)
{
	struct afb_hsrv *hsrv = closure;
	char *path = strchr(spec, ':');

	if (path == NULL) {
		ERROR("Missing ':' in alias %s. Alias ignored", spec);
		return 1;
	}
	*path++ = 0;
	INFO("Alias for url=%s to path=%s", spec, path);
	return add_alias(hsrv, spec, path, 0, 1);
}

static int add_interface(void *closure, const char *value)
{
	struct afb_hsrv *hsrv = closure;
	int rc;

	rc = afb_hsrv_add_interface(hsrv, value);
	return rc >= 0;
}

static int http_server_create(struct afb_hsrv **result)
{
	int rc;
	const char *uploaddir, *rootdir, *itfspec;
	const char *rootapi, *roothttp = NULL, *rootbase, *tmp;
	struct afb_hsrv *hsrv = NULL;
	struct json_object *itfs = NULL;
	int cache_timeout, http_port = -1;
	int session_timeout;
	int no_httpd = 0, is_https = 0;
	struct json_object *obj;

	/* default result: NULL */
	*result = NULL;

	/* read parameters */
	http_port = -1;
	rc = wrap_json_unpack(afb_binder_main_config, "{ss ss si s?i ss s?b s?b si ss s?s s?o}",
				"uploaddir", &uploaddir,
				"rootdir", &rootdir,
				"cache-eol", &cache_timeout,

				"port", &http_port,
				"rootapi", &rootapi,
				"no-httpd", &no_httpd,
				"https", &is_https,

				"cntxtimeout", &session_timeout,
				"rootbase", &rootbase,
				"roothttp", &roothttp,
				"interface", &itfs
			);
	if (rc < 0) {
		ERROR("Can't get HTTP server config");
		exit(1);
	}

	/* is http service allowed ? */
	if (no_httpd) {
		return 0;
	}

	/* checking port */
	if (http_port < 0) {
		/* not set, check existing interfaces */
		obj = itfs;
		if (json_object_get_type(obj) == json_type_array)
			obj = json_object_array_length(obj) ? json_object_array_get_idx(obj, 0) : NULL;
		if (obj == NULL) {
			ERROR("No port and no interface ");
			rc= X_EINVAL;
			goto end;
		}
		itfspec = json_object_get_string(obj);
		rc = 0;
		while (itfspec[rc] && itfspec[rc] != '/') rc++;
		while (rc && itfspec[rc - 1] != ':') rc--;
		http_port = atoi(&itfspec[rc]);

	} else if (http_port == 0) {
		ERROR("random port is not implemented");
		rc= X_EINVAL;
		goto end;
	} else {
		char buffer[1000];

		/* normalize itf as array */
		if (itfs == NULL || json_object_get_type(itfs) != json_type_array) {
			obj = json_object_new_array();
			if (obj == NULL) {
				rc = X_ENOMEM;
				goto end;
			}
			if (itfs != NULL)
				json_object_array_add(obj, json_object_get(itfs));
			json_object_object_add(afb_binder_main_config, "interface", obj);
			itfs = obj;
		}

		/* add the default interface */
		rc = snprintf(buffer, sizeof buffer, "tcp:%s:%d", DEFAULT_BINDER_INTERFACE, http_port);
		if (rc < 0 || rc >= sizeof buffer) {
			rc = X_ENOMEM;
			goto end;
		}

		obj = json_object_new_string(buffer);
		if (obj == NULL) {
			rc = X_ENOMEM;
			goto end;
		}

		json_object_array_add(itfs, obj);
		NOTICE("Browser URL= http%s://localhost:%d", "s"+!is_https, http_port);
	}
#if WITH_ENVIRONMENT
	if (http_port && addenv_int("AFB_PORT", http_port) < 0) {
		ERROR("can't set HTTP environment");
		goto error;
	}
#endif

	/* setting up */
	NOTICE("Serving rootdir=%s uploaddir=%s", rootdir, uploaddir);

	/* setup of cookies */
	if (!afb_hreq_init_cookie(http_port, 0, session_timeout)) {
		ERROR("initialisation of HTTP cookies failed");
		rc = X_ENOMEM;
		goto end;
	}

	/* setup of download directory */
	rc = afb_hreq_init_download_path(uploaddir);
	if (rc < 0) {
		static const char fallback_uploaddir[] = "/tmp";
		WARNING("unable to set the upload directory %s", uploaddir);
		rc = afb_hreq_init_download_path(fallback_uploaddir);
		if (rc < 0) {
			ERROR("unable to fallback to upload directory %s", fallback_uploaddir);
			goto end;
		}
		uploaddir = fallback_uploaddir;
	}

	/* setup of download directory */
	hsrv = afb_hsrv_create();
	if (hsrv == NULL) {
		ERROR("memory allocation failure");
		rc = X_ENOMEM;
		goto end;
	}

	/* initialize the cache timeout */
	if (!afb_hsrv_set_cache_timeout(hsrv, cache_timeout))
		goto error;

	/* set the root api handlers */
	if (!afb_hsrv_add_handler(hsrv, rootapi,
			afb_hswitch_websocket_switch, afb_binder_main_apiset, 20))
		goto error;
	if (!afb_hsrv_add_handler(hsrv, rootapi,
			afb_hswitch_apis, afb_binder_main_apiset, 10))
		goto error;

	/* set alias of config */
	if (run_for_config_array_opt("alias", init_alias, hsrv))
		goto error;

	/* set predefined aliases */
	if (!add_alias_weak(hsrv, "/devtools", DEVTOOLS_INSTALL_DIR, 0, 1))
		goto error;
	if (!add_alias_weak(hsrv, "/.well-known", WELL_KNOWN_DIR, -15, 1))
		goto error;
	tmp = secure_getenv("AFB_BINDER_WELL_KNOWN_DIR");
	if (tmp && !add_alias_weak(hsrv, "/.well-known", tmp, -13, 1))
		goto error;
	if (!add_alias_weak_dir(hsrv, "/download", "${AFB_WORKDIR}/download", -15, 1))
		goto error;
	tmp = secure_getenv("AFB_BINDER_DOWNLOAD_DIR");
	if (tmp && !add_alias_weak_dir(hsrv, "/download", tmp, -13, 1))
		goto error;

	/* set the root of http */
	if (roothttp != NULL && !add_alias(hsrv, "", roothttp, -10, 1))
		goto error;

	/* set the root of One Page Applications */
	if (!afb_hsrv_add_handler(hsrv, rootbase,
			afb_hswitch_one_page_api_redirect, NULL, -20))
		goto error;

	*result = hsrv;
	return 0;

error:
	ERROR("initialisation of httpd failed");
	rc = X_ECANCELED;
	afb_hsrv_put(hsrv);
end:
	*result = NULL;
	return rc;
}

static int readfile(const char *filename, char **buffer, size_t *size)
{
	int rc, fd;
	off_t of;
	size_t sz;
	char *buf;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		rc = -errno;
	else {
		of = lseek(fd, 0, SEEK_END);
		if (of < 0 || lseek(fd, 0, SEEK_SET) < 0)
			rc = -errno;
		else if (of >= SIZE_MAX)
			rc = X_E2BIG;
		else {
			sz = (size_t)of;
			buf = malloc(sz + 1);
			if (buf == NULL)
				rc = X_ENOMEM;
			else if (read(fd, buf, sz) != (ssize_t)sz) {
				rc = -errno;
				free(buf);
			}
			else {
				buf[sz] = 0;
				*buffer = buf;
				if (size)
					*size = sz;
				rc = 0;
			}
		}
		close(fd);
	}
	return rc;
}

static int get_https_value(const char *key, const char *filename, char **value)
{
	char buffer[PATH_MAX];
	int rc;
	if (!filename) {
		filename = secure_getenv("AFB_HTTPS_PREFIX") ?: SYS_CONFIG_DIR "/.https/";
		rc = snprintf(buffer, sizeof buffer, "%s%s.pem", filename, key);
		if (rc < 0 || rc >= sizeof buffer)
			return X_EINVAL;
		filename = buffer;
	}
	rc = readfile(filename, value, NULL);
	if (rc < 0) {
		ERROR("can't read file %s: %s", filename, strerror(-rc));
		return rc;
	}
	return 0;
}

static int get_https_config(char **key, char **cert)
{
	int rc, is_https;
	const char *okey, *ocert;

	/* read HTTPS parameters if any */
	is_https = 0;
	ocert = okey = *key = *cert = NULL;
	rc = wrap_json_unpack(afb_binder_main_config, "{s?b s?s s?s}",
				"https", &is_https,
				"https-key", &okey,
				"https-cert", &ocert
			);
	if (rc < 0)
		rc = X_ECANCELED;
	else if (!is_https) {
		if (okey || ocert)
			WARNING("HTTPS disabled but option set for HTTPS certificate and/or key");
		rc = 0; /* no https */
	}
	else {
		rc = get_https_value("key", okey, key);
		if (rc >= 0) {
			rc = get_https_value("cert", ocert, cert);
			if (rc >= 0)
				rc = 1; /* yes https */
		}
	}
	return rc;
}

static int http_server_start(struct afb_hsrv *hsrv)
{
	int rc;
	const char *errs;
	char *key, *cert;

	/* get HTTPS config if any */
	rc = get_https_config(&key, &cert);
	if (rc < 0) {
		ERROR("wrong HTTPS configuration");
		return rc;
	}

	/* start the daemon */
	INFO("HTTP%s server starting", "S"+!rc);
	rc = afb_hsrv_start_tls(hsrv, 15, cert, key);
	if (!rc) {
		ERROR("starting of httpd failed");
		return X_ECANCELED;
	}

	/* add interfaces */
	errs = run_for_config_array_opt("interface", add_interface, hsrv);
	if (errs) {
		ERROR("setting interface %s failed", errs);
		return X_ECANCELED;
	}

	return 0;
}
#endif

/*---------------------------------------------------------
 | execute_command
 +--------------------------------------------------------- */

static void exit_at_end()
{
	exit(0);
}

static void wait_child(int signum, void* arg)
{
	pid_t pid = (pid_t)(intptr_t)arg;
	pid_t pidchld = childpid;

	if (pidchld == pid) {
		childpid = 0;
		if (!SELF_PGROUP)
			killpg(pidchld, SIGKILL);
		waitpid(pidchld, NULL, 0);
		afb_sched_exit(0, exit_at_end);
	} else {
		waitpid(pid, NULL, 0);
	}
}

static void on_sigchld(int signum, siginfo_t *info, void *uctx)
{
	if (info->si_pid == childpid) {
		switch (info->si_code) {
		case CLD_EXITED:
		case CLD_KILLED:
		case CLD_DUMPED:
			afb_sched_post_job(0, 0, 0, wait_child, (void*)(intptr_t)info->si_pid);
		default:
			break;
		}
	}
}

/*
# @@ @
# @p port
*/

#define SUBST_CHAR  '@'
#define SUBST_STR   "@"

static char *instanciate_string(char *arg, const char *port)
{
	char *resu, *it, *wr, c;
	int chg, dif;

	/* get the changes */
	chg = 0;
	dif = 0;
	it = strchrnul(arg, SUBST_CHAR);
	while (*it) {
		c = *++it;
		if (c == 'p' && port) {
			chg++;
			dif += (int)strlen(port) - 2;
		} else if (c == SUBST_CHAR) {
			it++;
			chg++;
			dif--;
		}
		it = strchrnul(it, SUBST_CHAR);
	}

	/* return arg when no change */
	if (!chg)
		return arg;

	/* allocates the result */
	resu = malloc((it - arg) + dif + 1);
	if (!resu) {
		ERROR("out of memory");
		return NULL;
	}

	/* instanciate the arguments */
	wr = resu;
	for (;;) {
		it = strchrnul(arg, SUBST_CHAR);
		wr = mempcpy(wr, arg, it - arg);
		if (!*it)
			break;
		c = *++it;
		if (c == 'p' && port)
			wr = stpcpy(wr, port);
		else {
			if (c != SUBST_CHAR)
				*wr++ = SUBST_CHAR;
			*wr++ = *it;
		}
		arg = ++it;
	}

	*wr = 0;
	return resu;
}

static int instanciate_environ(const char *port)
{
	extern char **environ;
	char *repl;
	int i;

	/* instantiate the environment */
	for (i = 0 ; environ[i] ; i++) {
		repl = instanciate_string(environ[i], port);
		if (!repl)
			return -1;
		environ[i] = repl;
	}
	return 0;
}

static char **instanciate_command_args(struct json_object *exec, const char *port)
{
	char **result;
	char *repl, *item;
	int i, n;

	/* allocates the result */
	n = (int)json_object_array_length(exec);
	result = malloc((n + 1) * sizeof * result);
	if (!result) {
		ERROR("out of memory");
		return NULL;
	}

	/* instanciate the arguments */
	for (i = 0 ; i < n ; i++) {
		item = (char*)json_object_get_string(json_object_array_get_idx(exec, i));
		repl = instanciate_string(item, port);
		if (!repl) {
			free(result);
			return NULL;
		}
		result[i] = repl;
	}
	result[i] = NULL;
	return result;
}

static int execute_command()
{
	struct json_object *exec, *oport;
	struct sigaction siga;
	const char *port;
	char **args;

	/* check whether a command is to execute or not */
	if (!json_object_object_get_ex(afb_binder_main_config, "exec", &exec))
		return 0;

	if (SELF_PGROUP)
		setpgid(0, 0);

	/* install signal handler */
	memset(&siga, 0, sizeof siga);
	siga.sa_sigaction = on_sigchld;
	siga.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD, &siga, NULL);

	/* fork now */
	childpid = fork();
	if (childpid)
		return 0;

	/* compute the string for port */
	if (json_object_object_get_ex(afb_binder_main_config, "port", &oport))
		port = json_object_get_string(oport);
	else
		port = getenv("AFB_PORT");
	/* instantiate arguments and environment */
	args = instanciate_command_args(exec, port);
	if (args && instanciate_environ(port) >= 0) {
		/* run */
		if (!SELF_PGROUP)
			setpgid(0, 0);
		execv(args[0], args);
		ERROR("can't launch %s: %m", args[0]);
	}
	exit(1);
	return -1;
}

/*---------------------------------------------------------
 | startup calls
 +--------------------------------------------------------- */

struct startup_req
{
	struct afb_req_common comreq;
	char *api;
	char *verb;
	struct json_object *calls;
	int index;
	int count;
	const char *callspec;
	struct afb_session *session;
};

static void startup_call_reply(struct afb_req_common *comreq, int status, unsigned nreplies, struct afb_data * const *replies)
{
	struct startup_req *sreq = containerof(struct startup_req, comreq, comreq);

	if (status >= 0) {
		NOTICE("startup call %s returned %d", sreq->callspec, status);
	} else {
		ERROR("startup call %s ERROR! %d", sreq->callspec, status);
		exit(1);
	}
}

static void startup_call_current(struct startup_req *sreq);

static void startup_call_unref(struct afb_req_common *comreq)
{
	struct startup_req *sreq = containerof(struct startup_req, comreq, comreq);

	free(sreq->api);
	free(sreq->verb);
	afb_req_common_cleanup(&sreq->comreq);
	if (++sreq->index < sreq->count)
		startup_call_current(sreq);
	else {
		afb_session_close(sreq->session);
		afb_session_unref(sreq->session);
		free(sreq);
	}
}

static struct afb_req_common_query_itf startup_req_common_itf =
{
	.reply = startup_call_reply,
	.unref = startup_call_unref
};

static void startup_call_current(struct startup_req *sreq)
{
	const char *api, *verb, *json;
	struct afb_data *arg0;
	int rc;

	sreq->callspec = json_object_get_string(json_object_array_get_idx(sreq->calls, sreq->index)),
	api = sreq->callspec;
	verb = strchr(api, '/');
	if (verb) {
		json = strchr(verb, ':');
		if (json) {
			sreq->api = strndup(api, verb - api);
			sreq->verb = strndup(verb + 1, json - verb - 1);

			rc = afb_data_create_raw(&arg0, &afb_type_predefined_json, json + 1, strlen(json), 0, 0);
			if (sreq->api && sreq->verb && rc >= 0) {
				afb_req_common_init(&sreq->comreq, &startup_req_common_itf, sreq->api, sreq->verb, 1, &arg0);
				afb_req_common_set_session(&sreq->comreq, sreq->session);
				sreq->comreq.validated = 1;
				afb_req_common_process(&sreq->comreq, afb_binder_main_apiset);
				return;
			}
		}
	}
	ERROR("Bad call specification %s", sreq->callspec);
	exit(1);
}

static void run_startup_calls()
{
	struct json_object *calls;
	struct startup_req *sreq;
	int count;

	if (json_object_object_get_ex(afb_binder_main_config, "call", &calls)
	 && json_object_is_type(calls, json_type_array)
	 && (count = (int)json_object_array_length(calls))) {
		sreq = calloc(1, sizeof *sreq);
		afb_session_create(&sreq->session, 3600);
		sreq->calls = calls;
		sreq->index = 0;
		sreq->count = count;
		startup_call_current(sreq);
	}
}

/**
 * Set directories
 */
static void setup_directories()
{
	struct json_object *obj;
	const char *workdir, *rootdir, *str;

	/* get the directories */
	workdir = ".";
	if (json_object_object_get_ex(afb_binder_main_config, "workdir", &obj)
	 && json_object_is_type(obj, json_type_string))
		workdir = json_object_get_string(obj);

	rootdir = ".";
	if (json_object_object_get_ex(afb_binder_main_config, "rootdir", &obj)
	 && json_object_is_type(obj, json_type_string))
		rootdir = json_object_get_string(obj);

#if WITH_ENVIRONMENT
	str = getenv("PWD");
	addenv_realpath("OLDPWD", str ?: ".");
#endif
	/* set the directories */
	mkdir(workdir, S_IRWXU | S_IRGRP | S_IXGRP);
	if (chdir(workdir) < 0) {
		ERROR("Can't enter working dir %s", workdir);
		exit(1);
	}
	if (afb_common_rootdir_set(rootdir) < 0) {
		ERROR("failed to set common root directory %s", rootdir);
		exit(1);
	}
#if WITH_ENVIRONMENT
	if (addenv_realpath("AFB_WORKDIR", "."     /* resolved by realpath */) < 0
	 || addenv_realpath("PWD", "."     /* resolved by realpath */) < 0
	 || addenv_realpath("AFB_ROOTDIR", rootdir /* relative to current directory */) < 0) {
		ERROR("can't set DIR environment");
		exit(1);
	}
#endif
}

/*---------------------------------------------------------
 | job starting the binder
 +--------------------------------------------------------- */

static void start(int signum, void *arg)
{
#if WITH_AFB_HOOK
	const char *tracereq = NULL, *traceapi = NULL, *traceevt = NULL;
	const char *traceses = NULL, *traceglob = NULL;
	unsigned flags;
#endif
	struct json_object *settings = NULL;
	int max_session_count, session_timeout, api_timeout;
	int rc;

#if WITH_AFB_DEBUG
	afb_debug("start-entry");
#endif

	if (signum) {
		ERROR("start aborted: received signal %s", strsignal(signum));
		exit(1);
	}

	setup_directories();

	rc = wrap_json_unpack(afb_binder_main_config, "{"
			"si si si"
			"s?o"
			"}",

			"apitimeout", &api_timeout,
			"cntxtimeout", &session_timeout,
			"session-max", &max_session_count,

			"set", &settings
			);
	if (rc < 0) {
		ERROR("Unable to get start config");
		exit(1);
	}

#if WITH_AFB_HOOK
	rc = wrap_json_unpack(afb_binder_main_config, "{"
			"s?s s?s s?s s?s s?s"
			"}",

			"tracereq", &tracereq,
			"traceapi", &traceapi,
			"traceevt", &traceevt,
			"traceses",  &traceses,
			"traceglob", &traceglob
			);
	if (rc < 0) {
		ERROR("Unable to get hook config");
		exit(1);
	}
#endif

	/* initialize session handling */
	if (afb_session_init(max_session_count, session_timeout)) {
		ERROR("initialisation of session manager failed");
		goto error;
	}

	/* configure the daemon */
	afb_api_common_set_config(settings);
	afb_binder_main_apiset = afb_apiset_create("main", api_timeout);
	if (!afb_binder_main_apiset) {
		ERROR("can't create main api set");
		goto error;
	}
	afb_global_api_init(afb_binder_main_apiset);
	rc = afb_monitor_init(afb_binder_main_apiset, afb_binder_main_apiset);
	if (rc < 0) {
		ERROR("failed to setup monitor");
		goto error;
	}
#if WITH_SUPERVISION
	rc = afb_supervision_init(afb_binder_main_apiset, afb_binder_main_config);
	if (rc < 0) {
		ERROR("failed to setup supervision");
		goto error;
	}
#endif

#if WITH_AFB_HOOK
	/* install hooks */
	if (tracereq) {
		rc = afb_hook_flags_req_from_text(tracereq, &flags);
		if (rc < 0) {
			ERROR("invalid tracereq spec '%s'", tracereq);
			goto error;
		}
		afb_hook_create_req(NULL, NULL, NULL, flags, NULL, NULL);
	}
	if (traceapi) {
		rc = afb_hook_flags_api_from_text(traceapi, &flags);
		if (rc < 0) {
			ERROR("invalid traceapi spec '%s'", traceapi);
			goto error;
		}
		afb_hook_create_api(NULL, flags, NULL, NULL);
	}
	if (traceevt) {
		rc = afb_hook_flags_evt_from_text(traceevt, &flags);
		if (rc < 0) {
			ERROR("invalid traceevt spec '%s'", traceevt);
			goto error;
		}
		afb_hook_create_evt(NULL, flags, NULL, NULL);
	}
	if (traceses) {
		rc = afb_hook_flags_session_from_text(traceses, &flags);
		if (rc < 0) {
			ERROR("invalid traceses spec '%s'", traceses);
			goto error;
		}
		afb_hook_create_session(NULL, flags, NULL, NULL);
	}
	if (traceglob) {
		rc = afb_hook_flags_global_from_text(traceglob, &flags);
		if (rc < 0) {
			ERROR("invalid traceglob spec '%s'", traceglob);
			goto error;
		}
		afb_hook_create_global(flags, NULL, NULL);
	}
#endif

#if WITH_EXTENSION
	/* prepare extensions */
	rc = afb_extend_config(afb_binder_main_config);
	if (rc < 0) {
		ERROR("Extension config failed");
		goto error;
	}
#endif

#if WITH_LIBMICROHTTPD
	rc = http_server_create(&afb_binder_http_server);
	if (rc < 0) {
		ERROR("can't create HTTP server");
		goto error;
	}
#endif

	/* load bindings and apis */
#if WITH_AFB_DEBUG
	afb_debug("start-load");
#endif
#if WITH_DYNAMIC_BINDING
	load_bindings("binding");
#if WITH_DIRENT
	apiset_start_list("ldpaths", afb_api_so_add_pathset_fails, "the binding path set");
	apiset_start_list("weak-ldpaths", afb_api_so_add_pathset_nofails, "the weak binding path set");
#endif
#endif
	apiset_start_list("auto-api", afb_autoset_add_any, "the automatic api path set");
#if WITH_DBUS_TRANSPARENCY
	apiset_start_list("dbus-client", afb_api_dbus_add_client, "the afb-dbus client");
#endif
	apiset_start_list("ws-client", afb_api_ws_add_client_weak, "the afb-websocket client");
#if WITH_EXTENSION
	/* declare extensions */
	rc = afb_extend_declare(afb_binder_main_apiset, afb_binder_main_apiset);
	if (rc < 0) {
		ERROR("Extension declare failed");
		goto error;
	}
#if WITH_LIBMICROHTTPD
	rc = afb_extend_http(afb_binder_http_server);
	if (rc < 0) {
		ERROR("Extension http failed");
		goto error;
	}
#endif
#endif

	DEBUG("Init config done");

	/* start the services */
#if WITH_AFB_DEBUG
	afb_debug("start-start");
#endif
#if WITH_CALL_PERSONALITY
	personality((unsigned long)-1L);
#endif
	rc = afb_apiset_start_all_services(afb_binder_main_apiset);
	if (rc < 0) {
		ERROR("Services start failed");
		goto error;
	}

	/* export started apis */
	apiset_start_list("ws-server", afb_api_ws_add_server, "the afb-websocket service");
#if WITH_DBUS_TRANSPARENCY
	apiset_start_list("dbus-server", afb_api_dbus_add_server, "the afb-dbus service");
#endif
#if WITH_EXTENSION
	/* start extensions */
	rc = afb_extend_serve(afb_binder_main_apiset);
	if (rc < 0) {
		ERROR("Extension serve failed");
		goto error;
	}
#endif

	/* start the HTTP server */
#if WITH_LIBMICROHTTPD
#if WITH_AFB_DEBUG
	afb_debug("start-http");
#endif
	if (afb_binder_http_server != NULL) {
		rc = http_server_start(afb_binder_http_server);
		if (rc < 0)
			goto error;
	}
#endif

	/* run the startup calls */
#if WITH_AFB_DEBUG
	afb_debug("start-call");
#endif
	run_startup_calls();

	/* run the command */
#if WITH_AFB_DEBUG
	afb_debug("start-exec");
#endif
	if (execute_command() < 0)
		goto error;

	/* ready */
#if WITH_SYSTEMD
	sd_notify(1, "READY=1");
#endif

	/* activate the watchdog */
#if HAS_WATCHDOG
	if (afb_watchdog_activate() < 0)
		ERROR("can't start the watchdog");
#endif

	return;
error:
	exit(1);
}

/*---------------------------------------------------------
 | main
 |   Parse option and launch action
 +--------------------------------------------------------- */

int main(int argc, char *argv[])
{
	struct json_object *obj;
	int njobs, nthr;

#if WITH_AFB_DEBUG
	afb_debug("main-entry");
#endif

	// ------------- Build session handler & init config -------
	afb_binder_main_config = json_object_new_object();
	afb_binder_opts_parse_initial(argc, argv, &afb_binder_main_config);

#if WITH_EXTENSION
	/* load extensions */
	if (afb_extend_load(afb_binder_main_config) < 0) {
		ERROR("loading extension failed");
		return 1;
	}
#endif

	afb_binder_opts_parse_final(argc, argv, &afb_binder_main_config);

	if (afb_sig_monitor_init(
		!json_object_object_get_ex(afb_binder_main_config, "trap-faults", &obj)
			|| json_object_get_boolean(obj)) < 0) {
		ERROR("failed to initialise signal handlers");
		return 1;
	}

	if (json_object_object_get_ex(afb_binder_main_config, "name", &obj)) {
		verbose_set_name(json_object_get_string(obj), 0);
		process_name_set_name(json_object_get_string(obj));
		process_name_replace_cmdline(argv, json_object_get_string(obj));
	}
#if WITH_AFB_DEBUG
	afb_debug("main-args");
#endif

	// --------- run -----------
	daemonize();
	INFO("running with pid %d", getpid());

	/* set the daemon environment */
	setup_handlers();

#if WITH_AFB_DEBUG
	afb_debug("main-start");
#endif

	njobs = DEFAULT_JOBS_MAX;
	if (json_object_object_get_ex(afb_binder_main_config, "jobs-max", &obj)) {
		njobs = json_object_get_int(obj);
		if (njobs < DEFAULT_JOBS_MIN)
			njobs = DEFAULT_JOBS_MIN;
	}
	nthr = DEFAULT_THREADS_MAX;
	if (json_object_object_get_ex(afb_binder_main_config, "threads-max", &obj)) {
		nthr = json_object_get_int(obj);
		if (nthr < DEFAULT_THREADS_MIN)
			nthr = DEFAULT_THREADS_MIN;
	}

	/* enter job processing */
	afb_sched_start(nthr, 0, njobs, start, NULL);
	WARNING("hoops returned from jobs_enter! [report bug]");
	return 1;
}
