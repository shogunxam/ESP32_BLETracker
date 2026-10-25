#!/usr/bin/env python3
"""Integration test for MQTT messages published by an ESP32 BLETracker."""

import argparse
import json
import logging
import queue
import re
import sys
import uuid
from pathlib import Path

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None


LOGGER = logging.getLogger("mqtt-test")
DEFAULT_CONFIG = Path(__file__).with_name("mqtt-test-config.json")


def normalize_device_id(value):
    if not isinstance(value, str):
        raise ValueError("device IDs must be strings")
    device_id = value.replace(":", "").replace("-", "").upper()
    if device_id and (len(device_id) not in (12, 40) or not re.fullmatch(r"[0-9A-F]+", device_id)):
        raise ValueError(
            f"Invalid device ID {value!r}: expected a 6-byte MAC or 20-byte beacon ID in hexadecimal"
        )
    return device_id


def load_config(config_path):
    with config_path.open(encoding="utf-8") as config_file:
        config = json.load(config_file)

    required = ("broker_host", "location", "gateway")
    missing = [key for key in required if not config.get(key)]
    if missing:
        raise ValueError(f"Missing required configuration value(s): {', '.join(missing)}")

    port = config.get("broker_port", 1883)
    if not isinstance(port, int) or not 1 <= port <= 65535:
        raise ValueError("broker_port must be an integer from 1 to 65535")

    for key in ("location", "gateway"):
        if any(character in config[key] for character in ("+", "#", "/")):
            raise ValueError(f"{key} must be a single MQTT topic level")
    if config["gateway"].upper() in ("ESP32-SET-YOUR-GATEWAY", "CHANGE-TO-TRACKER-HOSTNAME"):
        raise ValueError(
            "gateway is still an example value; set it to the exact tracker hostname "
            "shown on the web configuration page"
        )

    device_ids = config.get("device_ids", [])
    if not isinstance(device_ids, list):
        raise ValueError("device_ids must be a JSON array of device IDs")
    device_ids = [normalize_device_id(device_id) for device_id in device_ids]

    legacy_device_id = config.get("device_id", "")
    if legacy_device_id:
        device_ids.append(normalize_device_id(legacy_device_id))

    config["device_ids"] = list(dict.fromkeys(device_id for device_id in device_ids if device_id))
    config["topic_prefix"] = f"{config['location']}/{config['gateway']}"
    config["broker_port"] = port
    return config


def decode_json(payload, topic):
    try:
        value = json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"{topic} did not contain valid JSON: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"{topic} JSON payload must be an object")
    return value


