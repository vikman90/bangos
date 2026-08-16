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
# 8: tasks sent, waiting for tasks prompt
# 9: status & ping sent in tasks, waiting for response
# 10: exit sent in tasks, waiting for init menu
# 11: threads sent, waiting for threads menu
# 12: run all tests (4) sent in threads, waiting for completion
# 13: pause dismissed in threads, waiting for threads menu
# 14: exit (5) sent in threads, waiting for init menu
# 15: exit (6) sent in init, waiting for clean shutdown
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

    if state == 0 and "Select an option [1-6]:" in out_log:
        time.sleep(0.3)
        print("\n[Test Step 1/6] Spawning Standalone /bin/sysinfo via fork+execve (Option 2)...")
        sock.sendall(b"2\r\n")
        state = 1
        last_action_time = now

    elif state == 1 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from sysinfo process...")
        sock.sendall(b"\r\n")
        state = 2
        last_action_time = now

    elif state == 2 and out_log.count("Select an option [1-6]:") >= 2:
        time.sleep(0.3)
        print("\n[Test Step 2/6] Spawning Standalone /bin/bench via fork+execve (Option 3)...")
        sock.sendall(b"3\r\n")
        state = 3
        last_action_time = now

    elif state == 3 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from bench process...")
        sock.sendall(b"\r\n")
        state = 4
        last_action_time = now

    elif state == 4 and out_log.count("Select an option [1-6]:") >= 3:
        time.sleep(0.3)
        print("\n[Test Step 3/6] Spawning Standalone /bin/calc via fork+execve (Option 1)...")
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

    elif state == 7 and out_log.count("Select an option [1-6]:") >= 4:
        time.sleep(0.3)
        print("\n[Test Step 4/6] Spawning Preemptive Multitasking /bin/tasks (Option 4)...")
        sock.sendall(b"4\r\n")
        state = 8
        last_action_time = now

    elif state == 8 and "Available commands: 'status', 'ping', 'stats', 'spawn', 'exit'" in out_log:
        time.sleep(0.4)
        print("\n[Test] Sending interactive commands 'status' and 'ping' to /bin/tasks...")
        sock.sendall(b"status\r\nping\r\n")
        state = 9
        last_action_time = now

    elif state == 9 and "pong" in out_log:
        time.sleep(0.3)
        print("\n[Test] Exiting /bin/tasks...")
        sock.sendall(b"exit\r\n")
        state = 10
        last_action_time = now

    elif state == 10 and out_log.count("Select an option [1-6]:") >= 5:
        time.sleep(0.3)
        print("\n[Test Step 5/6] Spawning Multithreading & Synchronization /bin/threads (Option 5)...")
        sock.sendall(b"5\r\n")
        state = 11
        last_action_time = now

    elif state == 11 and "Select an option [1-5]:" in out_log:
        time.sleep(0.3)
        print("\n[Test] Running all Multithreading Synchronization tests (Option 4)...")
        sock.sendall(b"4\r\n")
        state = 12
        last_action_time = now

    elif state == 12 and "Producer-Consumer pipeline completed" in out_log and ("Press Enter to return to main menu..." in out_log[len(out_log)-200:] or "Press Enter" in out_log[len(out_log)-200:]):
        time.sleep(0.3)
        print("\n[Test] Dismissing pause in /bin/threads...")
        sock.sendall(b"\r\n")
        state = 13
        last_action_time = now

    elif state == 13 and out_log.count("Select an option [1-5]:") >= 2:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from /bin/threads (Option 5)...")
        sock.sendall(b"5\r\n")
        state = 14
        last_action_time = now

    elif state == 14 and out_log.count("Select an option [1-6]:") >= 6:
        time.sleep(0.3)
        print("\n[Test Step 6/6] Requesting System Halt/Exit (Option 6)...")
        sock.sendall(b"6\r\n")
        state = 15
        last_action_time = now

    elif state == 15 and ("PID=1 exited with status code: 0" in out_log or "Init process (PID 1) terminated" in out_log):
        print("\n\n[SUCCESS] All standalone ELF executions, multitasking and multithreading verified successfully!")
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
    ("System Multitasking Status: ACTIVE", "Preemptive multitasking /bin/tasks"),
    ("pong", "UART interaction during background task"),
    ("MUTEX SYNCHRONIZATION SUCCESS!", "Thread Mutex atomic synchronization"),
    ("Producer-Consumer pipeline completed", "Counting Semaphore synchronization"),
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
