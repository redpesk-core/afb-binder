###########################################################################
# Copyright (C) 2015-2025 IoT.bzh Company
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

# configure

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/binder-settings.h.in
	${CMAKE_CURRENT_BINARY_DIR}/binder-settings.h
	@ONLY)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/libafb-binder.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/libafb-binder.pc
	@ONLY)

# build

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${json-c_INCLUDE_DIRS}
	${libafb_INCLUDE_DIRS}
	${librp-utils_INCLUDE_DIRS}
)

add_library(libafb-binder SHARED
	libafb-binder.c
)

set_target_properties(libafb-binder PROPERTIES
	OUTPUT_NAME afb-binder
	VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        SOVERSION "${PROJECT_VERSION_MAJOR}"
)

target_link_libraries(libafb-binder
	${json-c_LDFLAGS}
	${libafb_LDFLAGS}
	${librp-utils_LDFLAGS}
)

add_executable(afb-binder
	main-afb-binder.c
	afb-binder-opts.c
	afb-binder-config.c
	afb-binder-utils.c
)

target_link_libraries(afb-binder
	${json-c_LDFLAGS}
	${libafb_LDFLAGS}
	${librp-utils_LDFLAGS}
)

find_library (fts fts)
if(fts)
	target_link_libraries(afb-binder ${fts})
	target_link_libraries(libafb-binder ${fts})
endif()
find_library (argp argp)
if(argp)
	target_link_libraries(afb-binder ${argp})
	target_link_libraries(libafb-binder ${argp})
endif()

# install

install(TARGETS afb-binder
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS libafb-binder
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES libafb-binder.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

INSTALL(FILES
	${CMAKE_CURRENT_BINARY_DIR}/libafb-binder.pc
    DESTINATION
	${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(WITH_MONITORING)
	file(GLOB monit_files ${CMAKE_CURRENT_SOURCE_DIR}/monitoring/*)
	install(FILES ${monit_files}
		DESTINATION ${MONITORING_INSTALL_DIR})
endif()
