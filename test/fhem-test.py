#!/usr/bin/env python3
"""
Mock Collectord Simulator (v2 - Interactive console for ping/now/stop)
Simulates the behavior of the FHEM 'collectord' client to test servers compatible with 'lepresenced'.
Replicates the state machine, automatic reconnection, and dynamic timeout adaptation.
"""

import socket
import threading
import queue
import time
import logging
import json
import os
import sys

# Configure logging to mimic the original daemon's output
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [%(threadName)s] - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

class DeviceMonitor(threading.Thread):
    """Thread that manages the connection and state logic for a single MAC device."""
    
    def __init__(self, name, host, port, mac, presence_timeout, absence_timeout):
        super().__init__(name=f"Monitor-{name}")
        self.host = host
        self.port = port
        self.mac = mac.lower().strip() # Normalize to lowercase as the server does
        self.presence_timeout = int(presence_timeout)
        self.absence_timeout = int(absence_timeout)
        
        # Initial state
        self.current_timeout = self.absence_timeout
        self.state = "absence"
        
        # Queue to send manual commands (e.g., 'now') to the thread
        self.cmd_queue = queue.Queue()
        self.running = True
        
        # Buffer to handle partial socket reads
        self._buffer = ""

    def run(self):
        """Main thread loop with reconnection logic."""
        while self.running:
            try:
                self._connect_and_monitor()
            except Exception as e:
                logging.error(f"Unexpected error in monitoring loop: {e}")
            
            if self.running:
                logging.warning(f"Reconnecting to {self.host}:{self.port} in 5 seconds...")
                time.sleep(5)

    def _connect_and_monitor(self):
        """Handles the single socket connection session."""
        logging.info(f"Connecting to {self.host}:{self.port} for MAC {self.mac}")
        sock = None
        try:
            sock = socket.create_connection((self.host, self.port), timeout=10)
            sock.settimeout(30) # Read timeout to prevent infinite blocking
            logging.info("Connected successfully.")
            
            # Send the initial registration command
            self._send(sock, f"{self.mac}|{self.current_timeout}")
            
            while self.running:
                # 1. Check if there are commands in the queue (e.g., 'now' or 'stop')
                try:
                    cmd = self.cmd_queue.get_nowait()
                    if cmd == "stop":
                        logging.info("Sending 'stop' command to server.")
                        self._send(sock, "stop")
                        # We don't kill the thread, the server might ignore it or close the connection.
                    elif cmd == "now":
                        logging.info("Sending forced 'now' command.")
                        self._send(sock, "now")
                    elif cmd == "ping":
                        logging.info("Sending 'ping' command (server will close connection after reply).")
                        self._send(sock, "ping")
                except queue.Empty:
                    pass

                # 2. Read data from the socket
                try:
                    data = sock.recv(1024)
                    if not data:
                        logging.info("Connection closed by server (EOF).")
                        break
                    
                    self._buffer += data.decode('utf-8', errors='ignore')
                    
                    # Process all complete lines in the buffer
                    while '\n' in self._buffer:
                        line, self._buffer = self._buffer.split('\n', 1)
                        line = line.strip('\r').strip()
                        if line:
                            self._handle_response(sock, line)
                            
                except socket.timeout:
                    # Timeout is normal, just used to unblock the loop and check the command queue
                    continue
                except ConnectionResetError:
                    logging.warning("Connection reset by server.")
                    break

        except ConnectionRefusedError:
            logging.warning(f"Connection refused by {self.host}:{self.port}")
            time.sleep(5)
        except OSError as e:
            logging.error(f"Network error: {e}")
            time.sleep(5)
        finally:
            if sock:
                sock.close()
                logging.info("Socket closed.")

    def _send(self, sock, msg):
        """Sends a message ensuring the newline terminator."""
        logging.debug(f"SEND -> '{msg}'")
        try:
            sock.sendall((msg + "\n").encode('utf-8'))
        except BrokenPipeError:
            logging.warning("Broken pipe during send.")

    def _handle_response(self, sock, response):
        """State machine that processes server responses."""
        logging.info(f"RECV <- '{response}'")
        resp_lower = response.lower()
        
        if resp_lower.startswith("command accepted"):
            pass # Expected behavior, no action needed
            
        elif resp_lower.startswith("pong"):
            # The server replied to ping. Note: the server will close the connection right after.
            logging.info("✅ PING SUCCESSFUL! Server replied correctly.")
            # We do nothing here; the read loop will get an EOF on the next iteration 
            # and handle the automatic reconnection.
            
        elif resp_lower.startswith("present"):
            if self.state == "absence":
                logging.info(f"*** STATE CHANGE: ABSENCE -> PRESENT for {self.mac} ***")
                self.state = "present"
                # Adapt timeout: now we want more frequent checks
                self.current_timeout = self.presence_timeout
                logging.info(f"Updating timeout to {self.current_timeout}s (presence)")
                self._send(sock, f"{self.mac}|{self.current_timeout}")
                
        elif resp_lower.startswith("absence"):
            if self.state == "present":
                logging.info(f"*** STATE CHANGE: PRESENT -> ABSENCE for {self.mac} ***")
                self.state = "absence"
                # Adapt timeout: now we can relax and check less frequently
                self.current_timeout = self.absence_timeout
                logging.info(f"Updating timeout to {self.current_timeout}s (absence)")
                self._send(sock, f"{self.mac}|{self.current_timeout}")
                
        elif resp_lower.startswith("no command running"):
            pass # Valid response to 'stop' or 'now' if nothing is running
            
        elif resp_lower.startswith("command rejected"):
            logging.error("Server REJECTED the command (unknown or malformed).")
            
        elif resp_lower.startswith("error"):
            logging.error("Server reported an internal error.")
            
        else:
            logging.warning(f"Unrecognized response from server: '{response}'")

    def send_command(self, cmd):
        """Public method to send commands to the thread from the outside."""
        self.cmd_queue.put(cmd)


