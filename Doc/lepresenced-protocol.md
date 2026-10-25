# Specifications for Implementing an `lepresenced` Compatible Server

This document describes the network protocol and state logic required to implement a server compatible with the FHEM `collectord` client, emulating the behavior of the original `lepresenced` (Bluetooth Low Energy Presence Daemon).

---

## 1. Transport and Network Layer

- **Protocol:** TCP (Stream)
- **Default Port:** `5333` (configurable)
- **Listen Address:** `0.0.0.0` (all interfaces) or `127.0.0.1`
- **Keep-Alive:** It is highly recommended to enable `SO_KEEPALIVE` on the socket to quickly detect client disconnections.
- **Encoding:** Plain text ASCII/UTF-8.
- **Message Terminator:** Every command and response **MUST** end with a newline character (`\n` or `0x0A`).

---

## 2. Message Formats

All messages are text strings. The server must handle multiple clients concurrently (using I/O multiplexing like `select`/`poll`/`epoll` or threading).

### 2.1 Client → Server Commands

The client (`collectord`) sends the following commands. The server must parse them after stripping leading/trailing whitespaces and the newline character.

| Command | Format | Description |
| :--- | :--- | :--- |
| **Register/Update** | `<MAC>\|<interval>\n` | Registers a device to monitor. `<MAC>` is the Bluetooth MAC address (e.g., `aa:bb:cc:dd:ee:ff`, case-insensitive). `<interval>` is an integer in seconds. The server must acknowledge and then send automatic state updates based on this interval. |
| **Force Update** | `now\n` | Forces an immediate state check for the MAC associated with this specific client connection, ignoring the timer. |
| **Ping** | `ping\n` | Health-check request. The server replies with statistics and **closes the connection** immediately after. |
| **Stop** | `stop\n` | Stop request. For compatibility with `lepresenced`, the server ignores the actual stop (scanning is continuous) but must reply with a specific acknowledgment. |

*Note:* Any unrecognized command should be logged internally by the server but **must not** generate an error response on the socket or close the connection (specific behavior of `lepresenced`).

### 2.2 Server → Client Responses

#### A. Synchronous Acknowledgments
These responses are sent immediately upon receiving a valid command.

| Received Command | Server Response |
| :--- | :--- |
| `<MAC>\|<interval>` | `command accepted\n` |
| `now` | `command accepted\n` |
| `stop` | `no command running\n` |
| `ping` | `pong [clients=<N>;devices=<N>;min_age=<N>;max_age=<N>;beacons_hcitool=<N>;beacons_hcidump=<N>;beacons_diff=<N>]\n` *(See note below)* |

> **Note on Ping:** The values in brackets are statistics. In a custom implementation, you can return dummy values (e.g., `0` or `1`) or calculated ones, as long as the string starts with `pong [` and ends with `]\n`. **Crucial:** The original daemon closes the TCP connection immediately after sending the `pong` response.

#### B. Asynchronous State Updates
The server sends these messages **spontaneously** to the client's socket when the client's `interval` timer expires, or when the RSSI signal changes significantly (default threshold: > 10 dBm).

The payload after the state keyword (`present` or `absence`) uses a flexible `key=value;key=value` format. 

**State: PRESENT**
```text
present;device_name=<name>;rssi=<value>;model=<model_string>;daemon=<daemon_string>\n
```
*Optional (if supported):* `;batteryPercent=<level>;batteryPercentAge=<age_in_hours>`

**State: ABSENT**
```text
absence;rssi=unreachable;model=<model_string>;daemon=<daemon_string>\n
```

---

## 3. Payload Structure and Field Requirements (Addon Data)

The client (`collectord`) and the underlying FHEM module parse the data following the first semicolon as generic key-value pairs. Understanding which fields are mandatory vs. optional is crucial for custom implementations.

### 3.1 Mandatory / Highly Recommended Fields
- **`rssi`**: **Mandatory**. The client uses this integer value to determine which room/server has the strongest signal when multiple servers report the same MAC. For `absence`, it must be the literal string `unreachable`.
- **`device_name`**: **Highly Recommended**. Used for display and logging in the FHEM UI.

### 3.2 Optional Fields (Battery)
- **`batteryPercent`**: **Optional**. An integer from **0 to 100** representing the battery percentage. If your server cannot read the battery, omit this field entirely.
- **`batteryPercentAge`**: **Optional**. Represents the age of the battery reading in hours. 
  - *Nuance:* While the original `lepresenced` always sends them as a pair, it is **not strictly mandatory** to include `batteryPercentAge` if you include `batteryPercent`. The client and FHEM will gracefully handle its absence. If you don't know the age, simply omit it.

