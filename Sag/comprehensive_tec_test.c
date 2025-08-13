#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#define BUFFER_SIZE 1024

// Multiple baud rates to test
const int baud_rates[] = {9600, 19200, 38400, 57600, 115200};
const speed_t baud_constants[] = {B9600, B19200, B38400, B57600, B115200};
const int num_bauds = 5;

int configure_port(int fd, speed_t baud) {
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        return -1;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable reading, ignore control lines

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // No software flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    tty.c_oflag &= ~OPOST;                          // No output processing

    tty.c_cc[VTIME] = 20;    // 2 second timeout
    tty.c_cc[VMIN] = 0;      // Return immediately on any data

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return -1;
    }
    
    return 0;
}

int test_comprehensive_communication(const char* port) {
    printf("\n=== Testing %s ===\n", port);
    
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Cannot open %s: %s\n", port, strerror(errno));
        return -1;
    }
    
    // Test different baud rates
    for (int b = 0; b < num_bauds; b++) {
        printf("\nTesting %d baud...\n", baud_rates[b]);
        
        if (configure_port(fd, baud_constants[b]) < 0) {
            printf("  Failed to configure port\n");
            continue;
        }
        
        // Multiple command formats to try
        const char* test_commands[] = {
            "$R100?\r",          // Temperature query with CR
            "$R100?\r\n",        // Temperature query with CRLF
            "$R100?\n",          // Temperature query with LF
            "$?\r",              // Status query
            "?",                 // Simple query
            "*IDN?\r\n",         // Standard instrument ID
            "VER?\r\n",          // Version query
            "\r\n",              // Just line ending
            "$Q\r",              // Stop command
            "$R1?\r",            // PID Kp query
            NULL
        };
        
        for (int i = 0; test_commands[i] != NULL; i++) {
            // Clear buffers completely
            tcflush(fd, TCIOFLUSH);
            usleep(50000); // 50ms
            
            // Send command
            int cmd_len = strlen(test_commands[i]);
            printf("  Sending: ");
            for (int j = 0; j < cmd_len; j++) {
                if (test_commands[i][j] == '\r') printf("\\r");
                else if (test_commands[i][j] == '\n') printf("\\n");
                else printf("%c", test_commands[i][j]);
            }
            printf(" (%d bytes)\n", cmd_len);
            
            int written = write(fd, test_commands[i], cmd_len);
            if (written != cmd_len) {
                printf("    Write failed: %s\n", strerror(errno));
                continue;
            }
            
            // Wait for response with multiple timeouts
            for (int timeout = 0; timeout < 3; timeout++) {
                usleep(100000 + (timeout * 200000)); // 100ms, 300ms, 500ms
                
                char response[BUFFER_SIZE];
                int bytes_read = read(fd, response, BUFFER_SIZE - 1);
                
                if (bytes_read > 0) {
                    response[bytes_read] = '\0';
                    printf("    *** SUCCESS *** Got %d bytes: '", bytes_read);
                    for (int k = 0; k < bytes_read; k++) {
                        if (response[k] == '\r') printf("\\r");
                        else if (response[k] == '\n') printf("\\n");
                        else if (response[k] >= 32 && response[k] <= 126) printf("%c", response[k]);
                        else printf("\\x%02X", (unsigned char)response[k]);
                    }
                    printf("'\n");
                    printf("    *** FOUND TEC ON %s at %d baud ***\n", port, baud_rates[b]);
                    close(fd);
                    return 1;
                }
            }
            printf("    No response\n");
        }
    }
    
    close(fd);
    return 0;
}

int main() {
    printf("=== COMPREHENSIVE PR59 TEC Controller Detection ===\n");
    printf("Testing powered-on TEC controller...\n");
    
    // Focus on most likely ports first
    const char* priority_ports[] = {
        "/dev/tec-controller",  // Symlink
        "/dev/ttyS0",          // Most common
        "/dev/ttyS1",          // Second most common
        "/dev/ttyUSB0",        // USB adapter
        "/dev/ttyUSB1",
        "/dev/ttyACM0",        // USB CDC
        "/dev/ttyACM1",
        NULL
    };
    
    int found = 0;
    
    for (int i = 0; priority_ports[i] != NULL; i++) {
        int result = test_comprehensive_communication(priority_ports[i]);
        if (result == 1) {
            found = 1;
            printf("\n*** COMMUNICATION ESTABLISHED ***\n");
            printf("Update your symlink: sudo ln -sf %s /dev/tec-controller\n", 
                   priority_ports[i]);
            break;
        }
    }
    
    if (!found) {
        printf("\n*** NO RESPONSE FROM TEC CONTROLLER ***\n");
        printf("\nTroubleshooting steps:\n");
        printf("1. Check power LED on TEC controller\n");
        printf("2. Check all cable connections\n");
        printf("3. Try different USB/Serial cable\n");
        printf("4. Check if TEC controller has a different interface (Ethernet, etc.)\n");
        printf("5. Power cycle the TEC controller\n");
        printf("6. Check TEC controller manual for communication settings\n");
        
        // Show what ports are available
        printf("\nAvailable serial ports:\n");
        system("ls -la /dev/tty{S,USB,ACM}* 2>/dev/null | head -10");
        
        return 1;
    }
    
    return 0;
}