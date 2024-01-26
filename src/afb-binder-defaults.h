/*
 * Copyright (C) 2015-2024 IoT.bzh Company
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

#pragma once

/**
 * The default timeout of sessions in seconds
 */
#if !defined(DEFAULT_SESSION_TIMEOUT)
# define DEFAULT_SESSION_TIMEOUT		32000000
#endif

/**
 * The default maximum count of sessions
 */
#if !defined(DEFAULT_MAX_SESSION_COUNT)
# define DEFAULT_MAX_SESSION_COUNT       200
#endif

/**
 * The default timeout of api calls in seconds
 */
#if !defined(DEFAULT_API_TIMEOUT)
# define DEFAULT_API_TIMEOUT		20
#endif

/**
 * default settings for jobs
 */
#if !defined(DEFAULT_JOBS_MIN)
# define DEFAULT_JOBS_MIN		10
#endif
#if !defined(DEFAULT_JOBS_MAX)
# define DEFAULT_JOBS_MAX		200
#endif

/**
 * default settings for threads
 */
#if !defined(DEFAULT_THREADS_POOL)
# define DEFAULT_THREADS_POOL		2
#endif
#if !defined(DEFAULT_THREADS_MIN)
# define DEFAULT_THREADS_MIN		1
#endif
#if !defined(DEFAULT_THREADS_MAX)
# define DEFAULT_THREADS_MAX		5
#endif

/***************************************************/
#if WITH_LIBMICROHTTPD
/**
 * The default timeout of cache in seconds
 */
#if !defined(DEFAULT_CACHE_TIMEOUT)
# define DEFAULT_CACHE_TIMEOUT		100000
#endif

/**
 * The default HTTP port to serve
 */
#if !defined(DEFAULT_HTTP_PORT)
# define DEFAULT_HTTP_PORT		1234
#endif

/**
 * The default interface to serve
 */
#if !defined(DEFAULT_BINDER_INTERFACE)
# define DEFAULT_BINDER_INTERFACE	"*"
#endif
#endif

