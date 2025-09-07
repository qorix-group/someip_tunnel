// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#include <csignal>
#endif
#include <vsomeip/vsomeip.hpp>
#include "hello_world_service.hpp"
#include <iceoryx2_bridge.hpp>

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
hello_world_service* hw_srv_ptr(nullptr);
void handle_signal(int _signal) {
    if (hw_srv_ptr != nullptr && (_signal == SIGINT || _signal == SIGTERM))
        hw_srv_ptr->terminate();
}
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    SomeipTunnel tunnel;

    tunnel.init();
    tunnel.start();
}
