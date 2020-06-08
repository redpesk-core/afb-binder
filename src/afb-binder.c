/*
 * Copyright (C) 2015-2020 IoT.bzh Company
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

#include <libafb/core/afb-apiset.h>
#include <libafb/core/afb-api-common.h>
#include <libafb/core/afb-req-common.h>
#include <libafb/core/afb-req-reply.h>
#include <libafb/core/afb-session.h>
#include <libafb/core/afb-common.h>
#include <libafb/core/afb-jobs.h>
#include <libafb/core/afb-sched.h>
#include <libafb/core/afb-sig-monitor.h>
#include <libafb/core/containerof.h>
#if WITH_AFB_HOOK
#   include <libafb/core/afb-hook.h>
#   include <libafb/core/afb-hook-flags.h>
#endif
#include <libafb/misc/afb-monitor.h>

#include <libafb/apis/afb-api-ws.h>
#include <libafb/misc/afb-autoset.h>
#include <libafb/apis/afb-api-so.h>
#if WITH_DBUS_TRANSPARENCY
#   include <libafb/apis/afb-api-dbus.h>
#endif
#if WITH_AFB_DEBUG
#   include <libafb/misc/afb-debug.h>
#endif
#if WITH_SUPERVISION
#   include <libafb/misc/afb-supervision.h>
#endif
#if WITH_LIBMICROHTTPD
#   include <libafb/http/afb-hsrv.h>
#   include <libafb/http/afb-hreq.h>
#   include <libafb/http/afb-hswitch.h>
#endif

#include <libafb/sys/process-name.h>
#include <libafb/utils/wrap-json.h>
#include <libafb/sys/watchdog.h>
#include <libafb/sys/verbose.h>
#include <libafb/sys/x-realpath.h>

#if !defined(DEFAULT_BINDER_INTERFACE)
#  define DEFAULT_BINDER_INTERFACE NULL
#endif

/*
   if SELF_PGROUP == 0 the launched command is the group leader
   if SELF_PGROUP != 0 afb-daemon is the group leader
*/
#define SELF_PGROUP 0

struct afb_apiset *main_apiset;
struct json_object *main_config;

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
 * @return the created entry or NULL or memory depletion
 */
static char *addenv(const char *name, const char *value)
{
	char *head, *middle;

	head = malloc(2 + strlen(name) + strlen(value));
	if (head) {
		middle = stpcpy(head, name);
		middle[0] = '=';
		strcpy(&middle[1], value);
		if (putenv(head)) {
			free(head);
			head = 0;
		}
	}
	return head;
}

/**
 * Tiny helper around addenv that export the real path
 *
 * @param name name of the variable to set
 * @param path the path value to export to the variable
 *
 * @return the created entry or NULL or memory depletion
 */
static char *addenv_realpath(const char *name, const char *path)
{
	char buffer[PATH_MAX];
	char *p = realpath(path, buffer);
	return p ? addenv(name, p) : 0;
}

/**
 * Tiny helper around addenv that export an integer
 *
 * @param name name of the variable to set
 * @param value the integer value to export
 *
 * @return the created entry or NULL or memory depletion
 */
