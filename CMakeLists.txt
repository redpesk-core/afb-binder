###########################################################################
# Copyright (C) 2015-2020 IoT.bzh Company
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

set(PROJECT_NAME afb-binder)
set(PROJECT_PRETTY_NAME "Application Framework Binder")
set(PROJECT_DESCRIPTION "Secured binder of API for clients of the Application framework")
set(PROJECT_VERSION 1.2)
set(PROJECT_URL "https://github.com/redpesk/afb-libafb")
set(PROJECT_LANGUAGE C)


INCLUDE(FindPkgConfig)
INCLUDE(GNUInstallDirs)

###########################################################################
# possible settings

option(WITH_MONITORING            "Activates installation of monitoring"   ON)
option(PROVIDE_AFB_DAEMON         "Provide afb-daemon compatibility items" OFF)

set(AFS_SUPERVISION_SOCKET "@urn:AGL:afs:supervision:socket" CACHE STRING "Internal socket for supervision")
set(AFS_SUPERVISOR_PORT    1619 CACHE STRING "Port of service for the supervisor")
set(AFS_SUPERVISOR_TOKEN   HELLO CACHE STRING "Secret token for the supervisor")
set(DATADIR                ${CMAKE_INSTALL_FULL_DATADIR}/afb-binder CACHE STRING "Path to datadir")
set(MONITORING_INSTALL_DIR ${DATADIR}/monitoring CACHE STRING "Path to install monitoring")

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
set(CMAKE_C_FLAGS_CCOV         "-g -O2 --coverage")

###########################################################################

PKG_CHECK_MODULES(json-c REQUIRED json-c)
PKG_CHECK_MODULES(libafb REQUIRED libafb)

ADD_DEFINITIONS(-DAFB_VERSION="${PROJECT_VERSION}")

add_subdirectory(src)

if(PROVIDE_AFB_DAEMON)
	add_subdirectory(afb-daemon)
endif()
