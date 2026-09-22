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

#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <json-c/json.h>
#include <rp-utils/rp-yaml-default-expand.h>
#include <libafb/misc/afb-verbose.h>

int expand_config(struct json_object **config, int readrefs)
{
	return rp_yaml_default_expand(config, NULL,
			readrefs ? RP_YAMLEXP_ALL_ALL : RP_YAMLEXP_SCAN_ANY);
}


/* read a config file */
int read_config_file(struct json_object **obj, const char *filename)
{
	int rc;
	*obj = NULL;
	rc = access(filename, R_OK);
	if (rc == 0)
		LIBAFB_ERROR("Can't read file %s: %s", filename, strerror(errno));
	else
		rc = rp_yaml_default_expand(obj, filename, RP_YAMLEXP_ALL_ALL);
	return rc;
}
