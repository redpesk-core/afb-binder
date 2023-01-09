# The library libafb-binder

The library *libafb-binder* can be used to create programs that easily
use *libafb* to integrate in AFB framework.

The following libraries are known to integrate *libafb-binder* in order
to offer AFB framework connection to widely used programming languages:

- *libafb-python*
- *libafb-nodejs*
- *libafb-lua*

## Basic usage

The below example shows a basic usage

```C
#include <libafb-binder.h>
/**
* @brief create a binder instance from configuration and run it
* @param config the configuration object
* @return 1 in case of success or 0 in case of error
*/
int run_binder(json_object *config)
{
  AfbBinderHandleT *binder_handle = NULL;
  const char *errormsg = AfbBinderConfig(config, &binder_handle, NULL);
  if (errormsg != NULL)
    fprintf(stderr, "initialisation of binder failed: %s\n", errormsg);
  else
    AfbBinderStart(binder_handle, NULL, NULL, NULL);
  return errormsg == NULL;
}
```

As the main purpose of *libafb-binder* is to wrap complexity of libafb in a higher
abstraction layer, the main settings is done through a structured configuration
object whose internal representation is a *json_object* and whose external
canonical representation can be JSON or YAML.

The documentation below use the JSON representation to explain the expected
structure and content of the *json_object* used internally.

The known libraries integrating *libafb-binder* are using native language
notations for setting their configuration. These representations must
be converted to their equivalent *json_object*.

## Overview of API

The API is accessible through incuding `libafb-binder.h` header file.
That header includes 2 other important headers: json-c and libafb/v4

```C
#include <json-c/json.h>
#include <libafb/afb-v4.h>
```

The library *libafb-binder* is intended to ease the use of *libafb*
but as it use it internally, *libafb*, as imported in the header file
can be used directly where needed.

### Binder instance

The function `AfbBinderConfig` must be called to create a binder instance.
That instance can then be used for loading bindings and adding API, verb
and event handlers (or implementations).

