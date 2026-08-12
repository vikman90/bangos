import subprocess
import socket
import time
import sys

PORT = 4444

cmd = [
    "qemu-system-x86_64",
    "-m", "512M",
    "-bios", "/usr/share/ovmf/OVMF.fd",
    "-drive", "file=fat:rw:build/esp,format=raw",
    "-serial", f"tcp:127.0.0.1:{PORT},server=on,wait=off",
    "-nographic",
    "-net", "none",
    "-monitor", "none"
]

print("[Test] Launching QEMU with TCP serial socket on port 4444...")
proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

sock = None
for _ in range(40):
    try:
        sock = socket.create_connection(("127.0.0.1", PORT), timeout=1.0)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        break
    except Exception:
        time.sleep(0.2)

if not sock:
    print("[Error] Could not connect to QEMU TCP serial socket.")
    proc.kill()
    sys.exit(1)

sock.settimeout(0.2)
out_log = ""
start_time = time.time()
sent_inputs = False

print("[Test] Connected to bare-metal serial port! Waiting for process execution...")

while time.time() - start_time < 12:
    try:
        data = sock.recv(1024)
        if data:
            text = data.decode("utf-8", errors="ignore")
            sys.stdout.write(text)
            sys.stdout.flush()
            out_log += text
    except socket.timeout:
        pass
    except Exception:
        break

    if "Launching ELF process execution" in out_log and not sent_inputs:
        time.sleep(0.5)
        print("\n[Test] Sending inputs '3\\r\\n' and '4\\r\\n' to serial UART FIFO...")
        sock.sendall(b"3\r\n4\r\n")
        sent_inputs = True

    if "5.00" in out_log:
        print("\n[SUCCESS] Bare-metal hypotenuse calculation result 5.00 verified!")
        sock.close()
        proc.kill()
        sys.exit(0)

sock.close()
proc.kill()

if "5.00" in out_log:
    print("\n[SUCCESS] Bare-metal hypotenuse calculation result 5.00 verified!")
    sys.exit(0)
else:
    print("\n[Test Failure] Received Serial Log:\n" + out_log)
    sys.exit(1)
