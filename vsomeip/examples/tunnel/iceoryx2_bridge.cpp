#include "iceoryx2_bridge.hpp"
#include "vsomeip/constants.hpp"
#include "vsomeip/message.hpp"
#include "vsomeip/primitive_types.hpp"
#include <cstring>
#include <functional>
#include <cassert>
#include <iterator>
#include <iox2/node.hpp>
#include <mutex>
#include <thread>
#include <vsomeip/internal/logger.hpp>
constexpr iox::units::Duration CYCLE_TIME = iox::units::Duration::fromMilliseconds(100);

SomeipTunnel::SomeipTunnel(std::shared_ptr<vsomeip_v3::runtime> runtime) :
    mNode{iox2::NodeBuilder().create<iox2::ServiceType::Ipc>().expect("successful node creation")}, mRecv{}, mShallRun{true}, mRtm{runtime},
    mApp{mRtm->create_application("Tunnel")},
    mToGateway{mNode.service_builder(iox2::ServiceName::create("TunnelToRust").expect("valid service name"))
                       .publish_subscribe<SomeipTunnelPayload>()
                       .user_header<SomeipTunnelHeader>()
                       .subscriber_max_buffer_size(20)
                       .history_size(20)
                       .open_or_create()
                       .expect("successful service creation/opening")
                       .publisher_builder()
                       .create()
                       .expect("successful publisher creation")},
    mFromGateway{mNode.service_builder(iox2::ServiceName::create("TunnelFromRust").expect("valid service name"))
                         .publish_subscribe<SomeipTunnelPayload>()
                         .user_header<SomeipTunnelHeader>()
                         .subscriber_max_buffer_size(20)
                         .history_size(20)
                         .open_or_create()
                         .expect("successful service creation/opening")
                         .subscriber_builder()
                         .create()
                         .expect("successful subscriber creation")},
    mMessagesInProgress{}, mRd{}, mGen{}, mDist{mGen()} {

    mRecv = std::thread{&SomeipTunnel::recvInternal, this};
}

void on_state(vsomeip::state_type_e _state) {
    if (_state == vsomeip::state_type_e::ST_REGISTERED) {
        VSOMEIP_INFO << "REGISTERED WORKING";
    }
}

