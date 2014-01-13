# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2011-2014, RedJack, LLC.
# All rights reserved.
#
# Please see the COPYING file in this distribution for license details.
# ----------------------------------------------------------------------

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

#-----------------------------------------------------------------------
# Build the library

set(LIBFLEET_SRC
    libfleet/fleet.c
    libfleet/single-threaded.c
)

# Update the VERSION and SOVERSION properties below according to the following
# rules (taken from [1]):
#
# VERSION = current.revision.age
#
#   1. Start with a VERSION of `0.0.0` for each shared library.
#   2. Update VERSION only immediately before a public release of your software.
#      More frequent updates are unnecessary, and only guarantee that the
#      current interface number gets larger faster.
#   3. If the library source code has changed at all since the last update, then
#      increment `revision` (`c.r.a` becomes `c.r+1.a`).
#   4. If any interfaces have been added, removed, or changed since the last
#      update, increment `current`, and set `revision` to 0.
#   5. If any interfaces have been added since the last public release, then
#      increment `age`.
#   6. If any interfaces have been removed or changed since the last public
#      release, then set `age` to 0.
#
# SOVERSION should always equal `current`.
#
# Note that changing `current` means that you are releasing a new
# backwards-incompatible version of the library.  This has implications on
# packaging, so once an API has stabilized, these should be a rare occurrence.
#
# [1] http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info

add_library(libfleet SHARED ${LIBFLEET_SRC})
set_target_properties(libfleet PROPERTIES
    OUTPUT_NAME fleet
    VERSION 0.0.0
    SOVERSION 0)
target_link_libraries(libfleet
)

install(TARGETS libfleet DESTINATION ${CMAKE_INSTALL_LIBDIR})

#-----------------------------------------------------------------------
# Generate the pkg-config file

set(prefix ${CMAKE_INSTALL_PREFIX})
configure_file(fleet.pc.in ${CMAKE_CURRENT_BINARY_DIR}/fleet.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fleet.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
