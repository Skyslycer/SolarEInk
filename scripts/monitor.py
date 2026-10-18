"""Follow this ESP32 across native USB disconnects without resetting it."""

import argparse
import time

import serial
from serial.tools import list_ports


def usb_identity(value):
    return value.replace(":", "").replace("-", "").upper()


def find_port(serial_number):
    for port in list_ports.comports():
        if (port.vid, port.pid) != (0x303A, 0x1001):
            continue
        if usb_identity(port.serial_number or "") == usb_identity(serial_number):
            return port.device
    return None


def emit(message):
    print(time.strftime("%H:%M:%S"), message, flush=True)


class BootLogSerial(serial.Serial):
    def _reset_input_buffer(self):
        # POSIX pyserial normally discards buffered bytes when opening a port.
        # Retain them: the board may already have printed its wake diagnostics.
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-number", default="50:78:7D:17:DB:68",
                        help="USB serial number of the ESP32 to follow")
    parser.add_argument("--cycles", type=int, default=0,
                        help="Exit after this many complete cycles (0: forever)")
    parser.add_argument("--duration", type=float, default=0,
                        help="Stop after this many seconds (0: forever)")
    args = parser.parse_args()
    if args.cycles < 0 or args.duration < 0:
        parser.error("cycles and duration must not be negative")

    deadline = time.monotonic() + args.duration if args.duration else float("inf")
    connection = None
    pending = b""
    cycles = 0
    last_error = None
    emit(f"Waiting for ESP32 {args.serial_number}; Ctrl+C to stop.")
    try:
        while time.monotonic() < deadline:
            try:
                if connection is None:
                    port = find_port(args.serial_number)
                    if port is None:
                        time.sleep(0.05)
                        continue
                    connection = BootLogSerial(port=None, baudrate=115200,
                                               timeout=0.1, exclusive=True)
                    # Set these BEFORE opening. Clearing them individually can
                    # trigger USB_UART_CHIP_RESET and destroy the RTC counter.
                    connection.dtr = True
                    connection.rts = True
                    connection.port = port
                    connection.open()
                    last_error = None
                    emit(f"Connected to {port}")

                pending += connection.read(connection.in_waiting or 1)
                while b"\n" in pending:
                    raw, pending = pending.split(b"\n", 1)
                    line = raw.decode("utf-8", errors="replace").rstrip("\r")
                    emit(line)
                    if "Wi-Fi off. Total awake" in line:
                        cycles += 1
                if args.cycles and cycles >= args.cycles:
                    break
            except (serial.SerialException, OSError) as error:
                was_connected = connection is not None and connection.is_open
                if connection is not None:
                    connection.close()
                    connection = None
                if pending:
                    emit(pending.decode("utf-8", errors="replace"))
                    pending = b""
                if was_connected:
                    emit("USB disconnected; waiting for next wake...")
                elif str(error) != last_error:
                    emit(f"Cannot open port: {error}. Retrying; close other monitors.")
                    last_error = str(error)
                time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        if connection is not None:
            connection.close()
        emit(f"Monitor stopped; complete cycles captured: {cycles}")
    return 1 if args.cycles and cycles < args.cycles else 0


if __name__ == "__main__":
    raise SystemExit(main())
