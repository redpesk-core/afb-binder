###########################################################################
# Copyright (C) 2015-2021 IoT.bzh Company
#
# Author: José Bollo <jose.bollo@iot.bzh>
#
# $RP_BEGIN_LICENSE$
# Commercial License Usage
#  Licensees holding valid commercial IoT.bzh licenses may use this file in
#  accordance with the commercial license agreement provided with the
#  Software or, alternatively, in accordance with the terms contained in
#  a written agreement between you and The IoT.bzh Company. For licensing terms
#  and conditions see https://www.iot.bzh/terms-conditions. For further
#  information use the contact form at https://www.iot.bzh/contact.
# 
# GNU General Public License Usage
#  Alternatively, this file may be used under the terms of the GNU General
#  Public license version 3. This license is as published by the Free Software
#  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
#  of this file. Please review the following information to ensure the GNU
#  General Public License requirements will be met
#  https://www.gnu.org/licenses/gpl-3.0.html.
# $RP_END_LICENSE$
###########################################################################

CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

project(afb-binder C)

set(PROJECT_PRETTY_NAME "Application Framework Binder")
set(PROJECT_DESCRIPTION "Secured binder of API for clients of the Application framework")
set(PROJECT_VERSION 4.0.0beta6)
set(PROJECT_URL "https://github.com/redpesk-core/afb-binder")

INCLUDE(FindPkgConfig)
INCLUDE(GNUInstallDirs)

###########################################################################
# possible settings

option(WITH_MONITORING            "Activates installation of monitoring"   ON)
option(WITH_EXTENSION             "Activates installation of extensions"   ON)
option(PROVIDE_AFB_DAEMON         "Provide afb-daemon compatibility items" OFF)

set(CONFDIR                ${CMAKE_INSTALL_FULL_SYSCONFDIR}/afb CACHE STRING "Path to system config directory")
set(DATADIR                ${CMAKE_INSTALL_FULL_DATADIR}/afb-binder CACHE STRING "Path to datadir")
set(MONITORING_INSTALL_DIR ${DATADIR}/monitoring CACHE STRING "Path to install monitoring")
set(DEVTOOLS_INSTALL_DIR   ${CMAKE_INSTALL_FULL_DATADIR}/afb-ui-devtools/binder CACHE STRING "Path to devtools")
set(WELL_KNOWN_DIR         well-known CACHE STRING "Path to well known directory")

set(DEFAULT_JOBS_MIN      10 CACHE STRING "Default minimum count of jobs queueable")
set(DEFAULT_JOBS_MAX     200 CACHE STRING "Default maximum count of jobs queueable")

# bench shew that 5 is minimal if synchronous calls are used, otherwise 1 is enought for asynchromous calls
set(DEFAULT_THREADS_MIN    1 CACHE STRING "Default minimum count of managed threads")
set(DEFAULT_THREADS_MAX    5 CACHE STRING "Default maximum count of managed threads")

###########################################################################
string(REGEX MATCH "^/" WELL_KNOWN_DIR_IS_ABSOLUTE ${WELL_KNOWN_DIR})
if(${WELL_KNOWN_DIR_IS_ABSOLUTE})
	set(REAL_WELL_KNOWN_DIR "${WELL_KNOWN_DIR}")
else(${WELL_KNOWN_DIR_IS_ABSOLUTE})
	set(REAL_WELL_KNOWN_DIR "${DATADIR}/${WELL_KNOWN_DIR}")
endif(${WELL_KNOWN_DIR_IS_ABSOLUTE})
###########################################################################

link_libraries(-Wl,--as-needed -Wl,--gc-sections -Wl,--no-undefined)

add_compile_options(-Wall -Wextra -Wconversion)
add_compile_options(-Wno-unused-parameter) # frankly not using a parameter does it care?
add_compile_options(-Wno-sign-compare -Wno-sign-conversion)
add_compile_options(
	$<$<C_COMPILER_ID:GNU>:-Werror=maybe-uninitialized>
	$<$<CXX_COMPILER_ID:GNU>:-Werror=maybe-uninitialized>
	$<$<C_COMPILER_ID:Clang>:-Werror=sometimes-uninitialized>
	$<$<CXX_COMPILER_ID:Clang>:-Werror=sometimes-uninitialized>
	$<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
)
add_compile_options(-ffunction-sections -fdata-sections)
add_compile_options(-fPIC)
add_compile_options(-g -O2)
set (CMAKE_CXX_STANDARD 14)

set(CMAKE_C_FLAGS_PROFILING    "-g -O2 -pg -U_FORTIFY_SOURCE")
set(CMAKE_C_FLAGS_DEBUG        "-g -O2 -ggdb -U_FORTIFY_SOURCE")
set(CMAKE_C_FLAGS_RELEASE      "-g -O3")
set(CMAKE_C_FLAGS_COVERAGE     "-g -O2 --coverage")

###########################################################################

PKG_CHECK_MODULES(json-c REQUIRED json-c)
PKG_CHECK_MODULES(libafb REQUIRED libafb)

ADD_DEFINITIONS(
	-DAFB_VERSION="${PROJECT_VERSION}"
	-DDEFAULT_JOBS_MIN=${DEFAULT_JOBS_MIN}
	-DDEFAULT_JOBS_MAX=${DEFAULT_JOBS_MAX}
	-DDEFAULT_THREADS_MIN=${DEFAULT_THREADS_MIN}
	-DDEFAULT_THREADS_MAX=${DEFAULT_THREADS_MAX}
)

add_subdirectory(src)
add_subdirectory(docs)

if(PROVIDE_AFB_DAEMON)
	add_subdirectory(afb-daemon)
endif()

