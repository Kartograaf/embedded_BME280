#!/usr/bin/env python3
import argparse
import json
import socket
import time
import requests
import threading

from zeroconf import ServiceBrowser, Zeroconf
import paho.mqtt.client as mqtt


# ============================================================
# CONFIG
# ============================================================

SSDP_ADDR = ("239.255.255.250", 1900)

MSEARCH = "\r\n".join([
    "M-SEARCH * HTTP/1.1",
    "HOST:239.255.255.250:1900",
    'MAN:"ssdp:discover"',
    "MX:1",
    "ST:ssdp:all",
    "", ""
]).encode("utf-8")


# ============================================================
# DEVICE DISCOVERY
# ============================================================

class TasmotaListener:
    """mDNS discovery listener"""

    def __init__(self):
        self.devices = []

    def add_service(self, zeroconf, service_type, name):
        info = zeroconf.get_service_info(service_type, name)
        if info and info.addresses:
            ip = socket.inet_ntoa(info.addresses[0])
            self.devices.append(ip)

    # Required by newer zeroconf versions
    def update_service(self, *args):
        pass

    def remove_service(self, *args):
        pass


def discover_mdns(timeout=3):
    print("Discovering via mDNS...")
    zeroconf = Zeroconf()
    listener = TasmotaListener()

    ServiceBrowser(zeroconf, "_http._tcp.local.", listener)
    time.sleep(timeout)
    zeroconf.close()

    return listener.devices


def discover_udp(timeout=3):
    print("Discovering via UDP (SSDP)...")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                         socket.IPPROTO_UDP)
    sock.settimeout(timeout)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)

    sock.sendto(MSEARCH, SSDP_ADDR)

    devices = set()
    start = time.time()

    while True:
        try:
            data, addr = sock.recvfrom(65507)
            msg = data.decode(errors="ignore")

            if "Tasmota" in msg or "Sonoff" in msg:
                devices.add(addr[0])

        except socket.timeout:
            break

        if time.time() - start > timeout:
            break

    sock.close()
    return list(devices)


def verify_tasmota(ip):
    """Confirm device really is Tasmota"""
    try:
        r = requests.get(
            f"http://{ip}/cm?cmnd=Status%200",
            timeout=2
        )
        return "Status" in r.text
    except:
        return False


# ============================================================
# HTTP CONFIGURATION
# ============================================================

def send_command(ip, cmd):
    url = f"http://{ip}/cm"
    try:
        r = requests.get(url, params={"cmnd": cmd}, timeout=5)
        return r.json()
    except Exception as e:
        print(f"Command failed ({ip}):", e)
        return None


def configure_device(ip, wifi_ssid, wifi_pass, mqtt_host):
    print(f"Configuring {ip}")

    send_command(ip, f'Ssid1 "{wifi_ssid}"')
    send_command(ip, f'Password1 "{wifi_pass}"')

    send_command(ip, f'MqttHost {mqtt_host}')
    send_command(ip, 'MqttPort 1883')
    send_command(ip, 'TelePeriod 60')

    print("Configuration sent.")


# ============================================================
# MQTT VALIDATION
# ============================================================

class MQTTValidator:

    def __init__(self, host):
        self.host = host
        self.messages = []
        self.client = mqtt.Client()

        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

    def on_connect(self, client, userdata, flags, rc):
        print("MQTT connected")
        client.subscribe("tele/#")

    def on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
            self.messages.append(payload)
            print("Telemetry received:", payload)
        except:
            pass

    def run(self, duration=30):
        self.client.connect(self.host, 1883, 60)
        self.client.loop_start()
        time.sleep(duration)
        self.client.loop_stop()
        self.client.disconnect()

    # ---- validation ----
    def validate(self):
        report = {
            "messages_received": len(self.messages),
            "sensor_valid": False,
            "errors": []
        }

        for msg in self.messages:

            # detect BME/BMP sensor payload
            for key in msg.keys():
                if key.startswith("BME") or key.startswith("BMP"):
                    data = msg[key]

                    required = ["Temperature", "Humidity", "Pressure"]

                    for r in required:
                        if r not in data:
                            report["errors"].append(
                                f"Missing field {r}"
                            )
                            return report

                    report["sensor_valid"] = True
                    return report

        report["errors"].append("No sensor telemetry detected")
        return report


# ============================================================
# REPORT
# ============================================================

def generate_report(devices, validation):
    report = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "devices_found": devices,
        "validation": validation
    }

    filename = "tasmota_test_report.json"
    with open(filename, "w") as f:
        json.dump(report, f, indent=4)

    print(f"\nReport written to {filename}")


# ============================================================
# MAIN
# ============================================================

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--wifi-ssid", required=True)
    parser.add_argument("--wifi-pass", required=True)
    parser.add_argument("--mqtt-host", required=True)

    args = parser.parse_args()

    # ---- discovery ----
    devices = discover_mdns()

    if not devices:
        devices = discover_udp()

    verified = [ip for ip in devices if verify_tasmota(ip)]

    if not verified:
        print("No Tasmota devices discovered")
        return

    print("Found devices:", verified)

    # ---- configure ----
    for ip in verified:
        configure_device(ip,
                         args.wifi_ssid,
                         args.wifi_pass,
                         args.mqtt_host)

    # allow reconnect
    print("Waiting for device reconnect...")
    time.sleep(15)

    # ---- MQTT validation ----
    validator = MQTTValidator(args.mqtt_host)
    validator.run(30)

    validation = validator.validate()

    # ---- report ----
    generate_report(verified, validation)


if __name__ == "__main__":
    main()