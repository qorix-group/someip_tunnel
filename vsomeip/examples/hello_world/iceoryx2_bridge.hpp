#pragma once
#include <cstdint>

#include "iox/duration.hpp"
#include "iox2/event_id.hpp"
#include "iox2/log.hpp"
#include "iox2/node.hpp"
#include "iox2/service_name.hpp"
#include "iox2/service_type.hpp"
#include <iox2/subscriber.hpp>
#include <thread>
#include <unordered_map>
#include <vsomeip/vsomeip.hpp>
#include <random>
#include <iostream>
#include <iomanip>

enum class TunnelMsgType : uint32_t {
    OFFER_SERVICE,
    FIND_SERVICE,
    OFFER_SERVICE_ACK,
    FIND_SERVICE_ACK,
    MESSAGE,
};

struct SomeipTunnelHeader {
    static constexpr const char* IOX2_TYPE_NAME = "SomeipTunnelHeader";

    TunnelMsgType type;

    // below fields are optional based on type, for simplicity i did not modeled that
    uint16_t service_id;
    uint16_t instance_id;
    uint16_t method_id;

    uint64_t id; // if comes != 0 then shall be rewritten for response.
};

struct SomeipTunnelPayload {
    static constexpr const char* IOX2_TYPE_NAME = "SomeipTunnelPayload";

    uint16_t length;
    uint8_t payload[1500];
};

inline auto operator<<(std::ostream& stream, const SomeipTunnelPayload& value) -> std::ostream& {
    stream << "SomeipTunnelPayload { len: " << value.length << ", payload = " << std::hex << std::setw(4) << std::setfill('0');
    for (int i = 0; i < value.length; i++) {
        stream << " " << (int)value.payload[i];
    }

    stream << std::dec << "}";

    return stream;
}

inline auto operator<<(std::ostream& stream, const SomeipTunnelHeader& value) -> std::ostream& {
    stream << "SomeipTunnelHeader { type: " << static_cast<uint32_t>(value.type) << ", service_id: " << std::hex << std::setw(4)
           << std::setfill('0') << value.service_id << ", instance_id: " << std::hex << std::setw(4) << std::setfill('0')
           << value.instance_id << ", method_id: " << std::hex << std::setw(4) << std::setfill('0') << value.method_id
           << ", id: " << std::dec << value.id << " }";
    return stream;
}

class SomeipTunnel {

public:
    void start();
    void init();

    SomeipTunnel();

private:
    iox2::Node<iox2::ServiceType::Ipc> mNode;
    std::thread mRecv;
    std::shared_ptr<vsomeip::runtime> mRtm;
    std::shared_ptr<vsomeip::application> mApp;
    iox2::Publisher<iox2::ServiceType::Ipc, SomeipTunnelPayload, SomeipTunnelHeader> mToGateway;
    iox2::Subscriber<iox2::ServiceType::Ipc, SomeipTunnelPayload, SomeipTunnelHeader> mFromGateway;

    std::unordered_map<uint64_t, std::shared_ptr<vsomeip_v3::message>> mMessagesInProgress;

    std::random_device mRd; // seed source (non-deterministic, if available)
    std::mt19937_64 mGen; // 64-bit Mersenne Twister
    std::uniform_int_distribution<uint64_t> mDist;

    void fromGateway();
    void recvInternal();
    void fromSomeip(const std::shared_ptr<vsomeip_v3::message>& msg);
    void incommingMsg(const iox2::Sample<iox2::ServiceType::Ipc, SomeipTunnelPayload, SomeipTunnelHeader>& sample);
};
