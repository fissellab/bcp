#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#define BUFFER_SIZE 256

int configure_serial_port(int fd) {
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10; // 1 second timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return -1;
    }
    
    return 0;
}

int test_tec_communication(const char* port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        return -1; // Can't open port
    }
    
    if (configure_serial_port(fd) < 0) {
        close(fd);
        return -1;
    }
    
    // Clear any existing data
    tcflush(fd, TCIOFLUSH);
    
    // Try a simple temperature query
    const char* cmd = "$R100?\r";
    write(fd, cmd, strlen(cmd));
    
    // Wait for response
    usleep(200000); // 200ms
    
    char response[BUFFER_SIZE];
    int bytes_read = read(fd, response, BUFFER_SIZE - 1);
    
    close(fd);
    
    if (bytes_read > 0) {
        response[bytes_read] = '\0';
        printf("  SUCCESS on %s: %d bytes received: '%s'\n", port, bytes_read, response);
        return 1; // Found working port
    }
    
    return 0; // No response
}

int main() {
    printf("=== Scanning for PR59 TEC Controller ===\n\n");
    
    const char* test_ports[] = {
        "/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3", "/dev/ttyS4", "/dev/ttyS5",
        "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyUSB3",
        "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2",
        NULL
    };
    
    int found_count = 0;
    
    for (int i = 0; test_ports[i] != NULL; i++) {
        printf("Testing %s...\n", test_ports[i]);
        
        int result = test_tec_communication(test_ports[i]);
        if (result == 1) {
            printf("*** FOUND TEC CONTROLLER ON %s ***\n\n", test_ports[i]);
            found_count++;
        } else if (result == 0) {
            printf("  No response from %s\n", test_ports[i]);
        } else {
            printf("  Cannot access %s (%s)\n", test_ports[i], strerror(errno));
        }
    }
    
    if (found_count == 0) {
        printf("\n*** NO TEC CONTROLLERS FOUND ***\n");
        printf("Check:\n");
        printf("1. Power to TEC controller\n");
        printf("2. USB/Serial cable connections\n");
        printf("3. Device permissions\n");
        printf("4. Different baud rates\n");
        return 1;
    } else {
        printf("Found %d TEC controller(s)\n", found_count);
        return 0;
    }
}