The binder instance is then either started to serve automatically,
implementations living in callbacks, using `AfbBinderStart`, or,
it is started using `AfbBinderEnter`, that start services, and the
scheduler is run by polling using `AfbPollRunJobs`. See [event loop](#event-loop)

- **AfbBinderHandleT**: abstract type for handling binder instance
- **AfbBinderConfig**: create a binder instance for the given config
- **AfbBinderInfo**: get some configuration strings
- **AfbStartupCb**: type of callback functions for `AfbBinderStart` and `AfbBinderEnter`
- **AfbBinderStart**: run the binder loop until `AfbBinderExit` is called
- **AfbBinderExit**: leave the binder loop
- **AfbBinderEnter**: start binder instance services
- **AfbPollRunJobs**: run binder instance scheduler one time

### Setting items of the binder

The binder instance is used to load bindings, handle events and creation of
API and verbs. It provide configuration facilities on top of libafb.

- **AfbBindingLoad**: load binding in binder instance
- **AfbBinderGetApi**: get the global API
- **AfbApiImport**: load external API in binder instance
- **AfbApiCreate**: add an API in binder instance
- **AfbAddOneVerb**: add a verb to an API in binder instance
- **AfbAddVerbs**: add set of verbs to an API in binder instance
- **AfbAddOneEvent**: add an event handler to an API in binder instance
- **AfbAddEvents**: add events handler to an API in binder instance

### About magic

- **AfbMagicTagE**: enumeration type grouping predefined values for identitfying items
- **AfbMagicToString**: String representation of an AfbMagicTagE value

## Main configuration object

The main configuration object is passed to the function `AfbBinderConfig`.

This is a JSON example of main configuration object.

```json
{
    "uid": "my-binder-uid",
    "info": "free text",
    "verbose": 0,
    "timeout": 3600,
    "noconcurrency": false,
    "port": 1234,
    "roothttp": ".",
    "rootapi": "/api",
    "rootdir": ".",
    "https-cert": "/my/ssl/path/httpd.cert",
    "https-key": "/my/ssl/path/httpd.key",
    "alias": [
        "my-alias:/My-alias/path"
    ],
    "intf": "tcp:*:1234",
    "extentions": {},
    "ldpath": [
        "/opt/helloworld-binding/lib",
        "/usr/local/helloworld-binding/lib"
    ],
    "acls": {
        "rw": "urn:redpesk:permission::read-write",
        "ro": { "or": [ "#rw", "urn:redpesk:permission::read" ] }
    },
    "thread-pool": 1,
    "thread-max": 1,
    "trapfaults": true
}
```

All the fields are optionals except *uid*.

Here is the description of the configuration fields:

- **uid**:           a short identifier
- **info**:          a free text describing the configuration
- **verbose**:       verbosity level (integer from 0 to 9, default is 0)
- **timeout**:       global http timeout in seconds ( default is 32000000)
- **noconcurrency**: prevent API concurency if true (boolean, default is false)
- **port**:          http port number when 0 (default) no http service starts
- **roothttp**:      served directory for http (string, default is ".")
- **rootapi**:       HTTP prefix for accessing APIs (string, default is "/api")
- **rootdir**:       running directory (string, default is ".")
- **https-cert**:    path to TLS's X509 certificate (string or null, default is null for no TLS)
- **https-key**:     path to TLS's X509 private key (string or null, default is null for no TLS)
- **alias**:         list of HTTP prefix for paths (string or array of string of structure "prefix:path")
- **intf**:          listening HTTP interface (string or array of strings)
- **extentions**,    configuration of extentions (object)
- **ldpath**:        global list of directory for searching bindings (string or array of strings)
- **acls**:          dictionary of access control (object, see [access control](#acls))
- **thread-pool**:   initial thread pool size (integer, default is 0). Note than standard operations: verb,event,timer,...
                     do not extend thread pool. When needed they are pushed on waiting queue.
- **thread-max**:    autoclean thread pool when bigger than max (may temporaly get bigger), (integer, default is 1)
- **trapfaults**:    prevent handling faults when debugging (boolean, default is false)

## Binding configuration object

The binding configuration object is passed to the function `AfbBindingLoad`
that is used to load existing bindings.

This is a JSON example of binding configuration object.

```json
{
   "uid"    : "helloworld",
   "export" : "protected",
   "uri"    : "unix:@helloworld?as-api=helloworld",
   "path"   : "afb-helloworld-skeleton.so",
   "ldpath" : ["/opt/helloworld-binding/lib", "/usr/local/helloworld-binding/lib"],
   "alias"  : ["/hello:'/opt/helloworld-binding/htdocs","/devtools:/usr/share/afb-ui-devtools/binder"],
}
```

All the fields are optionals except *uid* and *path*.

Here is the description of the configuration fields:

- **uid**: binding uid use debug purpose only
- **path**: binding relative or full path
- **uri**: direct exportation path of the API of the binding (string, see [uri import/export](#uri))
- **export**: private, restricted public (string, see [exportation](#exportation))
- **ldpath**: local binding search path searched before default binder search path
- **alias**: alias list added to global binder existing list

## API configuration object

The API configuration object is passed to functions `AfbApiImport` or `AfbApiCreate`.

AFB microservice architecture allows to import external APIs in the context
of the binder instance. This mechanism make an external API(s) visible from
inside a binder as if it was existing locally. This is achieved by
function `AfbApiImport`.

Here is a typical JSON configuration when importing a foreign API.

```json
{
    "uid"    : "my-api-uid",
    "api"    : "my-api-name",
    "info"   : "free text",
    "verbose": 0,
    "export" : "public",
    "uri"    : "unix:@my-api",
    "lazy"   : true
}
```

API can also be created in the binder instance to implement a
local API in the way decided by the integrator. In that case,
the function `AfbApiCreate` must be used.

Both functions are taking an API configuration object but
some of the fields are specific to one functions, not both.

All the fields are optionals except *uid*.

Here is the description of the common configuration fields:

- **uid**: identifier (string)
- **api**: API name (string, default is the value of *uid*)
- **info**: a free text describing the API (string)
- **verbose**: verbosity level (integer from 0 to 9, default is 0)
- **export**: private, restricted public (string, see [exportation](#exportation))
- **uri**: import or export specification of the API (string, see [uri export](#uri))

Fields for `AfbApiImport` only are:

- **lazy**: prevent connection at start (boolean, default is false)

Fields for `AfbApiCreate` only are:

- **alias**: list of HTTP prefix for paths (string or array of string of structure "prefix:path")
- **require**: comma separated list of required classes (string)
- **provide**: comma separated list of provided classes (string)
- **noconcurrency**: prevent API concurrency if true (boolean, default is set globally)
- **verbs**: verb or list of verbs to add to the API (object or array of objects, see [verb config](#verb)).
- **events**: event or list of events to handle at the API (object or array of objects, see [event config](#event)).

*NOTA BENE*:

- if **export** is *restricted** and no **uri** is defined, then
a default **uri** value is automatically created as follow: "unix:@UID"
where UID is the uid of the API  (details of [uri import/export](#uri)).

- When creating an API, using `AfbApiCreate`, if no verb `info` is defined,
then a default *do nothing* `info` verb is automatically added.

<div id="verb"></div>

## Verb configuration object

Verbs are created by the functions `AfbApiCreate`, `AfbAddOneVerb` and `AfbAddVerbs`
that are all wrapping the function `afb_api_v4_add_verb_hookable`.

All the fields are optionals but either *uid* or *verb* must have a value.

Here is the description of the configuration fields:

- **uid**: identifier (string, default to value of *verb*)
- **verb**: name of the verb (string, default to value of *uid*)
- **info**: a free text describing the verb (string)
- **auth**: id of the permission required (string, default is *no permission required*)
- **session**: flag for handling session (integer, default is zero, see [session](#session))
- **regex**: tell if the name is a global pattern (boolean, default is false)


<div id="event"></div>

## Event configuration object

The 2 functions `AfbAddOneEvent` and `AfbAddEvents` are wrappers around
the function `afb_api_v4_event_handler_add_hookable`.

```json
{
  "uid": "id0",
  "pattern": "monitor/*"
}
```

All the fields are optionals except *uid*.

Here is the description of the configuration fields:

- **uid**: identifier (string)
- **pattern**: filtering [pattern](#pattern) of the event (string, default is "*")

<div id="exportation"></div>

## Exportation

All local API have access to all imported or created API.
But APIs are exported to the external world according to
*export* mode as set in the configuration.

The values possible for *export* are:

- **public**: binding api(s) is visible from HTTP and exportable as unix domain socket
- **restricted**: binding api(s) is exportable only as a unix domain socket
- **private**: all binding api(s) are visible for internal subcall only

<div id="pattern"></div>

## Patterns

Patterns are global expressions, it treats the character `*` as the replacement
of any sequence (even empty) of any characters. Examples:

- the pattern `super` only matches the string `super`
- the pattern `a*` matches `azerty` and `aligato` but not `zap`
- the pattern `a*b*c` matches `abac` and `aribetoc` but not `azerty`

<div id="acls"></div>

## Access control

The definition of access control is made globally.
Each definition is associated to a key.
Then definitions of verbs can point the permissions
it requires using the key.

Here is an example defining 3 accesses keys: *rw*, *ro* and *zip*:
```json
{
        "rw": "urn:redpesk:permission::read-write",
        "ro": { "or": [ "#rw", "urn:redpesk:permission::read" ] },
        "zip": {
                "LOA": 2,
                "token": true,
                "AnyOf": "urn:redpesk:permission::zip",
                "Unless": "#ro"
        }
}
```

Definition is made of a dictionary identifying the permissions.
The definition of a permission is either: a string, an array
or an object.

When the specification is a string, its meaning depends of
of its first character.

When the first character is not the sharp symbol,
the string is the required permission.

When the first character is the sharp symbol `#`, it is
an internal reference to an access control definition (on the
example above, `#rw` is the reference to the access requiring
the permission `urn:redpesk:permission::read-write`).

When the specification is an array, like in `[ ... ]`,
it is a shorthand for writing the object `{ "AllOf": [ ... ] }`,
meaning that all access control of the list must be satisfied.

When the specification is an object, it specifies one of the
following requirement:

- requires that an access token
- requires that a level of assurancy (LOA)
- requires that an access is not granted
- requires that at least one access of a list is granted
- requires that all accesses of a list are granted

Valid objects can contain multiple entries. In that case all the
entries are implicitely anded, meaning, all must be granted.

The valid keys are:

- **token**:
     Requires to have an access token.
     The value must be the boolean **true**.

- **LOA**:
     Requires an LOA of the given value.
     The value must be an integer from 0 to 3.

- **AllOf** (or **and**):
     Requires all the accesses listed to be granted.
     The value must be an array of access grant specifications.
     Single specification is also accepted as an array of one element.

- **AnyOf** (or **or**):
     Requires at least one of the accesses listed to be granted.
     The value must be an array of access grant specifications.
     Single specification is also accepted as an array of one element.

- **Unless** (or **not**):
     Requires the given access is not granted.
     The value must be an access grant specification.

Any other key is raising an error.


<div id="session"></div>

## Defining sessions

When defining verbs, it is possible to attach to each verb
a session requirement value.


That value is a mask of bits defined as below:

```

                          4      3       2      1      0
  +---  -  -  -  -----+-------+------+-------+------+------+
  |                   | CLOSE |      | CHECK |   L  O  A   |
  +---  -  -  -  -----+-------+------+-------+------+------+

```

Where fields are:

- **CLOSE**: (value = 16) single bit telling if session has to be closed when the verb returns.
- **CHECK**: (value = 4) single bit telling that the session must be identified with an access token.
- **L O A**: (value = 0, 1, 2 or 3) pair of bits telling the minimal required LOA.

Example: A value of 5 for the session tells that the client of the verb
must be identified with an access token and that the session must have
a LOA of 1 at least.


<div id="uri"></div>

## URI for import and export

When importing or exporting API using sockets, the *uri*
describes what kind of socket, how and where to connect.

The *uri* follows the grammar below:

```
 uri       = (tcp-uri | sysd-uri | unix-uri | char-uri) renaming?
 tcp-uri   = "tcp:" tcp-host ":" tcp-port tcp-api
 sysd-uri  = "sd:" any-name
 unix-uri  = "unix:" unix-spec
 char-uri  = "char:" path
 tcp-host  = ANY SEQUENCE OF CHARACTER WITHOUT ":"
 tcp-port  = ANY SEQUENCE OF CHARACTER WITHOUT "/"
 tcp-api   = ("/" any-name)? "/" api-name
 renaming  = "?as-api=" any-name
 unix-spec = unix-abs | path
 unix-abs  = "@" ANY SEQUENCE OF CHARACTER
 path      = A PATH ABSOLUTE OR RELATIVE
 api-name  = ANY SEQUENCE OF CHARACTER WITHOUT "/"
 any-name  = ANY SEQUENCE OF CHARACTER
```

The renaming is an easy way to change the name of API
across import/export.

## URI of type "tcp:"

Uris of type "tcp:" are connecting using TCP sockets
to or from a host and port.

The host can be specified by its name or its IP address or for
exporting server socket with the star `*` meaning any address.

The port can be specified using a number or the name of a service.

Without renaming, the API is named after the last slash folowing the port.
It is either the name of the exported API or the imported name.

## URI of type "sd:"

Uris of type "sd:" are connecting using socket opened using systemd
socket activation method. It is only suitable for server sockets.

Without renaming, the name of the API is deduced from the name using its last part.

## URI of type "unix:"

Uris of type "unix:" are connecting using unix domain sockets.

If the unix spec starts witn an arobase `@`, the socket is abstract
and doesn't have a path in the filesystem. Otherwise, the socket
is present in the file system and the spec is the path of that socket.

Without renaming, the name of the API is deduced from the name using its last part,
removing the starting @ if necessary.

## URI of type "char:"

Uris of type "char:" are connecting using a pipe or a character device.
This is only possible for client API (import).

Without renaming, the name of the API is deduced from the path using its last part.


<div id="event-loop"></div>

## Integration with an event loop

When not using AFB mainloop, `AfbBinderEnter` replaces `AfbBinderStart`.
Fonctionnaly both functions do the same thing, except that `AfbBinderEnter`
does not enter AFB mainloop, but expect the user to process AFB events/jobs
from its own mainloop.

Callback `AfbPollRunJobs` should be called each time AFB has waiting jobs
in its pool. A typical example is the interface with *NodeJS* that uses
*libuv* mainloop as in following code snipet.

```C
// Map LibUV mainloop event callback onto LibAfb signature
static void GlueOnPoolUvCb(uv_poll_t* handle, int status, int events) {
    AfbPollRunJobs();
}

int MyInitFunction() {
  int afbfd, statusN, statusU;

  // retreive libafb jobs epool file handle
  afbfd = afb_ev_mgr_get_fd();
  if (afbfd < 0) goto OnErrorExit;

  // retreive nodejs libuv jobs file loop handle
  statusN = napi_get_uv_event_loop(env, &loopU);
  if (statusN != napi_ok) goto OnErrorExit;

  // add libafb epool fd into libuv pool
  statusU = uv_poll_init(loopU, &poolU, afbfd);
  if (statusU < 0) goto OnErrorExit;

  // register callback handle for libafb jobs
  statusU = uv_poll_start(&poolU, UV_READABLE, GlueOnPoolUvCb);
  if (statusU < 0) goto OnErrorExit;

  // enter libuv mainloop
  statusU= uv_run(loopU, UV_RUN_DEFAULT);
  if (statusU < 0) goto OnErrorExit;

  ...

  return ok;

OnErrorExit:
  return error;
}
```

