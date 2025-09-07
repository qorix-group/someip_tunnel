# Tunnel Example README

## Testing the Tunnel with Rust

A simple way to test the tunnel is:

- Run the `hello_world` client example from the C++ side:
  ```bash
  ./build/examples/hello_world/hello_world_client
  ```
- Implement the SOME/IP service logic on the Rust side, connecting to the tunnel via Iceoryx2 (`TunnelFromRust` and `TunnelToRust`).

This setup allows you to verify that messages from the C++ client are received and handled by your Rust service implementation through the tunnel.

Refer to the main project README for details on message formats and integration points.
