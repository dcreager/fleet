# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2011-2014, RedJack, LLC.
# All rights reserved.
#
# Please see the COPYING file in this distribution for license details.
# ----------------------------------------------------------------------

set(LIBCORK_DIR ${CMAKE_SOURCE_DIR}/lib/libcork)
include_directories(${LIBCORK_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

#-----------------------------------------------------------------------
# Build the library

# Keep this list up-to-date with lib/libcork/src/CMakeLists.txt, and uncomment
# lines only as needed to satisfy linker errors.  That way we only build the
# parts of libcork that we actually use.
set(LIBCORK_SRC
    #${LIBCORK_DIR}/src/libcork/cli/commands.c
    ${LIBCORK_DIR}/src/libcork/core/allocator.c
    ${LIBCORK_DIR}/src/libcork/core/error.c
    #${LIBCORK_DIR}/src/libcork/core/gc.c
    #${LIBCORK_DIR}/src/libcork/core/hash.c
    #${LIBCORK_DIR}/src/libcork/core/ip-address.c
    #${LIBCORK_DIR}/src/libcork/core/mempool.c
    #${LIBCORK_DIR}/src/libcork/core/timestamp.c
    #${LIBCORK_DIR}/src/libcork/core/u128.c
    #${LIBCORK_DIR}/src/libcork/ds/array.c
    #${LIBCORK_DIR}/src/libcork/ds/bitset.c
    ${LIBCORK_DIR}/src/libcork/ds/buffer.c
    ${LIBCORK_DIR}/src/libcork/ds/dllist.c
    #${LIBCORK_DIR}/src/libcork/ds/file-stream.c
    #${LIBCORK_DIR}/src/libcork/ds/hash-table.c
    ${LIBCORK_DIR}/src/libcork/ds/managed-buffer.c
    #${LIBCORK_DIR}/src/libcork/ds/ring-buffer.c
    ${LIBCORK_DIR}/src/libcork/ds/slice.c
    #${LIBCORK_DIR}/src/libcork/posix/directory-walker.c
    #${LIBCORK_DIR}/src/libcork/posix/env.c
    #${LIBCORK_DIR}/src/libcork/posix/exec.c
    #${LIBCORK_DIR}/src/libcork/posix/files.c
    ${LIBCORK_DIR}/src/libcork/posix/process.c
    #${LIBCORK_DIR}/src/libcork/posix/subprocess.c
    ${LIBCORK_DIR}/src/libcork/pthreads/thread.c
)

set(LIBFLEET_SRC
    libfleet/fleet.c
    libfleet/local.c
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

add_library(libfleet SHARED ${LIBCORK_SRC} ${LIBFLEET_SRC})
set_target_properties(libfleet PROPERTIES
    OUTPUT_NAME fleet
    COMPILE_DEFINITIONS CORK_API=CORK_LOCAL
    VERSION 0.0.0
    SOVERSION 0)
target_link_libraries(libfleet
    ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS libfleet DESTINATION ${CMAKE_INSTALL_LIBDIR})

#-----------------------------------------------------------------------
# Generate the pkg-config file

set(prefix ${CMAKE_INSTALL_PREFIX})
configure_file(fleet.pc.in ${CMAKE_CURRENT_BINARY_DIR}/fleet.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fleet.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
