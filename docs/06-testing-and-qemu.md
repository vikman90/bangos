# 06 - Automated Testing and QEMU Simulation

BangOS includes an automated testing harness written in Python (`scripts/test_runner.py`) that enables headless kernel execution and verification in local environments and GitHub Continuous Integration (CI).

---

## 🖥️ QEMU TCP Serial Socket Setup

To send interactive inputs (`3\r\n` and `4\r\n`) to userland applications and capture output produced by the UART driver, QEMU redirects serial COM1 to a TCP socket on `127.0.0.1:4444`:

```bash
qemu-system-x86_64 \
    -m 512M \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:build/esp,format=raw \
    -serial tcp:127.0.0.1:4444,server=on,wait=off \
    -nographic \
    -net none \
    -monitor none
```

---

## 🐍 Test Runner Architecture (`scripts/test_runner.py`)

1. **Async QEMU Launch**: Spawns QEMU in the background.
2. **TCP Socket Connection**: Connects a TCP socket to `127.0.0.1:4444` and disables Nagle's algorithm (`socket.TCP_NODELAY`).
3. **UART Input Injection**: Detects ELF execution launch in output logs and transmits `3\r\n4\r\n` over the serial socket.
4. **Result Verification**: Analyzes output stream in real-time. Finding `"Hypotenuse: 5.00"` completes the test successfully (`exit code 0`). Timing out results in a test failure (`exit code 1`).

---

## 🤖 Makefile Integration

Run the automated test suite at any time via:

```bash
make test
```
