// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#include <csignal>
#endif
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

struct windows_position_struct {
    uint8_t fl, fr, rl, rr;
}windows_position_data;

bool rain_status = false;

#define WINDOWS_POSITION_SERVICE_ID       0x1000
#define WINDOWS_POSITION_INSTANCE_ID      0x1
#define WINDOWS_POSITION_EVENT_ID         0x8003
#define WINDOWS_POSITION_GET_METHOD_ID    0x0001
#define WINDOWS_POSITION_SET_METHOD_ID    0x0002
#define WINDOWS_POSITION_EVENTGROUP_ID    0x3

#define WINDOWS_CLOSE_SERVICE_ID       0x1010
#define WINDOWS_CLOSE_INSTANCE_ID      0x1
#define WINDOWS_CLOSE_EVENT_ID         0x8015
#define WINDOWS_CLOSE_GET_METHOD_ID    0x0001
#define WINDOWS_CLOSE_SET_METHOD_ID    0x0002
#define WINDOWS_CLOSE_EVENTGROUP_ID    15

class windows_position {
public:
    windows_position(uint32_t _cycle) :
        app_(vsomeip::runtime::get()->create_application()), is_registered_(false), cycle_(_cycle), blocked_(false), running_(true),
        is_offered_(false), offer_thread_(std::bind(&windows_position::run, this)), notify_thread_(std::bind(&windows_position::notify, this)) {
    }

