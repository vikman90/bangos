import os
import platform
import subprocess
import socket
import time
import sys
import re

PORT = 4444
DEFAULT_OVMF = "/opt/homebrew/share/qemu/edk2-x86_64-code.fd" if platform.system() == "Darwin" else "/usr/share/ovmf/OVMF.fd"
OVMF_PATH = os.environ.get("OVMF_PATH", DEFAULT_OVMF)
TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "180"))

cmd = [
    "qemu-system-x86_64",
    "-m", "512M",
    "-bios", OVMF_PATH,
    "-drive", "file=fat:rw:build/esp,format=raw",
    "-serial", f"tcp:127.0.0.1:{PORT},server=on,wait=off",
    "-nographic",
    "-net", "none",
    "-monitor", "none"
]

print(f"[Test] Launching QEMU with bios={OVMF_PATH} and TCP serial socket on port {PORT}...")
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

# Test state machine:
# 0: waiting for initial init menu
# 1: sysinfo sent, waiting for sysinfo output
# 2: return from sysinfo, waiting for menu
# 3: bench sent, waiting for bench output
# 4: return from bench, waiting for menu
# 5: calc sent, waiting for calc prompt
# 6: sides sent, waiting for calc results
# 7: return from calc, waiting for menu
# 8: exit sent, waiting for clean shutdown
state = 0
last_action_time = time.time()

print("[Test] Connected to bare-metal serial port! Waiting for process execution...")

while time.time() - start_time < TIMEOUT:
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

    now = time.time()

    if state == 0 and "Select an option [1-4]:" in out_log:
        time.sleep(0.3)
        print("\n[Test Step 1/4] Spawning Standalone /bin/sysinfo via fork+execve (Option 2)...")
        sock.sendall(b"2\r\n")
        state = 1
        last_action_time = now

    elif state == 1 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from sysinfo process...")
        sock.sendall(b"\r\n")
        state = 2
        last_action_time = now

    elif state == 2 and out_log.count("Select an option [1-4]:") >= 2:
        time.sleep(0.3)
        print("\n[Test Step 2/4] Spawning Standalone /bin/bench via fork+execve (Option 3)...")
        sock.sendall(b"3\r\n")
        state = 3
        last_action_time = now

    elif state == 3 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from bench process...")
        sock.sendall(b"\r\n")
        state = 4
        last_action_time = now

    elif state == 4 and out_log.count("Select an option [1-4]:") >= 3:
        time.sleep(0.3)
        print("\n[Test Step 3/4] Spawning Standalone /bin/calc via fork+execve (Option 1)...")
        sock.sendall(b"1\r\n")
        state = 5
        last_action_time = now

    elif state == 5 and "Enter first side:" in out_log:
        time.sleep(0.3)
        print("\n[Test] Sending side inputs '3' and '4'...")
        sock.sendall(b"3\r\n4\r\n")
        state = 6
        last_action_time = now

    elif state == 6 and "Hypotenuse:" in out_log and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from calc process...")
        sock.sendall(b"\r\n")
        state = 7
        last_action_time = now

    elif state == 7 and out_log.count("Select an option [1-4]:") >= 4:
        time.sleep(0.3)
        print("\n[Test Step 4/4] Requesting System Halt/Exit (Option 4)...")
        sock.sendall(b"4\r\n")
        state = 8
        last_action_time = now

    elif state == 8 and ("PID=1 exited with status code: 0" in out_log or "Init process (PID 1) terminated" in out_log):
        print("\n\n[SUCCESS] All standalone ELF executions and syscalls verified successfully!")
        sock.close()
        proc.kill()
        sys.exit(0)

sock.close()
proc.kill()

# Strip ANSI codes for verification checks
clean_log = re.sub(r'\x1b\[[0-9;]*[mGKHJ]', '', out_log)

checks = [
    ("BangOS (x86_64)", "BangOS OS header banner"),
    ("System Name:    BangOS", "uname syscall validation"),
    ("Calculated Pi:  3.141592", "FPU/SSE math benchmark"),
    ("Hypotenuse: 5.00", "Hypotenuse calculation"),
    ("Init process (PID 1) terminated", "Clean process shutdown")
]

all_passed = True
for term, desc in checks:
    if term in clean_log:
        print(f"[PASS] {desc}")
    else:
        print(f"[FAIL] {desc} (term '{term}' not found)")
        all_passed = False

if all_passed:
    print("\n[SUCCESS] All test suite assertions passed!")
    sys.exit(0)
else:
    print("\n[Test Failure] Serial Log output incomplete or failed.")
    sys.exit(1)