static char *addenv_int(const char *name, int value)
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

	if (json_object_object_get_ex(main_config, name, &array)) {
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
	return starter(value, main_apiset, main_apiset) >= 0;
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

static void setup_daemon()
{
	struct sigaction siga;

	/* install signal handlers */
	memset(&siga, 0, sizeof siga);
	siga.sa_flags = SA_SIGINFO;

	siga.sa_sigaction = on_sigterm;
	sigaction(SIGTERM, &siga, NULL);

	siga.sa_sigaction = on_sighup;
	sigaction(SIGHUP, &siga, NULL);

	/* handle groups */
	atexit(exit_handler);

	/* ignore any SIGPIPE */
	signal(SIGPIPE, SIG_IGN);
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
	wrap_json_unpack(main_config, "{s?b s?s}", "daemon", &daemon, "output", &output);
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
#if WITH_OPENAT
	return afb_hsrv_add_alias(hsrv, spec, afb_common_rootdir_get_fd(), path,
				  0, 1);
#else
	return afb_hsrv_add_alias_path(hsrv, spec, afb_common_rootdir_get_path(), path,
				  0, 1);
#endif
}

static int init_http_server(struct afb_hsrv *hsrv)
{
	int rc;
	const char *rootapi, *roothttp, *rootbase;

	roothttp = NULL;
	rc = wrap_json_unpack(main_config, "{ss ss s?s}",
				"rootapi", &rootapi,
				"rootbase", &rootbase,
				"roothttp", &roothttp);
	if (rc < 0) {
		ERROR("Can't get HTTP server config");
		exit(1);
	}

	if (!afb_hsrv_add_handler(hsrv, rootapi,
			afb_hswitch_websocket_switch, main_apiset, 20))
		return 0;

	if (!afb_hsrv_add_handler(hsrv, rootapi,
			afb_hswitch_apis, main_apiset, 10))
		return 0;

	if (run_for_config_array_opt("alias", init_alias, hsrv))
		return 0;

	if (roothttp != NULL) {
#if WITH_OPENAT
		if (!afb_hsrv_add_alias(hsrv, "",
			afb_common_rootdir_get_fd(), roothttp, -10, 1))
#else
		if (!afb_hsrv_add_alias_path(hsrv, "",
			afb_common_rootdir_get_path(), roothttp, -10, 1))
#endif
			return 0;
	}

	if (!afb_hsrv_add_handler(hsrv, rootbase,
			afb_hswitch_one_page_api_redirect, NULL, -20))
		return 0;

	return 1;
}

static int add_interface(void *closure, const char *value)
{
	struct afb_hsrv *hsrv = closure;
	int rc;

	rc = afb_hsrv_add_interface(hsrv, value);
	return rc > 0;
}

static struct afb_hsrv *start_http_server()
{
	int rc;
	const char *uploaddir, *rootdir, *errs;
	struct afb_hsrv *hsrv;
	int cache_timeout, http_port;
	struct json_object *junk;

	http_port = -1;
	rc = wrap_json_unpack(main_config, "{ss ss si s?i}",
				"uploaddir", &uploaddir,
				"rootdir", &rootdir,
				"cache-eol", &cache_timeout,
				"port", &http_port);
	if (rc < 0) {
		ERROR("Can't get HTTP server start config");
		exit(1);
	}

	if (afb_hreq_init_download_path(uploaddir)) {
		static const char fallback_uploaddir[] = "/tmp";
		WARNING("unable to set the upload directory %s", uploaddir);
		if (afb_hreq_init_download_path(fallback_uploaddir)) {
			ERROR("unable to fallback to upload directory %s", fallback_uploaddir);
			return NULL;
		}
		uploaddir = fallback_uploaddir;
	}

	hsrv = afb_hsrv_create();
	if (hsrv == NULL) {
		ERROR("memory allocation failure");
		return NULL;
	}

	if (!afb_hsrv_set_cache_timeout(hsrv, cache_timeout)
	    || !init_http_server(hsrv)) {
		ERROR("initialisation of httpd failed");
		afb_hsrv_put(hsrv);
		return NULL;
	}

	rc = afb_hsrv_start(hsrv, 15);
	if (!rc) {
		ERROR("starting of httpd failed");
		afb_hsrv_put(hsrv);
		return NULL;
	}

	NOTICE("Serving rootdir=%s uploaddir=%s", rootdir, uploaddir);

	/* check if port is set */
	if (http_port < 0) {
		/* not set, check existing interfaces */
		if (!json_object_object_get_ex(main_config, "interface", &junk)) {
			ERROR("No port and no interface ");
		}
	} else {
		rc = afb_hsrv_add_interface_tcp(hsrv, DEFAULT_BINDER_INTERFACE, (uint16_t) http_port);
		if (rc < 0) {
			ERROR("setting interface failed");
			afb_hsrv_put(hsrv);
			return NULL;
		}
		NOTICE("Browser URL= http://localhost:%d", http_port);
	}

	errs = run_for_config_array_opt("interface", add_interface, hsrv);
	if (errs) {
		ERROR("setting interface %s failed", errs);
		afb_hsrv_put(hsrv);
		return NULL;
	}

	return hsrv;
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
		afb_sched_exit(exit_at_end);
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
			afb_jobs_queue(0, 0, wait_child, (void*)(intptr_t)info->si_pid);
		default:
			break;
		}
	}
}

/*
# @@ @
# @p port
# @t token
*/

#define SUBST_CHAR  '@'
#define SUBST_STR   "@"

