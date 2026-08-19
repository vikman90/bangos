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
    "-drive", "file=build/disk.img,format=raw,index=1,media=disk",
    "-serial", f"tcp:127.0.0.1:{PORT},server=on,wait=off",
    "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
    "-netdev", "user,id=net0",
    "-device", "virtio-net-pci,netdev=net0",
    "-nographic",
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
            last_action_time = time.time()
    except socket.timeout:
        pass
    except Exception:
        break

    now = time.time()

    if state == 0 and "Select an option [1-9]:" in out_log:
        time.sleep(0.3)
        print("\n[Test Step 1/9] Spawning Standalone /bin/sysinfo via fork+execve (Option 2)...")
        sock.sendall(b"2\r\n")
        state = 1
        last_action_time = now

    elif state == 1 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from sysinfo process...")
        sock.sendall(b"\r\n")
        state = 2
        last_action_time = now

    elif state == 2 and out_log.count("Select an option [1-9]:") >= 2:
        time.sleep(0.3)
        print("\n[Test Step 2/9] Spawning Standalone /bin/bench via fork+execve (Option 3)...")
        sock.sendall(b"3\r\n")
        state = 3
        last_action_time = now

    elif state == 3 and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from bench process...")
        sock.sendall(b"\r\n")
        state = 4
        last_action_time = now

    elif state == 4 and out_log.count("Select an option [1-9]:") >= 3:
        time.sleep(0.3)
        print("\n[Test Step 3/9] Spawning Standalone /bin/calc via fork+execve (Option 1)...")
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

    elif state == 7 and out_log.count("Select an option [1-9]:") >= 4:
        time.sleep(0.3)
        print("\n[Test Step 4/9] Spawning Preemptive Multitasking /bin/tasks (Option 4)...")
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

    elif state == 10 and out_log.count("Select an option [1-9]:") >= 5:
        time.sleep(0.3)
        print("\n[Test Step 5/9] Spawning Multithreading & Synchronization /bin/threads (Option 5)...")
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

    elif state == 14 and out_log.count("Select an option [1-9]:") >= 6:
        time.sleep(0.3)
        print("\n[Test Step 6/9] Spawning Storage & ext2 Explorer /bin/disktool (Option 6)...")
        sock.sendall(b"6\r\n")
        state = 15
        last_action_time = now

    elif state == 15 and "Select an option [1-8]:" in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Running Persistence & Disk Write Test in /bin/disktool (Option 6)...")
        sock.sendall(b"6\r\n")
        state = 16
        last_action_time = now

    elif state == 16 and "SUCCESS: Disk write and readback verified" in out_log and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Dismissing pause and exiting /bin/disktool (Option 8)...")
        sock.sendall(b"\r\n8\r\n")
        state = 17
        last_action_time = now

    elif state == 17 and out_log.count("Select an option [1-9]:") >= 7:
        time.sleep(0.3)
        print("\n[Test Step 7/9] Running Userland Specification Test Suites (Option 7)...")
        sock.sendall(b"7\r\n")
        state = 18
        last_action_time = now

    elif state == 18 and "All socket specification tests evaluated to PASS!" in out_log and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Returning to init menu from test suites...")
        sock.sendall(b"\r\n")
        state = 19
        last_action_time = now

    elif state == 19 and out_log.count("Select an option [1-9]:") >= 8:
        time.sleep(0.3)
        print("\n[Test Step 8/9] Spawning Network Fetch & HTTP Diagnostic Client /bin/netfetch (Option 8)...")
        sock.sendall(b"8\r\n")
        state = 20
        last_action_time = now

    elif state == 20 and "Select an option [1-6]:" in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Running Automated Network & HTTP Test Suite in /bin/netfetch (Option 5)...")
        sock.sendall(b"5\r\n")
        state = 21
        last_action_time = now

    elif state == 21 and "All Network & HTTP/1.1 tests evaluated to PASS!" in out_log and "Press Enter to return to main menu..." in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Dismissing pause in /bin/netfetch...")
        sock.sendall(b"\r\n")
        state = 22
        last_action_time = now

    elif state == 22 and "Select an option [1-6]:" in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test] Exiting /bin/netfetch (Option 6)...")
        sock.sendall(b"6\r\n")
        state = 23
        last_action_time = now

    elif state == 23 and "Select an option [1-9]:" in out_log[len(out_log)-200:]:
        time.sleep(0.3)
        print("\n[Test Step 9/9] Requesting System Halt/Exit (Option 9)...")
        sock.sendall(b"9\r\n")
        state = 24
        last_action_time = now

    elif state == 24 and ("Shutting down BangOS" in out_log or "PID=1 exited" in out_log or "called exit(0)" in out_log):
        time.sleep(0.5)
        try:
            more = sock.recv(1024)
            if more:
                out_log += more.decode("utf-8", errors="ignore")
        except Exception:
            pass
        print("\n\n[SUCCESS] All standalone ELF executions, storage drive operations, network TCP/IP stack, ext2 VFS specs, and multitasking verified successfully!")
        sock.close()
        proc.kill()
        break

    if now - last_action_time > 45:
        print(f"\n[Test Warning] Inactivity timeout in state {state}. Proceeding to verification assertions...")
        break

