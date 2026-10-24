import socket
import time

# Global configuration to emulate FHEM
PORT = 5333  # Standard ESP port for FHEM LePresence Server

# Insert the real MAC address of the beacon you want to test here
MAC_TO_TEST = "AA:BB:CC:DD:EE:FF"
SCAN_TIMEOUT = "5"  # Bluetooth scan timeout requested to the ESP


def simulate_fhem_exact_client(tracker_ip):
    """Connects, sends the complete command with 'scan', waits for data,
    closes the connection, and loops. 100% FHEM emulation.
    """
    print(
        f"🚀 Exact FHEM Simulator started against tracker {tracker_ip}:{PORT}"
    )

    while True:
        try:
            # 1. FHEM opens a NEW TCP connection at each check cycle
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(15)  # Python network safety timeout
                s.connect((tracker_ip, PORT))
                print(f"\n🔌 [FHEM] Connected to {tracker_ip}:{PORT}")

                # 2. Sends the exact command expected by the FHEM protocol
                command = f"scan {MAC_TO_TEST} {SCAN_TIMEOUT}\n"
                print(f"📤 [FHEM -> ESP]: {command.strip()}")
                s.sendall(command.encode("utf-8"))

                # 3. Reads the first response from the ESP (Command ACK)
                # The firmware immediately responds with "command accepted\n" or "command rejected\n"
                ack_data = s.recv(1024)
                if not ack_data:
                    print("🛑 The ESP closed the connection unexpectedly.")
                    continue

                ack_response = ack_data.decode("utf-8").strip()
                print(f"📥 [ESP -> FHEM]: {ack_response}")

                if "rejected" in ack_response:
                    print(
                        "⚠️ The ESP rejected the command. You need to apply the patch to the C++ firmware."
                    )
                else:
                    # 4. If accepted, FHEM stays listening until the ESP sends the presence report
                    # The firmware executes the BLE scan and calls publishTag() to send the status string
                    report_data = s.recv(1024)
                    if report_data:
                        report_response = report_data.decode("utf-8").strip()
                        print(f"📊 [ESP REPORT]: {report_response}")

                        # Parsing the status in FHEM style
                        if "present;" in report_response:
                            print("🟢 Interpreted status by FHEM: PRESENT")
                        elif "absence;" in report_response:
                            print("🔴 Interpreted status by FHEM: ABSENT")
                    else:
                        print(
                            "🛑 No report received (connection closed by ESP)."
                        )

                # 5. Exiting the 'with' block closes the socket (exactly like FHEM does)
                print("🔌 [FHEM] Connection closed cleanly.")

        except socket.timeout:
            print("❌ Error: Network timeout while waiting for the ESP.")
        except Exception as e:
            print(f"❌ Cannot connect or communicate with the ESP: {e}")

        # 6. FHEM waits for the polling interval (e.g., 10 seconds) before the next cycle
        print("⏰ Waiting before the next check...")
        time.sleep(10)


if __name__ == "__main__":
    # Insert the real IP address of your ESP32 / ESP8266
    ESP_IP_ADDRESS = "192.168.1.x"

    if ESP_IP_ADDRESS == "192.168.1.x":
        print(
            "⚠️ Please configure your real ESP IP in the ESP_IP_ADDRESS variable before running the script!"
        )
    else:
        simulate_fhem_exact_client(ESP_IP_ADDRESS)
