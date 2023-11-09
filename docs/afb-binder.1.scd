afb-binder(1)	"Redpesk OS"

# NAME

afb-binder - Application Framework Binder

# SYNOPSIS

*afb-binder* [<_options_>...]

# DESCRIPTION

The program *afb-binder*, often called the *binder*, has 3 main purposes:

. ensure security context for *API*s provided by *bindings*
. connect *API*s running on different instances
. enable *HTML5* access to connected *API*s

*Bindings* are shared object libraries dynamically loaded by the binder.
These bindings when loaded are declaring *API*(s) that they provide.

API stands fo *A*pplication *P*rogrammer *I*nterface. In the context
of *afb-binder*, it represents a set of procedures linked to a same service.

An *API* groups named entries called *verbs*. A verb is a callable endpoint,
a procedure for short: it receives data, process it, return a result.

The program *afb-binder* ensure the security context for these API and verbs.
It tracks identity of callers and checks there rights and capabilities
against the statically declared policy attached to verbs.

It provides the transport mechanism to connect instances of *afb-binder*,
allowing one instance to call verbs of an other instance, providing an
invisible RPC (Remote Procedure Call) secured framework for free.

*afb-binder* also easy export of callable verbs to HTML5 applications.
But not only because it allows to build rich HTML5 applications by
providing a simple but powerful HTTP server.

To achieve its goals, *afb-binder* must identify clients and assign them
to client sessions indexed using a unique session identifier.


# OPTIONS

## Running

*--background*
	Turn to run in background mode (synonym of *--daemon*)

*--call* _CALLSPEC_
	Call a verb at start. This option can be repeated.
	
	The format of _CALLSPEC_ is *API/VERB:JSONARG*.

	The effect is to cal the *VERB* of the *API* with
	the JSON value *JSONARG* as argument.
	This call is made after initialisation and before
	starting to serve.

	Example: --call 'monitor/set:{"verbosity":{"api":"debug"}}'

*-D, --daemon*
	Turn to run in background mode (synonym of *--background*)

*-e, --exec* _COMMAND AND ARGS..._
	Execute the remaining arguments

	This *MUST* be the last option.
	
	The remaining arguments define a command that *afb-binder*
	will launch and run. *afb-binder* will stop when the launched
	command stops.
	
	The sequences *@p* and *@@* of the arguments are replaced with
	the _port_ (see *--port*) and _@_.

*--foreground*
	Turn to run in forground mode (this is the default mode)

*-j, --jobs-max* _VALUE_
	Maximum count of jobs that can be queued.
	Value must be a positive integer.

*-n, --name* _NAME_
	Set the visible name of the process for command _ps_.
	Also set the environment variable *AFB_NAME*.

*--session-max* _COUNT_
	Max count of simultaneous sessions.
	Value must be a positive integer.

*-o, --output* _FILENAME_
	Redirect stdout and stderr to output file of path _FILENAME_
	(useful when *--daemon*).

*-t, --threads-max* _VALUE_
	Maximum count of parallel threads held on.
	Value must be a positive integer.


## Directories

*--rootdir* _DIRECTORY_
	Root directory of the application.
	If the path is relative, it is given relative to the workdir
	as set by option *--workdir*.
	The default value is to use the workdir (same as *--rootdir=.*)

*--roothttp* _DIRECTORY_
	Root directory for serving files through HTTP.
	If the path is relative, it is given relative to the workdir
	as set by option *--workdir*.
	*If not set*, the default is to not serve files through HTTP
	but to serve API through websocket connections.

*-u, --uploaddir* _DIRECTORY_
	Directory for uploading files through HTTP POST methods.
	If the path is relative, it is given relative to the workdir
	as set by option *--workdir*.
	The default value is to use the workdir (same as *--uploaddir=.*)

*-w, --workdir* _DIRECTORY_
	Set the working directory.
	The working directory is the default directory of the program.
	By default, don't change of directory and use the current one.


## Http service

*--alias* _ALIAS_
	Install an HTTP alias for a specific directory.
	This option can be repeated.

	The syntax is *--alias=/PREF:DIR* where */PREF* is an absolute
	path (starting with a /) and *DIR* is either and absolute path
	or a path relative to the workdir.

	The effect is that HTTP requests to URL of path */PREF/subpath*
	served by files of *subpath* within the directory *DIR*.

	Example: --alias=/icons:/usr/share/icons maps the
	content of /usr/share/icons within the subpath /icons.

*--cache-eol* _TIMEOUT_
	Client cache end of live in seconds
	\[default 100000 (means 27 hours)]

*--cntxtimeout* _TIMEOUT_
	Client Session Context Timeout in seconds
	\[default 32000000 (means 370 days)]

*--https*
	Activates HTTPS

*--https-cert* _FILE_
	Path to the file containing the certificate in PEM
	format to use for serving HTTPS

