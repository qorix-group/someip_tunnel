// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <csignal>

#include <memory>
#include <vsomeip/vsomeip.hpp>

#include <iceoryx2_bridge.hpp>
#include <csignal>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <vsomeip/internal/logger.hpp>

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

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    auto node = iox2::NodeBuilder().create<iox2::ServiceType::Ipc>().expect("successful node creation");
    auto listener = node.service_builder(iox2::ServiceName::create("LifetimeFromGateway").expect("valid service name"))
                            .event()
                            .event_id_max_value(0xFFFFFFFFFFFF)
                            .open_or_create()
                            .expect("successful service creation/opening")
                            .listener_builder()
                            .create()
                            .expect("successful publisher creation");

    auto notifier = node.service_builder(iox2::ServiceName::create("LifetimeToGateway").expect("valid service name"))
                            .event()
                            .event_id_max_value(0xFFFFFFFFFFFF)
                            .open_or_create()
                            .expect("successful service creation/opening")
                            .notifier_builder()
                            .create()
                            .expect("successful publisher creation");

    auto runtime = vsomeip::runtime::get();
    auto routing = std::thread([&]() {
        auto app = runtime->create_application("Routing");
        app->init();
        app->start();
    });

    std::unique_ptr<SomeipTunnel> tunnel;
    std::thread thread;
    uint64_t currentPid = 0;

    std::cout << "Entering gateway wait loop" << std::endl;
    do {

        auto event = listener.blocking_wait_one();
        std::cout << "Event " << std::endl;

        if (currentPid == event->value().as_value()) {
            continue;
        }

        currentPid = event->value().as_value();

        if (tunnel) {

            std::cout << "Restart of SOME/IP gateway detected" << std::endl;
            tunnel->stop();
            thread.join();
        }

        std::cout << "Starting new instance of Tunnel" << std::endl;

        pthread_sigmask(SIG_BLOCK, &set, nullptr);
        tunnel.reset(new SomeipTunnel(runtime));
        tunnel->init();
        pthread_sigmask(SIG_UNBLOCK, &set, nullptr);

        thread = std::thread{[&] { tunnel->start(); }};

        VSOMEIP_INFO << "Tunnel ready, notifying!";
        notifier.notify().expect("Notification shall work");

    } while (true);
}
