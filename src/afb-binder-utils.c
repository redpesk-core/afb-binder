/*
 * Copyright (C) 2015-2025 IoT.bzh Company
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

#include <string.h>
#include "afb-binder-utils.h"

/**
* scan the specification and if it begins with one of the
* predefined private prefix
*/
int scan_export_prefix(const char *spec, export_prefix_type_t *type)
{
	int len = 0;
	export_prefix_type_t kind = Export_Default;
	if (*spec == '-') {
		len = 1;
		kind = Export_Private;
	}
	else if (memcmp(spec, "private:", 8) == 0) {
		len = 8;
		kind = Export_Private;
	}
	else if (memcmp(spec, "public:", 7) == 0) {
		len = 7;
		kind = Export_Public;
	}
	else if (memcmp(spec, "restricted:", 11) == 0) {
		len = 11;
		kind = Export_Restricted;
	}
	if (type != NULL)
		*type = kind;
	return len;
}
