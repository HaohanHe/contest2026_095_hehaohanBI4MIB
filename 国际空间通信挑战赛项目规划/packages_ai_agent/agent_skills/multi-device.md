# Multi-Device Collaboration

Coordinate tasks across multiple AI Agent devices via Node protocol.

## When to use
When user mentions another device, cross-device action, send to watch,
ask the speaker, or any multi-device scenario.

## How to use
AI Agent supports two modes:
- Hub mode: this device accepts connections from other Nodes
- Node mode: this device connects to an OpenClaw Gateway

### Check connected devices
run_shell "node_list" to see connected Nodes and their available tools.

### Call a remote tool
Remote Node tools appear in the tool list as "node:<device_id>:<tool_name>".
Call them like any other tool.

### Forward a message
To send a chat message to another device via the gateway:
The node_client automatically forwards messages using chat.forward protocol.

### Setup
- Hub mode: ws_server listens on port 28789 automatically
- Node mode: run_shell "set_gateway <host> <port> <token>" then "node_start"

## Example
User: "Ask the speaker to play music"
1. run_shell "node_list" to find the speaker node
2. Call node:speaker-01:music_play with the desired track