static char *instanciate_string(char *arg, const char *port, const char *token)
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
		} else if (c == 't' && token) {
			chg++;
			dif += (int)strlen(token) - 2;
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
		else if (c == 't' && token)
			wr = stpcpy(wr, token);
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

static int instanciate_environ(const char *port, const char *token)
{
	extern char **environ;
	char *repl;
	int i;

	/* instantiate the environment */
	for (i = 0 ; environ[i] ; i++) {
		repl = instanciate_string(environ[i], port, token);
		if (!repl)
			return -1;
		environ[i] = repl;
	}
	return 0;
}

static char **instanciate_command_args(struct json_object *exec, const char *port, const char *token)
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
		repl = instanciate_string(item, port, token);
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
	struct json_object *exec, *oport, *otok;
	struct sigaction siga;
	const char *token, *port;
	char **args;

	/* check whether a command is to execute or not */
	if (!json_object_object_get_ex(main_config, "exec", &exec))
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
	if (json_object_object_get_ex(main_config, "port", &oport))
		port = json_object_get_string(oport);
	else
		port = 0;
	/* instantiate arguments and environment */
	if (json_object_object_get_ex(main_config, "token", &otok))
		token = json_object_get_string(otok);
	else
		token = 0;
	args = instanciate_command_args(exec, port, token);
	if (args && instanciate_environ(port, token) >= 0) {
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

static void startup_call_reply(struct afb_req_common *comreq, const struct afb_req_reply *reply)
{
	struct startup_req *sreq = containerof(struct startup_req, comreq, comreq);

	if (!reply->error) {
		NOTICE("startup call %s returned %s (%s)", sreq->callspec, json_object_get_string(reply->object), reply->info?:"");
	} else {
		ERROR("startup call %s ERROR! %s (%s)", sreq->callspec, reply->error, reply->info?:"");
		exit(1);
	}
}

static void startup_call_current(struct startup_req *sreq);

static void startup_call_unref(struct afb_req_common *comreq)
{
	struct startup_req *sreq = containerof(struct startup_req, comreq, comreq);

	free(sreq->api);
	free(sreq->verb);
	json_object_put(sreq->comreq.json);
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
	enum json_tokener_error jerr;

	sreq->callspec = json_object_get_string(json_object_array_get_idx(sreq->calls, sreq->index)),
	api = sreq->callspec;
	verb = strchr(api, '/');
	if (verb) {
		json = strchr(verb, ':');
		if (json) {
			afb_req_common_init(&sreq->comreq, &startup_req_common_itf, NULL, NULL);
			sreq->comreq.validated = 1;
			sreq->api = strndup(api, verb - api);
			sreq->verb = strndup(verb + 1, json - verb - 1);
			sreq->comreq.apiname = sreq->api;
			sreq->comreq.verbname = sreq->verb;
			sreq->comreq.json = json_tokener_parse_verbose(json + 1, &jerr);
			if (sreq->api && sreq->verb && jerr == json_tokener_success) {
				afb_req_common_process(&sreq->comreq, main_apiset);
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

	if (json_object_object_get_ex(main_config, "call", &calls)
	 && json_object_is_type(calls, json_type_array)
	 && (count = (int)json_object_array_length(calls))) {
		sreq = calloc(1, sizeof *sreq);
		sreq->session = afb_session_create(3600);
		sreq->calls = calls;
		sreq->index = 0;
		sreq->count = count;
		startup_call_current(sreq);
	}
}

/*---------------------------------------------------------
 | job for starting the daemon
 +--------------------------------------------------------- */

static void start(int signum, void *arg)
{
#if WITH_AFB_HOOK
	const char *tracereq = NULL, *traceapi = NULL, *traceevt = NULL;
	const char *traceses = NULL, *traceglob = NULL;
#if !defined(REMOVE_LEGACY_TRACE)
	const char *tracesvc = NULL, *traceditf = NULL;
#endif
#endif
	const char *workdir = NULL, *rootdir = NULL, *token = NULL;
	struct json_object *settings = NULL;
	int max_session_count, session_timeout, api_timeout;
	int rc;
#if WITH_LIBMICROHTTPD
	const char *rootapi = NULL;
	int no_httpd = 0, http_port = -1;
	struct afb_hsrv *hsrv;
#endif


#if WITH_AFB_DEBUG
	afb_debug("start-entry");
#endif

	if (signum) {
		ERROR("start aborted: received signal %s", strsignal(signum));
		exit(1);
	}

	rc = wrap_json_unpack(main_config, "{"
			"ss ss s?s"
			"si si si"
			"s?o"
			"}",

			"rootdir", &rootdir,
			"workdir", &workdir,
			"token", &token,

			"apitimeout", &api_timeout,
			"cntxtimeout", &session_timeout,
			"session-max", &max_session_count,

			"set", &settings
			);
	if (rc < 0) {
		ERROR("Unable to get start config");
		exit(1);
	}

#if WITH_LIBMICROHTTPD
	rc = wrap_json_unpack(main_config, "{"
			"s?b s?i s?s"
			"}",

			"no-httpd", &no_httpd,
			"port", &http_port,
			"rootapi", &rootapi
			);
	if (rc < 0) {
		ERROR("Unable to get http config");
		exit(1);
	}
#endif

#if WITH_AFB_HOOK
	rc = wrap_json_unpack(main_config, "{"
#if !defined(REMOVE_LEGACY_TRACE)
			"s?s s?s"
#endif
			"s?s s?s s?s s?s s?s"
			"}",

#if !defined(REMOVE_LEGACY_TRACE)
			"tracesvc", &tracesvc,
			"traceditf", &traceditf,
#endif
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

	/* set the directories */
	mkdir(workdir, S_IRWXU | S_IRGRP | S_IXGRP);
	if (chdir(workdir) < 0) {
		ERROR("Can't enter working dir %s", workdir);
		goto error;
	}
	if (afb_common_rootdir_set(rootdir) < 0) {
		ERROR("failed to set common root directory %s", rootdir);
		goto error;
	}
#if WITH_ENVIRONMENT
	if (!addenv_realpath("AFB_WORKDIR", "."     /* resolved by realpath */)
	 || !addenv_realpath("AFB_ROOTDIR", rootdir /* relative to current directory */)) {
		ERROR("can't set DIR environment");
		goto error;
	}
#endif

#if WITH_LIBMICROHTTPD
	/* setup HTTP */
	if (!no_httpd) {
		if (http_port == 0) {
			ERROR("random port is not implemented");
			goto error;
		}
#if WITH_ENVIRONMENT
		if ((http_port > 0 && !addenv_int("AFB_PORT", http_port))
		 || (token && !addenv("AFB_TOKEN", token))) {
			ERROR("can't set HTTP environment");
			goto error;
		}
#endif
	}
#endif

	/* configure the daemon */
	afb_api_common_set_config(settings);
	main_apiset = afb_apiset_create("main", api_timeout);
	if (!main_apiset) {
		ERROR("can't create main api set");
		goto error;
	}
	if (afb_monitor_init(main_apiset, main_apiset) < 0) {
		ERROR("failed to setup monitor");
		goto error;
	}
#if WITH_SUPERVISION
	if (afb_supervision_init(main_apiset, main_config) < 0) {
		ERROR("failed to setup supervision");
		goto error;
	}
#endif

#if WITH_AFB_HOOK
	/* install hooks */
	if (tracereq)
		afb_hook_create_req(NULL, NULL, NULL, afb_hook_flags_req_from_text(tracereq), NULL, NULL);
#if !defined(REMOVE_LEGACY_TRACE)
	if (traceapi || tracesvc || traceditf)
		afb_hook_create_api(NULL, afb_hook_flags_api_from_text(traceapi)
			| afb_hook_flags_legacy_ditf_from_text(traceditf)
			| afb_hook_flags_legacy_svc_from_text(tracesvc), NULL, NULL);
#else
	if (traceapi)
		afb_hook_create_api(NULL, afb_hook_flags_api_from_text(traceapi), NULL, NULL);
#endif
	if (traceevt)
		afb_hook_create_evt(NULL, afb_hook_flags_evt_from_text(traceevt), NULL, NULL);
	if (traceses)
		afb_hook_create_session(NULL, afb_hook_flags_session_from_text(traceses), NULL, NULL);
	if (traceglob)
		afb_hook_create_global(afb_hook_flags_global_from_text(traceglob), NULL, NULL);
#endif

	/* load bindings and apis */
#if WITH_AFB_DEBUG
	afb_debug("start-load");
#endif
#if WITH_DYNAMIC_BINDING
	apiset_start_list("binding", afb_api_so_add_binding, "the binding");
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

	DEBUG("Init config done");

	/* start the services */
#if WITH_AFB_DEBUG
	afb_debug("start-start");
#endif
#if WITH_CALL_PERSONALITY
	personality((unsigned long)-1L);
#endif
	if (afb_apiset_start_all_services(main_apiset) < 0)
		goto error;

	/* export started apis */
	apiset_start_list("ws-server", afb_api_ws_add_server, "the afb-websocket service");
#if WITH_DBUS_TRANSPARENCY
	apiset_start_list("dbus-server", afb_api_dbus_add_server, "the afb-dbus service");
#endif

	/* start the HTTP server */
#if WITH_LIBMICROHTTPD
#if WITH_AFB_DEBUG
	afb_debug("start-http");
#endif
	if (!no_httpd) {
		if (!afb_hreq_init_cookie(http_port, rootapi, session_timeout)) {
			ERROR("initialisation of HTTP cookies failed");
			goto error;
		}

		hsrv = start_http_server();
		if (hsrv == NULL)
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
	if (watchdog_activate() < 0)
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
#if WITH_AFB_DEBUG
	afb_debug("main-entry");
#endif

	// ------------- Build session handler & init config -------
	main_config = afb_daemon_opts_parse(argc, argv);
	if (afb_sig_monitor_init(
		!json_object_object_get_ex(main_config, "trap-faults", &obj)
			|| json_object_get_boolean(obj)) < 0) {
		ERROR("failed to initialise signal handlers");
		return 1;
	}


	if (json_object_object_get_ex(main_config, "name", &obj)) {
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
	setup_daemon();

#if WITH_AFB_DEBUG
	afb_debug("main-start");
#endif

	/* enter job processing */
	afb_sched_start(3, 0, 2000, start, NULL);
	WARNING("hoops returned from jobs_enter! [report bug]");
	return 1;
}