def validate_messages(config, messages):
    prefix = config["topic_prefix"]
    checks = []
    failures = []

    def check(name, topic, validate):
        payload = messages.get(topic)
        if payload is None:
            failures.append(f"Missing MQTT message on {topic}")
            return
        try:
            validate(payload)
            checks.append(name)
        except ValueError as error:
            failures.append(str(error))

    def validate_availability(payload):
        if payload != "online":
            raise ValueError(f"{prefix}/LWT expected 'online', received {payload!r}")

    check("availability", f"{prefix}/LWT", validate_availability)

    def validate_sysinfo(payload):
        info = decode_json(payload, f"{prefix}/sysinfo")
        required = ("uptime", "version", "SSID", "IP")
        if any(key not in info for key in required):
            raise ValueError(f"{prefix}/sysinfo is missing one of: {', '.join(required)}")
        if not info["IP"]:
            raise ValueError(f"{prefix}/sysinfo contains an empty IP address")

    check("system information", f"{prefix}/sysinfo", validate_sysinfo)

    def validate_status(payload):
        status = decode_json(payload, f"{prefix}/status")
        if status.get("status") != "online":
            raise ValueError(f"{prefix}/status expected status 'online'")
        if not isinstance(status.get("devices_count"), int):
            raise ValueError(f"{prefix}/status is missing an integer devices_count")

    check("tracker status", f"{prefix}/status", validate_status)

    def validate_devices(payload):
        device_list = decode_json(payload, f"{prefix}/devices")
        devices = device_list.get("devices")
        if not isinstance(devices, list) or device_list.get("count") != len(devices):
            raise ValueError(f"{prefix}/devices has an invalid count or devices list")
        for device in devices:
            if not isinstance(device, dict) or not {"address", "state", "rssi", "last_seen"} <= device.keys():
                raise ValueError(f"{prefix}/devices contains an invalid device entry")

    check("device list", f"{prefix}/devices", validate_devices)

    for device_id in config["device_ids"]:
        device_topic = f"{prefix}/{device_id}"
        device_check_name = f"device state ({device_id})"

        def validate_device(payload):
            state = decode_json(payload, device_topic)
            if state.get("state") not in ("on", "off") or not isinstance(state.get("rssi"), int):
                raise ValueError(f"{device_topic} has an invalid state or RSSI")
            if "battery" in state and not isinstance(state["battery"], int):
                raise ValueError(f"{device_topic} has an invalid battery value")

        check(device_check_name, device_topic, validate_device)

        discovery_topic = (
            f"homeassistant/device_tracker/{config['gateway']}_device_{device_id}/config"
        )

        def validate_discovery(payload):
            discovery = decode_json(payload, discovery_topic)
            if discovery.get("state_topic") != device_topic:
                raise ValueError(f"{discovery_topic} has an unexpected state_topic")
            if discovery.get("availability_topic") != f"{prefix}/LWT":
                raise ValueError(f"{discovery_topic} has an unexpected availability_topic")

        check(f"Home Assistant device discovery ({device_id})", discovery_topic, validate_discovery)

    return checks, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "config",
        nargs="?",
        type=Path,
        default=DEFAULT_CONFIG,
        help=f"MQTT test configuration JSON (default: {DEFAULT_CONFIG})",
    )
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
    if mqtt is None:
        LOGGER.error("Missing dependency: install it with 'python3 -m pip install paho-mqtt'")
        return 2

    try:
        config = load_config(args.config)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        LOGGER.error("Invalid test configuration: %s", error)
        return 2

    incoming = queue.Queue()
    connection_errors = queue.Queue()
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"bletracker-mqtt-test-{uuid.uuid4().hex[:8]}",
    )
    if config.get("username") or config.get("password"):
        client.username_pw_set(config.get("username", ""), config.get("password", ""))

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code != 0:
            connection_errors.put(f"MQTT broker rejected connection: {reason_code}")
            return
        LOGGER.info("Connected to MQTT broker %s:%s", config["broker_host"], config["broker_port"])
        subscriptions = [
            (f"{config['topic_prefix']}/#", 0),
            (f"homeassistant/sensor/{config['gateway']}_status/config", 0),
            (f"homeassistant/sensor/{config['gateway']}_devices/config", 0),
        ]
        for device_id in config["device_ids"]:
            subscriptions.append((
                f"homeassistant/device_tracker/{config['gateway']}_device_"
                f"{device_id}/config",
                0,
            ))
        result, _ = client.subscribe(subscriptions)
        if result != mqtt.MQTT_ERR_SUCCESS:
            connection_errors.put(f"Could not subscribe to MQTT topics (error {result})")
        else:
            LOGGER.info("Subscribed to: %s", ", ".join(topic for topic, _ in subscriptions))

    def on_subscribe(client, userdata, mid, reason_code_list, properties):
        failed = [str(reason_code) for reason_code in reason_code_list if reason_code.is_failure]
        if failed:
            connection_errors.put(f"Broker rejected MQTT subscription(s): {', '.join(failed)}")
        else:
            LOGGER.info(
                "MQTT subscriptions accepted; restart the tracker to capture startup messages"
            )

    def on_message(client, userdata, message):
        try:
            payload = message.payload.decode("utf-8")
        except UnicodeDecodeError:
            payload = ""
        LOGGER.info("Received %s: %s", message.topic, payload)
        incoming.put((message.topic, payload))

    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_message = on_message

    try:
        client.connect(config["broker_host"], config["broker_port"], keepalive=30)
        client.loop_start()
        LOGGER.info("Waiting for tracker messages under %s/#", config["topic_prefix"])
        messages = {}
        reported_checks = set()
        reported_failures = set()

        while True:
            try:
                error = connection_errors.get_nowait()
                LOGGER.error("%s", error)
                return 1
            except queue.Empty:
                pass

            try:
                topic, payload = incoming.get(timeout=0.25)
                messages[topic] = payload
            except queue.Empty:
                continue

            checks, failures = validate_messages(config, messages)
            for name in checks:
                if name not in reported_checks:
                    LOGGER.info("PASS: %s", name)
                    reported_checks.add(name)
            for failure in failures:
                if not failure.startswith("Missing MQTT message on ") and failure not in reported_failures:
                    LOGGER.error("FAIL: %s", failure)
                    reported_failures.add(failure)
    except KeyboardInterrupt:
        LOGGER.info("Test stopped by user")
        checks, failures = validate_messages(config, messages)
        for name in checks:
            LOGGER.info("PASS: %s", name)
        missing = [failure for failure in failures if failure.startswith("Missing MQTT message on ")]
        if missing:
            LOGGER.warning("Some expected topics were not observed:")
            for failure in missing:
                LOGGER.warning("  %s", failure[len("Missing MQTT message on "):])
        failed = [failure for failure in failures if failure not in missing]
        for failure in failed:
            LOGGER.error("FAIL: %s", failure)
        return 0
    except OSError as error:
        LOGGER.error("Could not connect to MQTT broker: %s", error)
        return 1
    finally:
        if client.is_connected():
            client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    sys.exit(main())