def load_config(config_path):
    """Loads configuration from a JSON file."""
    if not os.path.exists(config_path):
        # Create a default config if it doesn't exist
        default_config = {
            "targets": [
                {
                    "name": "Test_Room",
                    "host": "127.0.0.1",
                    "port": 5333,
                    "mac": "AA:BB:CC:DD:EE:FF",
                    "presence_timeout": 10,
                    "absence_timeout": 30
                }
            ]
        }
        with open(config_path, 'w') as f:
            json.dump(default_config, f, indent=4)
        logging.info(f"Created example configuration file: {config_path}")
        logging.info("Please edit it with your server details and restart the script.")
        sys.exit(0)
        
    with open(config_path, 'r') as f:
        return json.load(f)


def main():
    config_file = "config.json" if len(sys.argv) <= 1 else sys.argv[1]
    logging.info(f"Starting Mock Collectord with config: {config_file}")
    config = load_config(config_file)
    
    threads = []
    
    # Start a thread for each configured target
    for target in config.get("targets", []):
        monitor = DeviceMonitor(
            name=target["name"],
            host=target["host"],
            port=target["port"],
            mac=target["mac"],
            presence_timeout=target["presence_timeout"],
            absence_timeout=target["absence_timeout"]
        )
        monitor.start()
        threads.append(monitor)
        logging.info(f"Started monitoring for {target['name']} ({target['mac']})")

    print("\n" + "="*50)
    print("INTERACTIVE CONSOLE STARTED")
    print("Available commands:")
    print("  ping  -> Test server 'pong' response (causes reconnection)")
    print("  now   -> Force an immediate check")
    print("  stop  -> Send the stop command")
    print("  quit  -> Exit the program")
    print("="*50 + "\n")
    
    try:
        # Keep the main thread alive
        while True:
            try:
                cmd = input("mock_collectord> ").strip().lower()
                if not cmd:
                    continue
                if cmd in ['quit', 'exit', 'q']:
                    break
                elif cmd in ['ping', 'now', 'stop']:
                    logging.info(f"Sending command '{cmd}' to all monitors...")
                    for t in threads:
                        t.send_command(cmd)
                else:
                    print("Unrecognized command. Use: ping, now, stop, quit")
            except EOFError:
                break
                
    except KeyboardInterrupt:
        logging.info("Interrupt signal received (Ctrl+C).")
        
    logging.info("Shutting down...")
    for t in threads:
        t.running = False
        t.join(timeout=2)
    logging.info("Shutdown complete.")


if __name__ == "__main__":
    main()