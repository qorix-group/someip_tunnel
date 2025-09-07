# someip_tunnel


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
./build/examples/tunnel/someip_tunnel
```


This is the place where interaction with SOME/IP happens and all needed changes to tunnel for/to RUST shall be done here