# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 3.13)
project (tunnel)

find_package(Threads REQUIRED)


list(APPEND CMAKE_PREFIX_PATH ${ICEORYX2_INSTALL_DIR})
find_package(iceoryx2-cxx REQUIRED)

set(VSOMEIP_NAME "vsomeip3")


add_executable(tunnel)
target_sources(tunnel PRIVATE someip_tunnel.cpp iceoryx2_bridge.cpp)
target_link_libraries(tunnel PRIVATE vsomeip3 Threads::Threads iceoryx2-cxx::static-lib-cxx)

target_compile_features(tunnel INTERFACE cxx_std_17)

target_include_directories(tunnel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)