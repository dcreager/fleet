# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2014, RedJack, LLC.
# All rights reserved.
#
# Please see the COPYING file in this distribution for license details.
# ----------------------------------------------------------------------

include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${CMAKE_BINARY_DIR}/src)

set(EXAMPLES_SRC
    fleet-examples.c
    run-example.c
    # actual examples below
    concurrent-batched.c
    concurrent-unbatched.c
    sequential-return.c
    sequential-run.c
)

add_executable(fleet-examples ${EXAMPLES_SRC})
target_link_libraries(fleet-examples libfleet)
