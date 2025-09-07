// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <csignal>

#include <vsomeip/vsomeip.hpp>

#include <iceoryx2_bridge.hpp>
#include <csignal>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <unistd.h>

void handle_signal(int) {
    printf("signal received, exiting\n");
    exit(-1);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

    SomeipTunnel tunnel;
    tunnel.init();

    pthread_sigmask(SIG_UNBLOCK, &set, nullptr);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    tunnel.start();
}
