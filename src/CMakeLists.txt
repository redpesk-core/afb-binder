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

# configure

configure_file(afb-binder.h.in afb-binder.h @ONLY)

# build

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${json-c_INCLUDE_DIRS}
	${libafb_INCLUDE_DIRS}
)

add_executable(afb-binder afb-binder.c afb-binder-opts.c)

target_link_libraries(afb-binder
	${json-c_LDFLAGS}
	${libafb_LDFLAGS}
)

# install

install(TARGETS afb-binder
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WITH_MONITORING)
	file(GLOB monit_files ${CMAKE_CURRENT_SOURCE_DIR}/monitoring/*)
	install(FILES ${monit_files}
		DESTINATION ${MONITORING_INSTALL_DIR})
endif()
