# The websocket protocol x-afb-ws-json1

The WebSocket protocol *x-afb-ws-json1* is used to communicate between
an application and a binder. It allows access to all registered apis
of the binder.

This protocol is inspired from the protocol **OCPP - SRPC** as described for
example here:
[OCPP transport specification - SRPC over WebSocket](http://www.gir.fr/ocppjs/ocpp_srpc_spec.shtml).

The registration to the IANA is still to be done, see:
[WebSocket Protocol Registries](https://www.iana.org/assignments/websocket/websocket.xml)

This document gives a short description of the protocol *x-afb-ws-json1*.
A more formal description has to be done.

## Architecture

The protocol is intended to be symmetric. It allows:

- to CALL a remote procedure that returns a result
- to push and receive EVENT

## Messages

Valid messages are made of *text* frames that are all valid JSON.

Valid messages are:

Calls:

```txt
[ 2, ID, PROCN, ARGS ]
[ 2, ID, PROCN, ARGS, TOKEN ]
```

Replies (3: OK, 4: ERROR):

```txt
[ 3, ID, RESP ]
[ 4, ID, RESP ]
```

Events:

```txt
[ 5, EVTN, OBJ ]
```

Where:

| Field | Type   | Description
|-------|--------|------------------
| ID    | string | A string that identifies the call. A reply to that call use the ID of the CALL.
| PROCN | string | The procedure name to call of the form "api/verb"
| ARGS  | any    | Any argument to pass to the call (see afb_req_json that returns it)
| RESP  | any    | The response to the call
| TOKEN | string | The authorisation token
| EVTN  | string | Name of the event in the form "api/event"
| OBJ   | any    | The companion object of the event

Below, an example of exchange:

```txt
C->S:   [2,"156","hello/ping",null]
S->C:   [3,"156",{"response":"Some String","jtype":"afb-reply","request":{"status":"success","info":"Ping Binder Daemon tag=pingSample count=1 query=\"null\""}}]
```

## History

### 14 November 2019

Removal of token returning. The replies

```txt
[ 3, ID, RESP, TOKEN ]
[ 4, ID, RESP, TOKEN ]
```

are removed from the specification.

## Future

Here are the planned extensions:

- add binary messages with cbor data
- add calls with unstructured replies

This could be implemented by extending the current protocol or by
allowing the binder to accept either protocol including the new ones.

## Javascript implementation

The file **AFB.js** is a javascript implementation of the protocol.

It can be downloaded [here](https://raw.githubusercontent.com/redpesk-devtools/afb-monitoring/refs/heads/master/src/AFB.js)

