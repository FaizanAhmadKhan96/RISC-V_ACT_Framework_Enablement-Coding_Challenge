/**
 * UART Interface
 * ===============
 *
 * Purpose:
 *   Initializes and configures a UART serial port on Linux using the termios API.
 *   Designed for communicating with a RISC-V hardware board.
 *
 * Features:
 *   - Configurable baud rate, data bits, parity, and stop bits
 *   - Transmits a test message to the target board
 *   - Receives responses using select() for non-blocking I/O with timeout
 *   - Graceful error handling for device, permission, and I/O failures
 *
 * Build:
 *   gcc -Wall -Wextra -o uart_riscv uart_riscv.c
 *
 * Run:
 *   sudo ./uart_riscv /dev/ttyUSB0 115200
 *   sudo ./uart_riscv /dev/ttyS0   9600
 *
 * Author: Faizan Ahmad Khan
 * Standard: POSIX / Linux
 */

#include <stdio.h>      /* printf, perror, fprintf                   */
#include <stdlib.h>     /* exit, EXIT_FAILURE, EXIT_SUCCESS          */
#include <string.h>     /* memset, strlen, strerror                  */
#include <unistd.h>     /* open, close, read, write                  */
#include <fcntl.h>      /* open() flags: O_RDWR, O_NOCTTY, O_NDELAY */
#include <termios.h>    /* termios struct, tcsetattr, cfsetspeed     */
#include <errno.h>      /* errno                                     */
#include <sys/select.h> /* select(), fd_set, FD_* macros             */
#include <sys/time.h>   /* struct timeval                            */

/* ─── Configuration Constants ─────────────────────────────────────────────── */

#define RX_BUFFER_SIZE   256    /* Max bytes to read in one receive call       */
#define READ_TIMEOUT_SEC   10    /* Seconds to wait for a response              */
#define READ_TIMEOUT_USEC  0    /* Microseconds component of timeout           */

/* Test message sent to the RISC-V board */
#define TEST_MESSAGE "Message sent to RISCV Board.\r\n"

/* ─── Function Declarations ────────────────────────────────────────────────── */

int  uart_open(const char *device);
int  uart_configure(int fd, int baud_rate);
int  uart_transmit(int fd, const char *message);
int  uart_receive(int fd, char *buffer, size_t buf_size);
void uart_close(int fd);
int  parse_baud_rate(int requested, speed_t *speed_out);

