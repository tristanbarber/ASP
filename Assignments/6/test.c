#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#define DEVICE "/dev/a6"

#define CDRV_IOC_MAGIC 'Z'
#define E2_IOCMODE1 _IO(CDRV_IOC_MAGIC, 1)
#define E2_IOCMODE2 _IO(CDRV_IOC_MAGIC, 2)

/* Test Case 1 *********************************************************************/
void* testCase1Thread(void* arg) {
    printf("Entered thread function for test case 1\n");
    int fd = open(DEVICE, O_RDWR);

    if (fd < 0) {
        perror("open");
    }
    else {
        printf("Thread opened device successfully.\n");
    }

    pause(); // Keep the thread alive to simulate holding the device open

    close(fd);
    return NULL;
}

void testCase1() {
    printf("Beginning Test Case 1\n");
    pthread_t t1, t2;

    pthread_create(&t1, NULL, testCase1Thread, NULL);
    sleep(1); // ensure some t1 to start execution
    pthread_create(&t2, NULL, testCase1Thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Test Case 1: Both threads finished execution -- SHOULD NOT REACH HERE\n");
}


/* Test Case 2 *********************************************************************/
void* testCase2Thread(void* arg) {
    printf("Entered thread function for test case 2\n");
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return NULL;
    }

    // Set the device into MODE2 using ioctl, which increments count2
    ioctl(fd, E2_IOCMODE2);
    printf("Thread function for test 2 opened in MODE2\n");
    pause(); // Keep the thread alive to simulate holding the device open

    close(fd);
    return NULL;
}

void testCase2() {
    printf("Beginning Test Case 2\n");
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return;
    }

    // Set the device into MODE2
    ioctl(fd, E2_IOCMODE2);
    printf("Main thread set device to MODE2\n");

    // Create two threads that will open the device in MODE2
    pthread_t t1, t2;
    pthread_create(&t1, NULL, testCase2Thread, NULL);
    pthread_create(&t2, NULL, testCase2Thread, NULL);
    sleep(1); // Allow threads to open the device in MODE2

    printf("Calling ioctl to switch to MODE1 (will deadlock)...\n");

    // Try to switch the device to MODE1, this should cause deadlock
    ioctl(fd, E2_IOCMODE1);

    // Clean up
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    close(fd);

    printf("Test Case 2: Both threads finished execution -- SHOULD NOT REACH HERE\n");
}

/* Test Case 3 *********************************************************************/
void testCase3() {
    printf("Beginning Test Case 3\n");
    int fd1, fd2;
    fd1 = open(DEVICE, O_RDWR);
    if (fd1 < 0) {
        perror("Failed to open device");
        return;
    }
    printf("Successfully opened device in MODE1\n");

    fd2 = open(DEVICE, O_RDWR);
    if (fd2 < 0) {
        perror("Failed to open device");
        return;
    }
    printf("Successfully opened device in MODE1\n");

    printf("Attemption to switch fd1 to MODE2, this should cause a deadlock.\n");
    // Try to switch to MODE2 while both are open in MODE1.
    if (ioctl(fd1, E2_IOCMODE2) < 0) {
        perror("Failed to switch to MODE2");
    }

    printf("Test Case 3: Successfully changed fd1 to MODE2 -- SHOULD NOT REACH HERE\n");
    close(fd1);
    close(fd2);
}

/* Test Case 4 *********************************************************************/
void* open_and_hold_sem2(void* arg) {
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) perror("open");
    pause();
    return NULL;
}

void deadlock_unreleased_sem2() {
    pthread_t t;
    pthread_create(&t, NULL, open_and_hold_sem2, NULL);
    sleep(1);

    printf("Second open should block on sem2...\n");
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) perror("open");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s [1|2|3|4]\n", argv[0]);
        return 1;
    }

    int option = atoi(argv[1]);
    printf("Running deadlock test case %d\n", option);

    switch(option) {
        case 1:
            testCase1();
            break;
        case 2:
            testCase2();
            break;
        case 3:
            testCase3();
            break;
        case 4:
            //deadlock_unreleased_sem2();
            break;
        default:
            printf("Invalid option. Use 1-4.\n");
    }

    return 0;
}