### 3.3 Optional Fields (Metadata)
- **`model`** and **`daemon`**: **Optional & Free-form**. These are essentially metadata tags. The client **does not validate** them against a hardcoded list of allowed names. 
  - You can safely change them to identify your custom implementation (e.g., `daemon=MyCustomServer V1.0;model=CustomHardware`).
  - You can omit them entirely, and the presence detection will still work perfectly.

### 3.4 Minimum Valid Responses
If you want to implement the absolute bare minimum required for the protocol to function, these are the valid payloads:

**Minimum PRESENT:**
```text
present;rssi=-70\n
```

**Minimum ABSENT:**
```text
absence;rssi=unreachable\n
```

---

## 4. State and Presence Logic (Core Algorithm)

To be fully compatible, the server must not just "ping" the device, but implement the presence validation logic used by `lepresenced`.

### 4.1 Internal Data Structure
The server must maintain:
1. A list of **Connected Clients**: `{ socket, requested_mac, interval, next_check_time }`.
2. A global table of **Seen Devices**: `{ mac: { name, rssi, reported_rssi, last_timestamp, previous_timestamp } }`.

### 4.2 The `is_present` Algorithm
A device is considered "present" **only if** both of these conditions are met:
1. The device has been seen at least once (`last_timestamp` exists).
2. `current_time - last_timestamp <= client_requested_interval`
3. `current_time - previous_timestamp <= client_requested_interval`

> **Rationale:** This double-check prevents "false positives" caused by a single stray beacon packet, requiring the device to be seen consistently within the time window.

### 4.3 Update Triggers
The server must send a state update to the client when:
- `current_time >= client.next_check_time`.
- **OR**, if the new `rssi` differs from the `reported_rssi` by more than a configurable threshold (default: `10`). In this case, `next_check_time` is set to `0` (immediate).

After sending the update, the server updates `reported_rssi = current_rssi` and `next_check_time = current_time + interval`.

---

## 5. Connection Lifecycle Management

1. **New Connection:** Accept the socket, add it to the monitoring pool. Do not send any welcome message.
2. **Data Reception:** Read from the buffer until `\n`. Strip `\r` and `\n`. Parse the command.
3. **Disconnection (EOF or Read Error):** 
   - Remove the socket from the monitoring pool.
   - Delete all client-MAC associations from memory.
   - Close the socket cleanly.
   - *Note:* The `lepresenced` server continues to scan globally even if no clients are connected.

---

## 6. Implementation Recommendations

1. **Non-blocking I/O:** Use `select`, `poll`, `epoll` (Linux) or an async framework (e.g., `asyncio` in Python, `net` in Node.js) to handle multiple clients without blocking the Bluetooth scanning process.
2. **Case Insensitivity:** Always normalize MAC addresses to lowercase (`toLowerCase()`) both when receiving commands and during internal comparisons, as `collectord` and `lepresenced` work in lowercase.
3. **Robustness:** Silently ignore malformed commands (logging them internally) instead of closing the connection, to maximize compatibility with past or future client versions.
4. **Testing without Hardware:** To test your server without actual Bluetooth hardware, create a separate thread that periodically injects dummy devices into the "Seen Devices" table with updated timestamps, allowing you to verify that the server correctly sends `present;...` and `absence;...` messages to a test client.

---

## 7. Example Communication Flow

```text
[Client] -> [Server] : "aa:bb:cc:dd:ee:ff|30\n"
[Server] -> [Client] : "command accepted\n"

// ... 30 seconds pass, server detects the device ...

[Server] -> [Client] : "present;device_name=Tile;rssi=-72;batteryPercent=85;model=lan-lepresenced;daemon=lepresenced V0.93\n"

// ... client, receiving "present", decides to check more frequently ...

[Client] -> [Server] : "aa:bb:cc:dd:ee:ff|10\n"
[Server] -> [Client] : "command accepted\n"

// ... 10 seconds pass, device is no longer detected ...

[Server] -> [Client] : "absence;rssi=unreachable;model=lan-lepresenced;daemon=lepresenced V0.93\n"

// ... client tests the connection ...

[Client] -> [Server] : "ping\n"
[Server] -> [Client] : "pong [clients=1;devices=1;min_age=5;max_age=35;beacons_hcitool=10;beacons_hcidump=10;beacons_diff=0]\n"
// Server immediately closes the TCP connection here.
```