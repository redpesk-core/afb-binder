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

#include <stdlib.h>
#include <stdio.h>
#include <argp.h>
#include <json-c/json.h>
#include <libafb/sys/verbose.h>

#include <libafb/extend/afb-extension.h>
#include <libafb/http/afb-hsrv.h>
#include <libafb/http/afb-hreq.h>

AFB_EXTENSION(test-logo)

const struct argp_option AfbExtensionOptionsV1[] = {
	{ .name="logo",     .key='L',   .arg=0, .doc="requires a logo" },
	{ .name="icon",     .key='I',   .arg="FILE", .doc="Defines the image file of the logo" },
	{ .name=0, .key=0, .doc=0 }
};

int AfbExtensionConfigV1(void **data, struct json_object *config)
{
	*data = &AfbExtensionManifest;
	NOTICE("Extension %s got config %s", AfbExtensionManifest.name, json_object_get_string(config));
	return 0;
}

int AfbExtensionDeclareV1(void *data, struct afb_apiset *declare_set, struct afb_apiset *call_set)
{
	NOTICE("Extension %s got to declare %s", AfbExtensionManifest.name, data == &AfbExtensionManifest ? "ok" : "error");
	return 0;
}

/**
 * Tiny example to show a page
 */
static int reply_hello_logo(struct afb_hreq *hreq, void *data)
{
	static const char coucou[] = "<html><title>Hello from LOGO</title><body><h1>Hello from LOGO";
	afb_hreq_reply_static(hreq, 200, (sizeof coucou) - 1, coucou, NULL);
	return 1;
}

int AfbExtensionHTTPV1(void *data, struct afb_hsrv *hsrv)
{
	NOTICE("Extension %s got HTTP %s", AfbExtensionManifest.name, data == &AfbExtensionManifest ? "ok" : "error");
	/* add a handler for queries of path /logo/... */
	afb_hsrv_add_handler(hsrv,"/logo",reply_hello_logo,0,30);
	return 0;
}

int AfbExtensionServeV1(void *data, struct afb_apiset *call_set)
{
	NOTICE("Extension %s got to serve %s", AfbExtensionManifest.name, data == &AfbExtensionManifest ? "ok" : "error");
	return 0;
}

int AfbExtensionExitV1(void *data, struct afb_apiset *declare_set)
{
	NOTICE("Extension %s got to exit %s", AfbExtensionManifest.name, data == &AfbExtensionManifest ? "ok" : "error");
	return 0;
}