*--https-key* _FILE_
	Path to the file containing the private key in PEM
	format to use for serving HTTPS

*-i, --interface* _INTERFACE_
	Add HTTP listening interface.
	The format is *SERVICE:HOST:PORT*.
	(ex: tcp:localhost:8080)

*--no-httpd*
	Forbid HTTP service: no file served and no API through
	websockets.

*-p, --port* _PORT_
	HTTP listening TCP port.
	The default value is 1234.
	Setting the option *--port=4545* is equivalent to set
	option *--interface=tcp:0.0.0.0:4545* that listen to
	any interface at port 4545.

*--rootapi* _PATH_
	HTTP Root API URL [default /api]

*--rootbase* _PATH_
	Angular Base Root URL [default /opa].
	This option is intended to serve *O*ne *P*age *A*pplications.

	The syntax is *--rootbase=/PREF* where */PREF* is an absolute
	path (starting with a /).

	The effect is that HTTP requests to URL of path */PREF/subpath*
	are redirected to */PREF/#!subpath* if it can not be satisfied
	normally.


## Bindings and APIs

*--apitimeout* _TIMEOUT_
	Binding API timeout in seconds [default 20]

	Defines how many seconds maximum a verb is allowed to run
	before raising an error. 0 means no limit.

*-A, --auto-api* _DIRECTORY_
	Automatic activation of APIs found in the given _DIRECTORY_ when
	an API api is missing.

	Since afb-binder 5.0.3, _DIRECTORY_ can be prefixed with an
	exportation specification. See discussion on EXPORT.

*-b, --binding* _BINDINGSPEC_
	Load the binding as described by _BINDINGSPEC_.

	_BINDINGSPEC_ can be as simple as a path to the library
	implementing the binding.

	In its full version, _BINDINGSPEC_ is made of the following
	parts:

```
		[EXPORT]PATH[:[UID:]CONFIG]
```

	Where:

	- *PATH* is the path path to the library implementing the binding
	- *EXPORT* (since 5.0.3, optional) tells how to export APIs of the
	  binder, see discussion on EXPORT
	- *CONFIG* (optional) is the path to JSON file holding configuration
	  data for the binding
	- *UID* (optional) if given, its value can be read by the binding for its
	  setting up (example: use one config for N specialization
	  selected by UID value)

*--ldpaths* _PATHSET_
	Load bindings found from paths given in _PATHSET_,
	separated by colons. Path to files must point bindings.
	Path to directories are scanned recursively to discover
	bindings.
	
	The format is *--ldpaths=dir1:dir2:binding1.so:...*.
	You can mix path to directories and to bindings.
	The sub-directories of the given directories are searched
	recursively.

	The bindings are the files terminated by '.so' (the extension
	so denotes shared object) that contain the public entry symbol.

	If any found binding fails to load, *afb-binder* will report
	the error and stop.

	Since afb-binder 5.0.3, _PATHSET_ can be prefixed with an
	exportation specification. See discussion on EXPORT.

*--weak-ldpaths* _PATHSET_
	Same as *--ldpaths* but if a binding fails to load, an error
	is reported but *afb-binder* will continue instead of stopping.

	Since afb-binder 5.0.3, _PATHSET_ can be prefixed with an
	exportation specification. See discussion on EXPORT.

*--ws-client* _SOCKSPEC_
	Bind an external API through WSAPI connection.

	See discussion on WS/RPC-API.

	Since afb-binder 5.0.3, _SOCKSPEC_ can be prefixed with an
	exportation specification. See discussion on EXPORT.

*--ws-server* _SOCKSPEC_
	Provide (Export) an API through WSAPI connection.

	See discussion on WS/RPC-API.

*--rpc-client* _SOCKSPEC_
	Bind an external API through RPCAPI connection.

	See discussion on WS/RPC-API.

	Since afb-binder 5.0.3, _SOCKSPEC_ can be prefixed with an
	exportation specification. See discussion on EXPORT.

*--rpc-server* _SOCKSPEC_
	Provide (Export) an API through RPCAPI connection.

	See discussion on WS/RPC-API.


## Extensions

*-x, --extension* _FILENAME_
	Load the extension of path _FILENAME_.

*-X, --extpaths* _PATHSET_
	Load extensions from dir1:dir2:...

	The format is *--extpaths=dir1:dir2:...* where directories
	are separated by colons.

	If any found extension fails to activate, *afb-binder* will report
	the error and stop.


## Configuration

*-C, --config* _FILENAME_
	Load options from the given config file.

	The loaded options are loaded early in the process of scanning options.
	Then if later options are set, it overrides the options loaded from the
	file.

	This option can be repeated. The differents configuration files loaded
	complete one an other if not overlapping. Otherwise, the effect
	depends on the value of the parameter. Arrays and dictionaries are
	merged but integral values are replaced.

	See discussion below.