sock.close()
proc.kill()

# Strip ANSI codes for verification checks
clean_log = re.sub(r'\x1b\[[0-9;]*[mGKHJ]', '', out_log)

checks = [
    ("All Ring 0 kernel self-tests evaluated to PASS!", "Ring 0 In-Kernel Unit Test Suite (PMM, VMM, KString, TarFS, Sched, ATA, ext2, Net)"),
    ("Bad File Descriptor Error Propagation (-EBADF)", "Userland Syscall Bad File Descriptor Validation (-EBADF)"),
    ("Null & Kernel Pointer Safety Checking (-EFAULT)", "Userland Syscall Pointer Bounds & Safety Checking (-EFAULT)"),
    ("Unimplemented System Call Dispatch Invariant (-ENOSYS)", "Userland Unimplemented Syscall Invariant (-ENOSYS)"),
    ("VMM 8MB Demand Paging & Zero-Fill Verification", "Userland VMM Demand Paging & Zero-Fill Verification"),
    ("VMM mprotect & Partial Hole Munmap", "Userland VMM mprotect & Split Munmap"),
    ("Process Fork + Exit Status Waitpid Propagation", "Userland Process Fork + Exit Status Waitpid Propagation"),
    ("POSIX File Open & Data Read", "Userland ext2 POSIX File Open & Data Read"),
    ("POSIX lseek() SEEK_SET & SEEK_CUR", "Userland ext2 POSIX lseek Offset Repositioning"),
    ("POSIX fstat() File Size & Mode", "Userland ext2 POSIX fstat Attribute Query"),
    ("POSIX File Creation, Persistence & Readback", "Userland ext2 File Creation & Persistence Verification"),
    ("POSIX TCP Stream Socket Creation (AF_INET, SOCK_STREAM)", "Userland POSIX Socket Creation & Allocation"),
    ("All socket specification tests evaluated to PASS!", "Userland POSIX Socket Specification Test Suite"),
    ("Adapter Type:   VirtIO Network Controller", "VirtIO Network Controller Detection"),
    ("Pinging Default Gateway (10.0.2.2)", "ICMP Echo Ping Gateway Communication"),
    ("DNS query resolved successfully!", "RFC 1035 UDP DNS Hostname Resolution"),
    ("TCP Connection ESTABLISHED with remote host!", "TCP 3-Way Handshake Connection"),
    ("HTTP/1.1 200 OK", "HTTP/1.1 Web Payload Fetch over TCP"),
    ("All Network & HTTP/1.1 tests evaluated to PASS!", "Automated Network & HTTP Diagnostics Suite"),
    ("BangOS (x86_64)", "BangOS OS header banner"),
    ("System Name:    BangOS", "uname syscall validation"),
    ("Calculated Pi:  3.141592", "FPU/SSE math benchmark"),
    ("Hypotenuse: 5.00", "Hypotenuse calculation"),
    ("System Multitasking Status: ACTIVE", "Preemptive multitasking /bin/tasks"),
    ("pong", "UART interaction during background task"),
    ("MUTEX SYNCHRONIZATION SUCCESS!", "Thread Mutex atomic synchronization"),
    ("Producer-Consumer pipeline completed", "Counting Semaphore synchronization"),
    ("Disk write and readback verified with 100% byte integrity", "Interactive /bin/disktool Storage Persistence & Readback"),
    ("Shutting down BangOS system cleanly...", "Clean process shutdown")
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