int main(int argc, char *argv[])
{
    /* ── 1. Parse command-line arguments ─────────────────────────────────── */

    if (argc < 3) {
        fprintf(stderr,
            "Usage: %s <device> <baud_rate>\n"
            "  Example: %s /dev/ttyUSB0 115200\n"
            "  Example: %s /dev/ttyS0   9600\n",
            argv[0], argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    const char *device    = argv[1];
    int         baud_rate = atoi(argv[2]);

    printf("[INFO] UART RISC-V ACT Validation Tool\n");
    printf("[INFO] Device   : %s\n", device);
    printf("[INFO] Baud rate: %d\n\n", baud_rate);

    /* ── 2. Open the UART device ─────────────────────────────────────────── */

    int fd = uart_open(device);
    if (fd < 0) {
        /* uart_open already printed the error; just exit */
        return EXIT_FAILURE;
    }
    printf("[OK]   Device opened successfully (fd=%d)\n", fd);

    /* ── 3. Configure UART parameters ───────────────────────────────────── */

    if (uart_configure(fd, baud_rate) < 0) {
        uart_close(fd);
        return EXIT_FAILURE;
    }
    printf("[OK]   UART configured: %d baud, 8N1 (8 data bits, no parity, 1 stop bit)\n",
           baud_rate);

    /* ── 4. Transmit the test message ───────────────────────────────────── */

    printf("[INFO] Sending test message: \"%s\"\n", TEST_MESSAGE);
    if (uart_transmit(fd, TEST_MESSAGE) < 0) {
        uart_close(fd);
        return EXIT_FAILURE;
    }
    printf("[OK]   Message transmitted successfully\n");

    /* ── 5. Receive response with timeout ───────────────────────────────── */

    char rx_buffer[RX_BUFFER_SIZE];
    memset(rx_buffer, 0, sizeof(rx_buffer));

    printf("[INFO] Waiting up to %d second(s) for response...\n", READ_TIMEOUT_SEC);
    int bytes_received = uart_receive(fd, rx_buffer, sizeof(rx_buffer) - 1);

    if (bytes_received < 0) {
        /* Error already reported inside uart_receive */
        uart_close(fd);
        return EXIT_FAILURE;
    } else if (bytes_received == 0) {
        printf("[WARN] No data received within the timeout period.\n");
        printf("[HINT] Check board power, cable, and baud rate settings.\n");
    } else {
        rx_buffer[bytes_received] = '\0'; /* Null-terminate for safe printing */
        printf("[OK]   Received %d byte(s):\n", bytes_received);
        printf("       >>> %s\n", rx_buffer);
    }

    /* ── 6. Clean up ────────────────────────────────────────────────────── */

    uart_close(fd);
    printf("[INFO] UART device closed. Done.\n");
    return EXIT_SUCCESS;
}

int uart_open(const char *device)
{
    if (device == NULL || device[0] == '\0') {
        fprintf(stderr, "[ERROR] Invalid device path (NULL or empty).\n");
        return -1;
    }

    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd < 0) {
        if (errno == ENOENT) {
            fprintf(stderr,
                "[ERROR] Device '%s' not found. Is the cable connected?\n", device);
        } else if (errno == EACCES) {
            fprintf(stderr,
                "[ERROR] Permission denied for '%s'.\n"
                "[HINT]  Run with sudo, or add your user to the 'dialout' group:\n"
                "        sudo usermod -aG dialout $USER\n", device);
        } else {
            fprintf(stderr,
                "[ERROR] Failed to open '%s': %s\n", device, strerror(errno));
        }
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "[ERROR] fcntl F_GETFL failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        fprintf(stderr, "[ERROR] fcntl F_SETFL failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int parse_baud_rate(int requested, speed_t *speed_out)
{
    switch (requested) {
        case 9600:   *speed_out = B9600;   break;
        case 19200:  *speed_out = B19200;  break;
        case 38400:  *speed_out = B38400;  break;
        case 57600:  *speed_out = B57600;  break;
        case 115200: *speed_out = B115200; break;
        case 230400: *speed_out = B230400; break;
        case 460800: *speed_out = B460800; break;
        case 921600: *speed_out = B921600; break;
        default:
            fprintf(stderr,
                "[ERROR] Unsupported baud rate: %d\n"
                "[HINT]  Supported: 9600, 19200, 38400, 57600, 115200, "
                "230400, 460800, 921600\n", requested);
            return -1;
    }
    return 0;
}

int uart_configure(int fd, int baud_rate)
{
    struct termios options;

    /* Read the current port settings into our struct */
    if (tcgetattr(fd, &options) < 0) {
        fprintf(stderr, "[ERROR] tcgetattr failed: %s\n", strerror(errno));
        return -1;
    }

    /* ── Baud Rate ────────────────────────────────────────────────────────── */

    speed_t speed;
    if (parse_baud_rate(baud_rate, &speed) < 0) {
        return -1;
    }
    cfsetispeed(&options, speed); /* Set input baud rate  */
    cfsetospeed(&options, speed); /* Set output baud rate */

    /* ── Control Modes (c_cflag) ─────────────────────────────────────────── */

    options.c_cflag |= CLOCAL; /* Ignore modem control lines (no hardware handshake) */
    options.c_cflag |= CREAD;  /* Enable the receiver                                */

    /* Data bits: 8 */
    options.c_cflag &= ~CSIZE; /* Clear current data-size bits first */
    options.c_cflag |=  CS8;   /* Then set to 8 data bits            */

    /* Parity: None */
    options.c_cflag &= ~PARENB; /* Disable parity generation/checking */
    options.c_cflag &= ~PARODD; /* (Even parity if PARENB were set)   */

    /* Stop bits: 1 */
    options.c_cflag &= ~CSTOPB; /* Clear = 1 stop bit; Set = 2 stop bits */

    /* Hardware flow control: Disabled */
    options.c_cflag &= ~CRTSCTS;

    /* ── Input Modes (c_iflag) ───────────────────────────────────────────── */

    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                       | INLCR  | IGNCR  | ICRNL  | IXON
                       | IXOFF  | IXANY);

    /* ── Output Modes (c_oflag) ──────────────────────────────────────────── */

    options.c_oflag &= ~OPOST;  /* Disable output post-processing (raw output) */
    options.c_oflag &= ~ONLCR;  /* Do not map NL to CR-NL on output            */

    /* ── Local Modes (c_lflag) ───────────────────────────────────────────── */

    options.c_lflag &= ~(ECHO | ECHOE | ECHONL | ICANON | ISIG | IEXTEN);

    /* ── Read timing (c_cc) ──────────────────────────────────────────────── */

    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0;

    /* ── Apply settings ──────────────────────────────────────────────────── */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        fprintf(stderr, "[ERROR] tcsetattr failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int uart_transmit(int fd, const char *message)
{
    if (message == NULL) {
        fprintf(stderr, "[ERROR] uart_transmit: NULL message pointer.\n");
        return -1;
    }

    size_t total  = strlen(message);
    size_t written = 0;

    /*
     * write() may return fewer bytes than requested (partial write).
     * Loop until all bytes are sent.
     */
    while (written < total) {
        ssize_t result = write(fd, message + written, total - written);

        if (result < 0) {
            if (errno == EINTR) {
                /* Interrupted by a signal — retry */
                continue;
            }
            fprintf(stderr, "[ERROR] UART write failed: %s\n", strerror(errno));
            return -1;
        }
        written += (size_t)result;
    }

    if (tcdrain(fd) < 0) {
        fprintf(stderr, "[WARN] tcdrain failed: %s\n", strerror(errno));
        /* Non-fatal — data was written to the kernel buffer */
    }

    return (int)written;
}

int uart_receive(int fd, char *buffer, size_t buf_size)
{
    if (buffer == NULL || buf_size == 0) {
        fprintf(stderr, "[ERROR] uart_receive: invalid buffer.\n");
        return -1;
    }

    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    /* Set the timeout duration */
    timeout.tv_sec  = READ_TIMEOUT_SEC;
    timeout.tv_usec = READ_TIMEOUT_USEC;

    int ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);

    if (ready < 0) {
        if (errno == EINTR) {
            fprintf(stderr, "[WARN] select() interrupted by signal.\n");
            return 0; /* Treat as timeout */
        }
        fprintf(stderr, "[ERROR] select() failed: %s\n", strerror(errno));
        return -1;
    }

    if (ready == 0) {
        /* Timeout: no data arrived within the deadline */
        return 0;
    }

    if (!FD_ISSET(fd, &read_fds)) {
        return 0; /* Shouldn't happen, but be defensive */
    }

    ssize_t bytes_read = read(fd, buffer, buf_size);

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No data right now despite select() saying ready — race condition */
            return 0;
        }
        fprintf(stderr, "[ERROR] UART read failed: %s\n", strerror(errno));
        return -1;
    }

    return (int)bytes_read;
}

void uart_close(int fd)
{
    if (fd >= 0) {
        tcflush(fd, TCIOFLUSH); /* Discard any pending bytes */
        close(fd);
    }
}