*-s, --set* _VALUE_
	Set parameters values for APIs.
	The value can be of the form *[API]/[KEY]:JSON*
	or *{"API":{"KEY":JSON},...}*

	See discussion below.

*-Z, --dump-config*
	Output before expansion a JSON representation of the configuration
	resulting from environment and options.
	Then exits.

*-z, --dump-final-config*
	Output after expansion a JSON representation of the configuration
	resulting from environment and options.
	Then exits.


## Verbosity and log

These options control the logging output of the binder.
By default, the logging level is set as if the option
*--log=error,warning* were entered.

*-c, --color*
	Enforce colorization of the output

*-l, --log* _LOGSPEC_
	Tune log level

	Tune the log level mask. The levels are:

		- error
		- warning
		- notice
		- info
		- debug

	The level can be set using + or -.

[[ *Examples*
:[ *Description*
|  error,warning
:  selects only the levels error and warning
|  +debug
:  adds level debug to the current verbosity
|  -warning
:  remove the level warning from the current verbosity
|  +warning-debug,info
:  Adds error and remove errors and warnings


*-q, --quiet*
	Decreases the verbosity, can be repeated

*-v, --verbose*
	Increases the verbosity, can be repeated


## Debugging

*-M, --monitoring*
	Enable HTTP monitoring at URL /monitoring

*--traceapi* _VALUE_
	Log internal api calls.
	Commonly used values are: *none*, *common*, *api*, *event*, *all*.

*--traceevt* _VALUE_
	Log internal event calls.
	Commonly used values are: *none*, *common*, *extra*, *all*

*--traceglob* _VALUE_
	Log internal global calls.
	Commonly used values are: *none*, *all*

*--tracereq* _VALUE_
	Log internal request calls.
	Commonly used values are: *none*, *common*, *extra*, *all*

*--traceses* _VALUE_
	Log internal session calls.
	Commonly used values are: *none*, *all*

*--trap-faults* _VALUE_
	Indicates that faults are to be trapped or not.
	_VALUE_ can be  *on*, *off*, *yes*, *no*, *true* (default), *false*, *1* or *0*

	The default is to trap faults (SIGSEGV, SIGABRT, SIGFPE) and
	to try to recover.
	But it could be difficult to debug and recover is sometimes hazardous.


## Usage and help

*-?, --help*
	Prints help with available options

*--usage*
	Give a short usage message

*-V, --version*
	Print program version and copyright


# ENVIRONMENT

Environment variables can be used to set some options or behaviours.
Also, the binder defines some variables that can be used by loaded
bindings or chilren programs.

## Variables for setting options

These environment variables are giving a value to their corresponding
option. For some of it, giving the option in the command line
override the value of the environment variable, while for some other,
giving the option on the command line extend the list of values for
the option.

[[ *variable*
:[ *option*
:[ *behaviour*
|  *AFB_TRACEREQ*
:  --tracereq
:  override
|  *AFB_TRACEEVT*
:  --traceevt
:  override
|  *AFB_TRACESES*
:  --traceses
:  override
|  *AFB_TRACEAPI*
:  --traceapi
:  override
|  *AFB_TRACEGLOB*
:  --traceglob
:  override
|  *AFB_TRAP_FAULTS*
:  --trap-faults
:  override
|  *AFB_SET_PORT*
:  --port
:  override
|  *AFB_HTTPS*
:  --https
:  override
|  *AFB_HTTPS_CERT*
:  --https-cert
:  override
|  *AFB_HTTPS_KEY*
:  --https-key
:  override
|  *AFB_LDPATHS*
:  --ldpaths
:  extend
|  *AFB_WEAK_LDPATHS*
:  --weak-ldpaths
:  extend
|  *AFB_EXTPATHS*
:  --extpaths
:  extend
|  *AFB_SET*
:  --set
:  extend
|  *AFB_LOG*
:  --log
:  mixed

## Variables for extra behaviour

These variables are checked by the binder on start.
They have no option counter part.

*AFB_BINDER_WELL_KNOWN_DIR*
	Set an alternate well-known directory. More or less equivalent
	to the option *--alias=/.well-known:${AFB_BINDER_WELL_KNOWN_DIR}*

*AFB_BINDER_DOWNLOAD_DIR*
	Set an alternate download directory. More or less equivalent
	to the option *--alias=/download:${AFB_BINDER_DOWNLOAD_DIR}*

*AFB_HTTPS_PREFIX*
	Directory where are searched the https key and certificate.
	Default is /etc/afb/.https

## Variables defined by the binder

These variables are defined by the binder and can be used by
bindings and children processes.

*AFB_PORT*
	Export the listening HTTP port of the binder.

*AFB_WORKDIR*
	The workdir of the binder.

*AFB_ROOTDIR*
	The root directory where the application is installed.

*AFB_NAME*
	The name of the binder if set using option *--name*.


# DISCUSSIONS

## Configuration

The option *--config* can be used instead of arguments on the command line.

Example:

```
	afb-binder \
		--no-ldpaths \
		--binding /home/15646/bindings/binding45.so \
		--binding /home/15646/bindings/binding3.so \
		--tracereq common \
		--port 5555 \
		--set api45/key:54027a5e3c6cb2ca5ddb97679ce32f185b067b0a557d16a8333758910bc25a72 \
		--exec /home/15646/bin/test654 @p
```

is equivalent to:

```
	afb-binder --config /home/15646/config1
```

when the file **/home/15646/config1** is:

```
	{
	  "no-ldpaths": true,
	  "binding": [
	    "\/home\/15646\/bindings\/binding45.so",
	    "\/home\/15646\/bindings\/binding3.so"
	  ],
	  "tracereq": "common",
	  "port": 5555,
	  "set" : {
	    "api45": {
	      "key": "54027a5e3c6cb2ca5ddb97679ce32f185b067b0a557d16a8333758910bc25a72"
	    }
	  },
	  "exec": [
	    "\/home\/15646\/bin\/test654",
	    "@p"
	  ]
	}
```
The otpion *--set* can be used to set values for specific APIs.
The set value can have different formats.

The most generic format is *{"API1":{"KEY1":VALUE,"KEY2":VALUE2,...},"API2":...}*

This example set 2 keys for the api *chook*:

```
	afb-binder -Z --set '{"chook":{"account":"urn:chook:b2ca5ddb97679","delay":500}}'
	{
	   "set": {
	     "chook": {
	       "account": "urn:chook:b2ca5ddb97679",
	       "delay": 500
	     }
	   }
	 }
```

An other format is: *[API]/[KEY]:VALUE*.
When API is omitted, it take the value "\*".
When KEY is ommitted, it take the value of "\*".

The settings for the *API* \* are globals and apply to all bindings.

The settings for the *KEY* \* are mixing the value for the API.

The following examples are all setting the same values:

```
	afb-binder --set '{"chook":{"account":"urn:chook:b2ca5ddb97679","delay":500}}'
	afb-binder --set 'chook/*:{"account":"urn:chook:b2ca5ddb97679","delay":500}'
	afb-binder --set 'chook/:{"account":"urn:chook:b2ca5ddb97679","delay":500}'
	afb-binder --set 'chook/account:"urn:chook:b2ca5ddb97679"' --set chook/delay:500
```


## WS/RPC-API, Connection of API

API can be exported using option *--ws-server* and imported using option *--ws-client*,
allowing a service running on a binder instance (client) to consume the API provided by
an other binder (server).

Starting with binder version 5.0.0 and above, API can also be exported with option *--ws-server*
and imported using the corresponding option *--rpc-client*. That new option use a protocol
compatible with API v4 format and able to transmit binary data and types of data.

When connecting binders together, the unit of connection is the API.

The format of the specification _SOCKSPEC_ is:

- for TCP: *tcp:host:port/api*
- for unix: *unix:path/api*
- for abstract unix: *unix:@name/api* or *unix:@api*
- for systemd activated: *sd:api*

It is possible, especially for unix connections to set the import API name
using the query like pattern *?as-api=name*, as below:

	*unix:path/come-api-name?as-api=name*

Example of connecting the API *foo* with the name *bar* using abstract
unix socket:

- on server: *--ws-server=unix:@foo*
- on client: *--ws-client=unix:@foo?as-api=bar*

Since afb-binder 5.0.3, _SOCKSPEC_ of client options can be prefixed with an
exportation specification. See discussion on EXPORT.

## EXPORT, Exportation of API

When HTTP serving is enabled, it is possible to tell if the APIs are exported
to HTTP interface or not. This setting is done using one of the prefix below.

[[ *prefix*
:[ *exportation*
|  *-*
:  not exported to HTTP
|  *private:*
:  not exported to HTTP
|  *public:*
:  exported to HTTP

Not setting the exportation set the export to the default value of the option
as given below:

[[ *option*
:[ *default export*
|  *--binding*
:  public
|  *--ldpaths*
:  public
|  *--weak-ldpaths*
:  public
|  *--ws-client*
:  private
|  *--rpc-client*
:  private
|  *--auto-api*
:  private

Note well that before version 5.0.3, the default exportation of all options
was public. The change introduced by the binder 5.0.3 assume to potentially break
existing setup.


# SEE ALSO

*afb-client*(1), *afb-binding*(7)


# AUTHOR

José Bollo <jose.bollo@iot.bzh>

# LINKS

- The source code:        https://github.com/redpesk-core/afb-binder
- Reporting bugs:         https://github.com/redpesk-core/afb-binder/issues
- Redpesk documentation:  https://docs.redpesk.bzh
