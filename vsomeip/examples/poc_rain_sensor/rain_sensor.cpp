// Copyright (C) 2014-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include <mutex>

#include <vsomeip/vsomeip.hpp>

#define RAIN_SENSOR_SERVICE_ID       0x1001
#define RAIN_SENSOR_INSTANCE_ID      0x1
#define RAIN_SENSOR_EVENT_ID         0x8004
#define RAIN_SENSOR_GET_METHOD_ID    0x0001
#define RAIN_SENSOR_SET_METHOD_ID    0x0002
#define RAIN_SENSOR_EVENTGROUP_ID    0x4

class service_rain_sensor {
public:
    service_rain_sensor(uint32_t _cycle) :
        app_(vsomeip::runtime::get()->create_application("RainSensor")), is_registered_(false), cycle_(_cycle), blocked_(false),
        running_(true), is_offered_(false), offer_thread_(std::bind(&service_rain_sensor::run, this)),
        notify_thread_(std::bind(&service_rain_sensor::notify, this)) { }

    bool init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }
        app_->register_state_handler(std::bind(&service_rain_sensor::on_state, this, std::placeholders::_1));

        app_->register_message_handler(RAIN_SENSOR_SERVICE_ID, RAIN_SENSOR_INSTANCE_ID, RAIN_SENSOR_GET_METHOD_ID,
                                       std::bind(&service_rain_sensor::on_get, this, std::placeholders::_1));

        app_->register_message_handler(RAIN_SENSOR_SERVICE_ID, RAIN_SENSOR_INSTANCE_ID, RAIN_SENSOR_SET_METHOD_ID,
                                       std::bind(&service_rain_sensor::on_set, this, std::placeholders::_1));

        std::set<vsomeip::eventgroup_t> its_groups;
        its_groups.insert(RAIN_SENSOR_EVENTGROUP_ID);
        app_->offer_event(RAIN_SENSOR_SERVICE_ID, RAIN_SENSOR_INSTANCE_ID, RAIN_SENSOR_EVENT_ID, its_groups, vsomeip::event_type_e::ET_FIELD,
                          std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip::reliability_type_e::RT_UNKNOWN);
        {
            std::lock_guard<std::mutex> its_lock(payload_mutex_);
            payload_ = vsomeip::runtime::get()->create_payload();
        }

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
        app_->offer_service(RAIN_SENSOR_SERVICE_ID, RAIN_SENSOR_INSTANCE_ID);
        is_offered_ = true;
        notify_condition_.notify_one();
    }

    void stop_offer() {
        app_->stop_offer_service(RAIN_SENSOR_SERVICE_ID, RAIN_SENSOR_INSTANCE_ID);
        is_offered_ = false;
    }

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
                  << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.") << std::endl;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
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
        app_->notify(RAIN_SENSOR_SERVICE_ID, RAIN_SENSOR_INSTANCE_ID, RAIN_SENSOR_EVENT_ID, payload_);
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
        vsomeip::byte_t its_data[1];
        while (running_) {
            std::unique_lock<std::mutex> its_lock(notify_mutex_);
            while (!is_offered_ && running_)
                notify_condition_.wait(its_lock);
            while (is_offered_ && running_) {
                its_data[0] ^= static_cast<uint8_t>(1);
                {
                    std::lock_guard<std::mutex> its_lock(payload_mutex_);
                    payload_->set_data(its_data, 1);

                    std::cout << "Rain sensor status: " << static_cast<int>(its_data[0]) << std::endl;
                    app_->notify(RAIN_SENSOR_SERVICE_ID, RAIN_SENSOR_INSTANCE_ID, RAIN_SENSOR_EVENT_ID, payload_);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
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
service_rain_sensor* its_sample_ptr(nullptr);
void handle_signal(int _signal) {
    if (its_sample_ptr != nullptr && (_signal == SIGINT || _signal == SIGTERM))
        its_sample_ptr->stop();
}
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    service_rain_sensor its_sample(5000);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_sample_ptr = &its_sample;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    if (its_sample.init()) {
        its_sample.start();
#ifdef VSOMEIP_ENABLE_SIGNAL_HANDLING
        its_sample.stop();
#endif
        return 0;
    } else {
        return 1;
    }
}
