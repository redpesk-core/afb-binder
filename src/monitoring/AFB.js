/*
 * Copyright (C) 2015-2025 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * SPDX-License-Identifier: MIT
 */
"use strict";

var AFB = function (base, initialtoken) {

	if (typeof base != "object")
		base = { base: base, token: initialtoken };

	var initial = {
		base: base.base || "api",
		token: initialtoken || base.token
			|| URLSearchParams(window.location.search).get('access_token')
			|| URLSearchParams(window.location.search).get('token')
			|| "HELLO",
		host: base.host || window.location.host,
		scheme: base.scheme || (window.location.protocol == "https:" ? "wss:" : "ws:"),
		url: base.url || undefined
	};

	var urlws = initial.url = initial.url || initial.scheme+"//"+initial.host+"/"+initial.base;

	var setURL = function (location, port) {
		let u = (window.location.protocol == "https:" ? "wss://" : "ws://") + location;
		if (port) {
			u += ':' + port;
		}
		urlws = u + '/' + initial.base;
	}

	/*********************************************/
	/****                                     ****/
	/****             AFB_context             ****/
	/****                                     ****/
	/*********************************************/
	var AFB_context;
	{
		var UUID = undefined;
		var TOKEN = initial.token;

		var context = function (token, uuid) {
			this.token = token;
			this.uuid = uuid;
		}

		context.prototype = {
			get token() { return TOKEN; },
			set token(tok) { if (tok) TOKEN = tok; },
			get uuid() { return UUID; },
			set uuid(id) { if (id) UUID = id; }
		};

		AFB_context = new context();
	}
	/*********************************************/
	/****                                     ****/
	/****             AFB_websocket           ****/
	/****                                     ****/
	/*********************************************/
	var AFB_websocket;
	{
		var CALL = 2;
		var RETOK = 3;
		var RETERR = 4;
		var EVENT = 5;

		var PROTO1 = "x-afb-ws-json1";

		AFB_websocket = function (on_open, on_abort) {
			var u = urlws, p = '?';
			if (AFB_context.token) {
				u = u + '?x-afb-token=' + AFB_context.token;
				p = '&';
			}
			if (AFB_context.uuid)
				u = u + p + 'x-afb-uuid=' + AFB_context.uuid;
			this.ws = new WebSocket(u, [PROTO1]);
			this.url = u;
			this.pendings = {};
			this.awaitens = {};
			this.counter = 0;
			this.ws.onopen = onopen.bind(this);
			this.ws.onerror = onerror.bind(this);
			this.ws.onclose = onclose.bind(this);
			this.ws.onmessage = onmessage.bind(this);
			this.onopen = on_open;
			this.onabort = on_abort;
		}

		function onerror(event) {
			var f = this.onabort;
			if (f) {
				delete this.onopen;
				delete this.onabort;
				f(this);
			}
			this.onerror && this.onerror(this);
		}

		function onopen(event) {
			var f = this.onopen;
			delete this.onopen;
			delete this.onabort;
			f && f(this);
		}

		function onclose(event) {
			var err = {
				jtype: 'afb-reply',
				request: {
					status: 'disconnected',
					info: 'server hung up'
				}
			};
			for (var id in this.pendings) {
				try { this.pendings[id][1](err); } catch (x) {/*NOTHING*/}
			}
			this.pendings = {};
			this.onclose && this.onclose();
		}

		function fire(awaitens, name, data) {
			var a = awaitens[name];
			if (a)
				a.forEach(function(handler){ handler(data, name); });
			var i = name.indexOf("/");
			if (i >= 0) {
				a = awaitens[name.substring(0, i)];
				if (a)
					a.forEach(function(handler){ handler(data, name); });
			}
			a = awaitens["*"];
			if (a)
				a.forEach(function(handler){ handler(data, name); });
		}

		function reply(pendings, id, ans, offset) {
			if (id in pendings) {
				var p = pendings[id];
				delete pendings[id];
				try { p[offset](ans); } catch (x) {/*TODO?*/}
			}
		}

		function onmessage(event) {
			var obj = JSON.parse(event.data);
			var code = obj[0];
			var id = obj[1];
			var ans = obj[2];
			AFB_context.token = obj[3];
			switch (code) {
			case RETOK:
				reply(this.pendings, id, ans, 0);
				break;
			case RETERR:
				reply(this.pendings, id, ans, 1);
				break;
			case EVENT:
			default:
				fire(this.awaitens, id, ans);
				break;
			}
		}

		function close() {
			this.ws.close();
			this.ws.onopen =
			this.ws.onerror =
			this.ws.onclose =
			this.ws.onmessage =
			this.onopen =
			this.onabort = function(){};
		}

		function call(method, request, callid) {
			return new Promise((function (resolve, reject) {
				var id, arr;
				if (callid) {
					id = String(callid);
					if (id in this.pendings)
						throw new Error("pending callid(" + id + ") exists");
				} else {
					do {
						id = String(this.counter = 4095 & (this.counter + 1));
					} while (id in this.pendings);
				}
				this.pendings[id] = [resolve, reject];
				arr = [CALL, id, method, request];
				if (AFB_context.token) arr.push(AFB_context.token);
				this.ws.send(JSON.stringify(arr));
			}).bind(this));
		}

		function onevent(name, handler) {
			var id = name;
			var list = this.awaitens[id] || (this.awaitens[id] = []);
			list.push(handler);
		}

		AFB_websocket.prototype = {
			close: close,
			call: call,
			onevent: onevent
		};
	}
	/*********************************************/
	/****                                     ****/
	/****                                     ****/
	/****                                     ****/
	/*********************************************/
	return {
		context: AFB_context,
		ws: AFB_websocket,
		setURL: setURL
	};
};

exports.AFB = AFB;