void SomeipTunnel::init() {

    // init the application
    if (!mApp->init()) {
        VSOMEIP_ERROR << "Couldn't initialize application";
        return;
    }

    mApp->register_state_handler(&on_state);

    mApp->register_message_handler(vsomeip_v3::ANY_SERVICE, vsomeip_v3::ANY_INSTANCE, vsomeip_v3::ANY_METHOD,
                                   std::bind(&SomeipTunnel::fromSomeip, this, std::placeholders::_1));

    mApp->register_availability_handler(
            vsomeip_v3::ANY_SERVICE, vsomeip_v3::ANY_INSTANCE,
            std::bind(&SomeipTunnel::serviceStateChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void SomeipTunnel::stop() {
    mApp->stop();

    mShallRun.store(false);
    mRecv.join();
}

void SomeipTunnel::serviceStateChanged(vsomeip_v3::service_t s, vsomeip_v3::instance_t i, bool active) {
    VSOMEIP_INFO << "Received serviceStateChanged from SOME/IP service " << s << " instance " << i << " active " << active;

    std::lock_guard<std::mutex> lck{mToGatewayMutex};

    auto new_sample = mToGateway.loan().expect("should have sample");

    SomeipTunnelHeader& header = new_sample.user_header_mut();

    header.type = TunnelMsgType::FIND_SERVICE_ACK;

    header.instance_id = i;
    header.service_id = s;
    header.is_active = active;

    ::iox2::send(std::move(new_sample)).expect("Sending sample shall work");
    VSOMEIP_INFO << "Service state changed was send via tunnel!";
}

void SomeipTunnel::fromSomeip(const std::shared_ptr<vsomeip_v3::message>& msg) {

    std::lock_guard<std::mutex> lck{mToGatewayMutex};
    auto new_sample = mToGateway.loan().expect("should have ssample");

    SomeipTunnelHeader& header = new_sample.user_header_mut();

    header.type = TunnelMsgType::MESSAGE;

    header.instance_id = msg->get_instance();
    header.service_id = msg->get_service();
    header.method_id = msg->get_method();

    header.id = mDist(mGen);

    SomeipTunnelPayload& payload = new_sample.payload_mut();
    auto l = msg->get_payload()->get_length();

    assert(l < 15000);

    payload.length = (uint16_t)l;
    std::memcpy(payload.payload, msg->get_payload()->get_data(), l);

    mMessagesInProgress.emplace(header.id, msg);

    VSOMEIP_INFO << "Passing over tunnel message with header: " << header << ", payload: " << payload;
    ::iox2::send(std::move(new_sample)).expect("Sending sample shall work");
}

void SomeipTunnel::start() {

    mApp->start(); // blocking
    mApp->clear_all_handler();
}

void SomeipTunnel::recvInternal() {

    auto counter = 0;

    while (mNode.wait(CYCLE_TIME).has_value() && mShallRun) {

        counter += 1;
        this->fromGateway();
    }
}

void SomeipTunnel::fromGateway() {

    do {
        auto sample = mFromGateway.receive();

        if (sample.has_error()) {
            std::cout << "Recv error ?!" << std::endl;
            break;
        }

        if (!sample->has_value()) {

            // std::cout << "NO sample ?!" << std::endl;
            break;
        }

        auto& s = sample->value();

        this->incommingMsg(s);

    } while (true);
}

void SomeipTunnel::incommingMsg(const iox2::Sample<iox2::ServiceType::Ipc, SomeipTunnelPayload, SomeipTunnelHeader>& sample) {

    auto header = sample.user_header();

    VSOMEIP_INFO << "Sample received, header: " << header << ", payload: " << sample.payload();

    switch (header.type) {
    case TunnelMsgType::OFFER_SERVICE: {

        for (int i = 0; i < header.service_metadata.len; i++) {
            std::set<vsomeip::eventgroup_t> its_groups;
            for (int j = 0; j < header.service_metadata.len; j++) {
                its_groups.insert(header.service_metadata.event_infos[i].event_groups[j]);
            }

            mApp->offer_event(header.service_id, header.instance_id, header.service_metadata.event_infos[i].event_id, its_groups,
                              (vsomeip_v3::event_type_e)header.service_metadata.event_infos[i].typ);
        }

        mApp->offer_service(header.service_id, header.instance_id);
        break;
    }
    case TunnelMsgType::FIND_SERVICE: {

        for (int i = 0; i < header.service_metadata.len; i++) {
            std::set<vsomeip::eventgroup_t> its_groups;
            for (int j = 0; j < header.service_metadata.len; j++) {
                its_groups.insert(header.service_metadata.event_infos[i].event_groups[j]);
            }

            mApp->request_event(header.service_id, header.instance_id, header.service_metadata.event_infos[i].event_id, its_groups,
                                (vsomeip_v3::event_type_e)header.service_metadata.event_infos[i].typ);
            mApp->subscribe(header.service_id, header.instance_id, *its_groups.begin());
        }

        mApp->request_service(header.service_id, header.instance_id);
        break;
    }
    case TunnelMsgType::OFFER_SERVICE_ACK:
    case TunnelMsgType::FIND_SERVICE_ACK:
    case TunnelMsgType::MESSAGE: {
        auto request = mMessagesInProgress.at(header.id);

        std::shared_ptr<vsomeip::message> resp = mRtm->create_response(request);

        auto data = sample.payload();

        // Create a payload which will be sent back to the client
        std::shared_ptr<vsomeip::payload> resp_pl = mRtm->create_payload();

        auto end = std::begin(data.payload);
        std::advance(end, data.length);

        std::vector<vsomeip::byte_t> pl_data(std::begin(data.payload), end);
        resp_pl->set_data(pl_data);
        resp->set_payload(resp_pl);

        // Send the response back
        mApp->send(resp);
        break;
    }

    case TunnelMsgType::EVENT: {

        auto data = mRtm->create_payload();

        auto msg_data = sample.payload();

        auto end = std::begin(msg_data.payload);
        std::advance(end, msg_data.length);

        std::vector<vsomeip::byte_t> pl_data(std::begin(msg_data.payload), end);
        data->set_data(pl_data);

        // Send notification
        mApp->notify(header.service_id, header.instance_id, header.method_id, data);
        break;
    }
    }
}
