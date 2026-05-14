# RISC-V ACT Framework Enablement and M-Mode Firmware Validation on Hardware Board - Coding Challenge

This repository contains a Linux system programming utility written in C for validating UART communication on RISC-V hardware boards

The program demonstrates low-level serial communication using the Linux `termios` API. It configures a UART interface, transmits a validation message, waits for a response using a timeout-based mechanism, and prints received data back to the terminal.

The implementation focuses heavily on robust Linux system programming practices, including:

- Non-blocking I/O
- Proper UART configuration
- Retry-safe transmission
- Error handling

To make development and testing easier without physical hardware, the tool was also validated using virtual serial ports created with `socat`.


---

## Files

| File           | Description                                |
|----------------|--------------------------------------------|
| `uart_riscv.c` | Main C source — all UART logic             |
| `Makefile`     | Build script                               |
| `README.md`    | This file                                  |

---

## Build

```bash
# Using make
make

```

---

## Run

```bash
# USB-to-serial adapter at 115200 baud (most RISC-V dev boards)
sudo ./uart_riscv /dev/ttyUSB0 115200

# Built-in serial port at 9600 baud
sudo ./uart_riscv /dev/ttyS0 9600

# Other common baud rates
sudo ./uart_riscv /dev/ttyUSB0 9600
sudo ./uart_riscv /dev/ttyUSB0 57600
sudo ./uart_riscv /dev/ttyUSB0 230400
```

---

## Supported Baud Rates

`9600`, `19200`, `38400`, `57600`, `115200`, `230400`, `460800`, `921600`

---

## UART Configuration Applied

| Parameter    | Value         |
|-------------|---------------|
| Baud rate   | User-supplied |
| Data bits   | 8             |
| Parity      | None          |
| Stop bits   | 1             |
| Flow ctrl   | None          |
| Mode        | Raw (binary)  |

---

## How It Works

1. **Open** — Opens the device file with `O_RDWR | O_NOCTTY | O_NDELAY`
2. **Configure** — Applies termios settings (8N1, raw mode, no flow control)
3. **Transmit** — Sends `RISCV_ACT_PING\r\n` with retry on partial writes
4. **Receive** — Uses `select()` to wait up to 5 seconds for a response
5. **Print** — Displays received bytes to stdout
6. **Close** — Flushes buffers and closes the file descriptor

---

## Error Handling

| Error Condition         | Handling                                              |
|------------------------|-------------------------------------------------------|
| Device not found       | Clear message + cable-check hint                      |
| Permission denied      | Explains `dialout` group fix                          |
| Unsupported baud rate  | Lists valid options                                   |
| Partial write          | Retries in a loop until all bytes sent                |
| Signal interruption    | EINTR handled in both write loop and select()         |
| Read timeout           | Reports timeout gracefully, suggests troubleshooting  |
| Read/write I/O error   | `strerror(errno)` message with [ERROR] prefix         |

---

## Testing Without Hardware

Use a **loopback connector** (short TX and RX pins together) or a virtual
serial pair created with `socat`:

```bash
# Create a virtual serial pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# socat will print: /dev/pts/3 <-> /dev/pts/4

# In terminal 1 — run the tool on one end
sudo ./uart_riscv /dev/pts/3 115200

# In terminal 2 — echo data back on the other end
cat /dev/pts/4          # to see what was sent
echo "ACT_PONG" > /dev/pts/4   # to send a response back
```

---

### UART DEMO with virtual serial ports




---