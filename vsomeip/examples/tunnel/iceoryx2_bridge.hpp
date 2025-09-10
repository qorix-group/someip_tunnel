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
#include "iox/vector.hpp"

enum class TunnelMsgType : uint32_t {
    OFFER_SERVICE,
    FIND_SERVICE,
    OFFER_SERVICE_ACK,
    FIND_SERVICE_ACK,
    MESSAGE,
};

enum class EventType : uint32_t {
    Field,
    Event,
};

struct SomeIPEventDesc {
    static constexpr const char* IOX2_TYPE_NAME = "SomeIPEventDesc";
    uint16_t event_id;
    uint16_t event_groups[4];
    uint8_t len;
    EventType typ;
};

struct FindServiceEntry {
    static constexpr const char* IOX2_TYPE_NAME = "FindServiceEntry";
    SomeIPEventDesc event_infos[10];
    uint8_t len;
    // We can add other infos if we need
};

struct SomeipTunnelHeader {
    static constexpr const char* IOX2_TYPE_NAME = "SomeipTunnelHeader";

    TunnelMsgType type;
    uint16_t service_id;
    uint16_t instance_id;
    uint16_t method_id;
    FindServiceEntry find_service_metadata; // only when typ == FindService

    bool is_active; // Only relevant for FindServiceAck

    uint64_t id; // if comes != 0 then shall be rewritten for response.
};

struct SomeipTunnelPayload {
    static constexpr const char* IOX2_TYPE_NAME = "SomeipTunnelPayload";

    uint16_t length;
    uint8_t payload[1500];
};

inline std::ostream& operator<<(std::ostream& os, const EventType& typ) {
    switch (typ) {
    case EventType::Field:
        os << "Field";
        break;
    case EventType::Event:
        os << "Event";
        break;
    default:
        os << "Unknown";
        break;
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const SomeIPEventDesc& desc) {
    os << "{event_id: " << desc.event_id << ", event_groups: [";
    for (size_t i = 0; i < desc.len; ++i) {
        os << desc.event_groups[i];
        if (i + 1 < desc.len)
            os << ", ";
    }
    os << "], typ: " << desc.typ << "}";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const FindServiceEntry& entry) {
    os << "{event_infos: [";
    for (size_t i = 0; i < entry.len; ++i) {
        os << entry.event_infos[i];
        if (i + 1 < entry.len)
            os << ", ";
    }
    os << "]}";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const SomeipTunnelHeader& value) {
    os << "SomeipTunnelHeader { type: " << static_cast<uint32_t>(value.type) << ", service_id: " << value.service_id
       << ", instance_id: " << value.instance_id << ", method_id: " << value.method_id
       << ", find_service_metadata: " << value.find_service_metadata << ", id: " << value.id << " }";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const SomeipTunnelPayload& value) {
    os << "SomeipTunnelPayload { len: " << value.length << ", payload = ";
    for (int i = 0; i < value.length; i++) {
        os << std::hex << std::setw(2) << std::setfill('0') << (int)value.payload[i] << " ";
    }
    os << std::dec << "}";
    return os;
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
    void serviceStateChanged(vsomeip_v3::service_t, vsomeip_v3::instance_t, bool);
};