    bool init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }
        app_->register_state_handler(std::bind(&windows_position::on_state, this, std::placeholders::_1));

        app_->register_message_handler(WINDOWS_POSITION_SERVICE_ID, WINDOWS_POSITION_INSTANCE_ID, WINDOWS_POSITION_GET_METHOD_ID,
                                       std::bind(&windows_position::on_get, this, std::placeholders::_1));

        app_->register_message_handler(WINDOWS_POSITION_SERVICE_ID, WINDOWS_POSITION_INSTANCE_ID, WINDOWS_POSITION_SET_METHOD_ID,
                                       std::bind(&windows_position::on_set, this, std::placeholders::_1));

        std::set<vsomeip::eventgroup_t> its_groups;
        its_groups.insert(WINDOWS_POSITION_EVENTGROUP_ID);
        app_->offer_event(WINDOWS_POSITION_SERVICE_ID, WINDOWS_POSITION_INSTANCE_ID, WINDOWS_POSITION_EVENT_ID, its_groups, vsomeip::event_type_e::ET_FIELD,
                          std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip::reliability_type_e::RT_UNKNOWN);
        {
            std::lock_guard<std::mutex> its_lock(payload_mutex_);
            payload_ = vsomeip::runtime::get()->create_payload();
        }

        app_->register_message_handler(vsomeip::ANY_SERVICE, WINDOWS_CLOSE_INSTANCE_ID, vsomeip::ANY_METHOD,
                                       std::bind(&windows_position::on_message, this, std::placeholders::_1));

        app_->register_availability_handler(
                WINDOWS_CLOSE_SERVICE_ID, WINDOWS_CLOSE_INSTANCE_ID,
                std::bind(&windows_position::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        std::set<vsomeip::eventgroup_t> its_groups2;
        its_groups2.insert(WINDOWS_CLOSE_EVENTGROUP_ID);
        app_->request_event(WINDOWS_CLOSE_SERVICE_ID, WINDOWS_CLOSE_INSTANCE_ID, WINDOWS_CLOSE_EVENT_ID, its_groups2, vsomeip::event_type_e::ET_FIELD);
        app_->subscribe(WINDOWS_CLOSE_SERVICE_ID, WINDOWS_CLOSE_INSTANCE_ID, WINDOWS_CLOSE_EVENTGROUP_ID);

        blocked_ = true;
        condition_.notify_one();
        return true;
    }

    void start() { app_->start(); }

    void stop() {
        running_ = false;
        blocked_ = true;
        condition_.notify_one();
        notify_condition_.notify_one();
        app_->clear_all_handler();
        app_->unsubscribe(WINDOWS_CLOSE_SERVICE_ID, WINDOWS_CLOSE_INSTANCE_ID, WINDOWS_CLOSE_EVENTGROUP_ID);
        app_->release_event(WINDOWS_CLOSE_SERVICE_ID, WINDOWS_CLOSE_INSTANCE_ID, WINDOWS_CLOSE_EVENT_ID);
        app_->release_service(WINDOWS_CLOSE_SERVICE_ID, WINDOWS_CLOSE_INSTANCE_ID);
        stop_offer();
        if (std::this_thread::get_id() != offer_thread_.get_id()) {
            if (offer_thread_.joinable()) {
                offer_thread_.join();
            }
        } else {
            offer_thread_.detach();
        }
        if (std::this_thread::get_id() != notify_thread_.get_id()) {
            if (notify_thread_.joinable()) {
                notify_thread_.join();
            }
        } else {
            notify_thread_.detach();
        }
        app_->stop();
    }

    void offer() {
        std::lock_guard<std::mutex> its_lock(notify_mutex_);
        app_->offer_service(WINDOWS_POSITION_SERVICE_ID, WINDOWS_POSITION_INSTANCE_ID);
        is_offered_ = true;
        notify_condition_.notify_one();
    }

    void stop_offer() {
        app_->stop_offer_service(WINDOWS_POSITION_SERVICE_ID, WINDOWS_POSITION_INSTANCE_ID);
        is_offered_ = false;
    }

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
                  << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.") << std::endl;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            app_->request_service(WINDOWS_CLOSE_SERVICE_ID, WINDOWS_CLOSE_INSTANCE_ID);
            if (!is_registered_) {
                is_registered_ = true;
            }
        } else {
            is_registered_ = false;
        }
    }

    void on_get(const std::shared_ptr<vsomeip::message>& _message) {
        std::shared_ptr<vsomeip::message> its_response = vsomeip::runtime::get()->create_response(_message);
        {
            std::lock_guard<std::mutex> its_lock(payload_mutex_);
            its_response->set_payload(payload_);
        }
        app_->send(its_response);
    }

    void on_set(const std::shared_ptr<vsomeip::message>& _message) {
        std::shared_ptr<vsomeip::message> its_response = vsomeip::runtime::get()->create_response(_message);
        {
            std::lock_guard<std::mutex> its_lock(payload_mutex_);
            payload_ = _message->get_payload();
            its_response->set_payload(payload_);
        }

        app_->send(its_response);
        app_->notify(WINDOWS_POSITION_SERVICE_ID, WINDOWS_POSITION_INSTANCE_ID, WINDOWS_POSITION_EVENT_ID, payload_);
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        offer();

        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
        }
    }

    void notify() {
        vsomeip::byte_t its_data[4];
        while (running_) {
            std::unique_lock<std::mutex> its_lock(notify_mutex_);
            while (!is_offered_ && running_)
                notify_condition_.wait(its_lock);
            while (is_offered_ && running_) {
                {
                    std::lock_guard<std::mutex> its_lock(payload_mutex_);
                    bool changed = false;
                    if (rain_status && windows_position_data.fl < 100) {
                        windows_position_data.fl++;
                        windows_position_data.fr++;
                        windows_position_data.rl++;
                        windows_position_data.rr++;
                        changed = true;
                    } else if (!rain_status && windows_position_data.fl > 0) {
                        windows_position_data.fl--;
                        windows_position_data.fr--;
                        windows_position_data.rl--;
                        windows_position_data.rr--;
                        changed = true;
                    }

                    if (changed) {
                        its_data[0] = static_cast<uint8_t>(windows_position_data.fl);
                        its_data[1] = static_cast<uint8_t>(windows_position_data.fr);
                        its_data[2] = static_cast<uint8_t>(windows_position_data.rl);
                        its_data[3] = static_cast<uint8_t>(windows_position_data.rr);
                        payload_->set_data(its_data, 4);

                        std::cout << "Windows status: " << static_cast<int>(windows_position_data.fl) << " " << static_cast<int>(windows_position_data.fr) << " "
                                << static_cast<int>(windows_position_data.rl) << " " << static_cast<int>(windows_position_data.rr) << std::endl;
                        app_->notify(WINDOWS_POSITION_SERVICE_ID, WINDOWS_POSITION_INSTANCE_ID, WINDOWS_POSITION_EVENT_ID, payload_);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        std::cout << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << _instance << "] is "
                  << (_is_available ? "available." : "NOT available.") << std::endl;
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _response) {
        std::stringstream its_message;
        std::shared_ptr<vsomeip::payload> its_payload = _response->get_payload();
        its_message << "(" << std::dec << its_payload->get_length() << ") " << std::hex;
        for (uint32_t i = 0; i < its_payload->get_length(); ++i)
            its_message << std::setw(2) << static_cast<int>(its_payload->get_data()[i]) << " ";
        std::cout << "Rain status received: " << (its_payload->get_data()[0] != 0) << std::endl;
        {
            rain_status = its_payload->get_data()[0] != 0;
        }
        std::cout << its_message.str() << std::endl;
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
    uint32_t cycle_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    bool running_;

    std::mutex notify_mutex_;
    std::condition_variable notify_condition_;
    bool is_offered_;

    std::mutex payload_mutex_;
    std::shared_ptr<vsomeip::payload> payload_;

    // blocked_ / is_offered_ must be initialized before starting the threads!
    std::thread offer_thread_;
    std::thread notify_thread_;
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
windows_position* windows_position_ptr(nullptr);
void handle_signal(int _signal) {
    if (windows_position_ptr != nullptr && (_signal == SIGINT || _signal == SIGTERM))
        windows_position_ptr->stop();
}
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    windows_position windows_position_instance(25);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    windows_position_ptr = &windows_position_instance;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    if (windows_position_instance.init()) {
        windows_position_instance.start();
#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
        windows_position_instance.stop();
#endif
        return 0;
    } else {
        return 1;
    }
}
