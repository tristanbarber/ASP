#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main() {
    int fd = open("/dev/tux0", O_RDWR);
    if (fd == -1) {
        printf("Failed to open /dev/tux0: %s\n", strerror(errno));
        return -1;
    }

    // Write to the device
    char write_buf[16] = "Tristan Barber\n";
    write(fd, write_buf, 16);
    printf("Write to device: %s", write_buf);

    // Read from the device - lseek is not defined in the device driver, so we can close and reopen to reset the file position
    // the proper fix would be to modify the device driver to support lseek, but not sure how to do that
    close(fd);
    fd = open("/dev/tux0", O_RDWR);
    if (fd == -1) {
        printf("Failed to open /dev/tux0: %s\n", strerror(errno));
        return -1;
    }
    char buffer[64] = {0};
    read(fd, buffer, sizeof(buffer));
    printf("Read from device: %s", buffer);

    close(fd);
}