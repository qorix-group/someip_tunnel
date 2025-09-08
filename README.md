# someip_tunnel

# prerequisites

- git LFS
- boost >= 1.66
- libacl

```bash
sudo apt-get install -y \
   git-lfs \
   libboost-all-dev \
   libacl1-dev
```

# rootfs folder

This is prebuild folder for iceoryx v0.6.1 !!!! If You want to use different version or something please follow full guide building iceoryx2 in their repo

# vsomeip - copy of COVESA vsomeip with added tunnel to iceoryx2

## Building vsomeip

1. Source the environment variables:
   ```bash
   source scripts/env
   ```
2. Build vsomeip:
   ```bash
   scripts/build_vsomeip.sh
   ```

## Running examples

1. Source the environment variables (if not already done):
   ```bash
   source scripts/env
   ```
2. Run an example (replace `<example>` with the actual example name):
   ```bash
   ./build/examples/<example>
   ```

- Make sure the required configuration file is present: `$VSOMEIP_CONFIGURATION` (default: `vsomeip/config/vsomeip-local.json`).
- The `LD_LIBRARY_PATH` is set to include `rootfs/lib` and `build`.

## Tunnel Example

The tunnel example is located in:
- `vsomeip/examples/tunnel/`
  - Main source: `someip_tunnel.cpp`
  - Bridge implementation: `iceoryx2_bridge.cpp`, `iceoryx2_bridge.hpp`

After building, the tunnel example binary will be in:
- `build/examples/tunnel/`

Run it as described above, for example:
```bash
./build/examples/tunnel/tunnel
```


This is the place where interaction with SOME/IP happens and all needed changes to tunnel for/to RUST shall be done here. THIS IS WIP, ALL INTERACTION WITH SOMEIP DAEMON SHALL BE ADDED IN HERE

## How the Tunnel to/from Rust Works

The tunnel example bridges SOME/IP and Rust using Iceoryx2 IPC services:

- **TunnelToRust**: SOME/IP messages received by the tunnel are packed into a header (`SomeipTunnelHeader`) and payload (`SomeipTunnelPayload`), then published to the `TunnelToRust` Iceoryx2 service. Rust code can subscribe to this service to receive SOME/IP messages.
- **TunnelFromRust**: The tunnel subscribes to the `TunnelFromRust` Iceoryx2 service. Rust code can publish messages to this service using the same header and payload format. The tunnel will unpack these messages and send them as SOME/IP responses or service offers.

### Message Format
- Header: Contains type, service/instance/method IDs, and a unique ID.
- Payload: Contains the raw SOME/IP message data.

### Rust Integration
- Subscribe to `TunnelToRust` to receive SOME/IP messages from C++.
- Publish to `TunnelFromRust` to send messages to SOME/IP via the tunnel.
- Use the same header and payload structures as defined in `iceoryx2_bridge.hpp`.

This mechanism allows seamless communication between Rust and SOME/IP using shared memory IPC.

Rust compatible data types for PubSub:
```rust
#[derive(Debug, ZeroCopySend)]
#[repr(C)]
#[type_name("TunnelMsgType")]
pub enum TunnelMsgType {
    OfferService = 0,
    FindService = 1,
    OfferServiceAck = 2,
    FindServiceAck = 3,
    Message = 4,
}

#[derive(Debug, ZeroCopySend)]
#[repr(C)]
#[type_name("SomeipTunnelHeader")]
pub struct SomeipTunnelHeader {
    type_: TunnelMsgType,

    // below fields are optional based on type, for simplicity i did not modeled that
    service_id: u16,
    instance_id: u16,
    method_id: u16,

    id: u64, // if comes != 0 then shall be rewritten for response.
}

#[derive(Debug, ZeroCopySend)]
#[repr(C)]
#[type_name("SomeipTunnelPayload")]
pub struct SomeipTunnelPayload {
    length: u16,
    payload: [u8; 1500],
}

```