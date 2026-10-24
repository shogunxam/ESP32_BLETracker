#!/usr/bin/env python3
"""Simulate a Loxone MiniServer UDP receiver for ESP32 BLETracker packets."""

import argparse
import socket


def parse_packet(message):
    """Parse a BLETracker status or system-information datagram."""
    tokens = message.split()
    if not tokens or not tokens[0].startswith("module="):
        return None

    packet = {"module": tokens[0].partition("=")[2]}
    if len(tokens) > 1 and tokens[1] == "alive":
        packet["type"] = "system"
        fields = tokens[2:]
    else:
        packet["type"] = "status"
        fields = tokens[1:]

    for token in fields:
        key, separator, value = token.partition("=")
        if not separator or not key or not value:
            return None
        packet[key] = value

    if packet["type"] == "status":
        device_fields = [
            key for key in packet
            if key not in {"module", "type", "rssi", "battery", "timestamp"}
        ]
        if len(device_fields) != 1 or not {"rssi", "timestamp"} <= packet.keys():
            return None
        packet["device"] = device_fields[0]
        packet["state"] = packet.pop(packet["device"])
        if packet["state"] not in {"0", "1"}:
            return None
    elif not {"uptime", "SSID", "rssi", "IP", "MAC", "timestamp"} <= packet.keys():
        return None

    return packet


def print_packet(message, address):
    packet = parse_packet(message)
    print(f"\n[{address[0]}:{address[1]}] {message}")

    if packet is None:
        print("Unrecognized BLETracker UDP packet.")
    elif packet["type"] == "status":
        state = "PRESENT" if packet["state"] == "1" else "ABSENT"
        print(
            f"Loxone input: module={packet['module']} "
            f"device={packet['device']} state={state} "
            f"rssi={packet['rssi']} dBm"
        )
        if "battery" in packet:
            print(f"Battery: {packet['battery']}%")
        print(f"Timestamp: {packet['timestamp']}")
    else:
        print(
            f"Loxone system-info input: module={packet['module']} "
            f"uptime={packet['uptime']} SSID={packet['SSID']} "
            f"rssi={packet['rssi']} dBm IP={packet['IP']} "
            f"MAC={packet['MAC']} timestamp={packet['timestamp']}"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--host",
        default="0.0.0.0",
        help="local interface to listen on (default: all interfaces)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=61883,
        help="UDP port to listen on (default: 61883)",
    )
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
        server.bind((args.host, args.port))
        print(f"Simulating Loxone UDP receiver on {args.host}:{args.port}")
        print("Press Ctrl+C to stop.")

        try:
            while True:
                data, address = server.recvfrom(2048)
                try:
                    message = data.decode("utf-8")
                except UnicodeDecodeError:
                    print(f"\n[{address[0]}:{address[1]}] Invalid UTF-8: {data.hex()}")
                    continue

                print_packet(message, address)
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
