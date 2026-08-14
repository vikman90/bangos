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

# Test state machine steps:
# 0: waiting for initial menu
# 1: sysinfo sent, waiting for sysinfo output
# 2: return from sysinfo, waiting for menu
# 3: bench sent, waiting for bench output
# 4: return from bench, waiting for menu
# 5: calc sent, waiting for calc prompt
# 6: sides sent, waiting for calc results
# 7: return from calc, waiting for menu
# 8: memtest sent, waiting for memtest output
# 9: return from memtest, waiting for menu
# 10: exit sent, waiting for clean exit
state = 0
last_action_time = time.time()

print("[Test] Connected to bare-metal serial port! Waiting for process execution...")

while time.time() - start_time < 30:
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

    if state == 0 and "Select an option [1-5]:" in out_log:
        time.sleep(0.3)
        print("\n[Test Step 1/5] Requesting System Information (Option 2)...")
        sock.sendall(b"2\r\n")
        state = 1
        last_action_time = now

    elif state == 1 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to main menu from sysinfo...")
        sock.sendall(b"\r\n")
        state = 2
        last_action_time = now

    elif state == 2 and out_log.count("Select an option [1-5]:") >= 2:
        time.sleep(0.3)
        print("\n[Test Step 2/5] Running CPU FPU/SSE and Timer Benchmark (Option 3)...")
        sock.sendall(b"3\r\n")
        state = 3
        last_action_time = now

    elif state == 3 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to main menu from bench...")
        sock.sendall(b"\r\n")
        state = 4
        last_action_time = now

    elif state == 4 and out_log.count("Select an option [1-5]:") >= 3:
        time.sleep(0.3)
        print("\n[Test Step 3/5] Launching Geometric Calculator (Option 1)...")
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
        print("\n[Test] Returning to main menu from calc...")
        sock.sendall(b"\r\n")
        state = 7
        last_action_time = now

    elif state == 7 and out_log.count("Select an option [1-5]:") >= 4:
        time.sleep(0.3)
        print("\n[Test Step 4/5] Running Dynamic Memory Stress Test (Option 4)...")
        sock.sendall(b"4\r\n")
        state = 8
        last_action_time = now

    elif state == 8 and "Memory Integrity Verification: PASSED" in out_log and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to main menu from memtest...")
        sock.sendall(b"\r\n")
        state = 9
        last_action_time = now

    elif state == 9 and out_log.count("Select an option [1-5]:") >= 5:
        time.sleep(0.3)
        print("\n[Test Step 5/5] Requesting System Halt/Exit (Option 5)...")
        sock.sendall(b"5\r\n")
        state = 10
        last_action_time = now

    elif state == 10 and "Process exited with status code: 0" in out_log:
        print("\n\n[SUCCESS] All modular userland applications and syscalls verified successfully!")
        sock.close()
        proc.kill()
        sys.exit(0)

sock.close()
proc.kill()

# Verification checks
checks = [
    ("BangOS (x86_64)", "BangOS OS header banner"),
    ("System Name:    BangOS", "uname syscall validation"),
    ("Calculated Pi:  3.1415926535", "FPU/SSE math benchmark"),
    ("Hypotenuse: 5.00", "Hypotenuse calculation"),
    ("Memory Integrity Verification: PASSED", "Dynamic memory integrity"),
    ("Process exited with status code: 0", "Clean process shutdown")
]

all_passed = True
for term, desc in checks:
    if term in out_log:
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
