#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#define DEVICE "/dev/a6"

#define CDRV_IOC_MAGIC 'Z'
#define E2_IOCMODE1 _IO(CDRV_IOC_MAGIC, 1)
#define E2_IOCMODE2 _IO(CDRV_IOC_MAGIC, 2)

// Required globals

int fdTest3;

/* Test Case 1 *********************************************************************/
void* testCase1t1(void* arg) {
    printf("Thread 1: Entering thread function 1 for test case 1 - opening in MODE1\n");

    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Thread 1: Failed to open device");
        pthread_exit(NULL);
    }
    sleep(3);
    printf("Thread 1: Attempt to switch to MODE2, should cause deadlock.\n");
    ioctl(fd, E2_IOCMODE2);

    printf("Thread 1: Successfully switched to MODE2 -- SHOULD NOT REACH HERE\n");
    close(fd);
    return NULL;
}

void* testCase1t2(void* arg) {
    printf("Thread 2: Entering thread function 2 for test case 1\n");
    sleep(1); // Ensure first open acquires sem1 and blocks on sem2

    printf("Thread 2: Attempting to open in MODE1\n");
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Thread 2: Failed to open device");
        pthread_exit(NULL);
    }

    printf("Thread 2: Successfully opened second device -- SHOULD NOT REACH HERE\n");

    close(fd);
    return NULL;
}

void testCase1() {
    printf("Beginning test case 1\n");

    pthread_t t1, t2;

    printf("Creating pthreads\n");
    pthread_create(&t1, NULL, testCase1t1, NULL);
    pthread_create(&t2, NULL, testCase1t2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Finished execution of test case 1 -- SHOULD NOT REACH HERE\n");
    return;
}

/* Test Case 2 *********************************************************************/
void testCase2() {
    printf("Beginning test case 2\n");

    printf("Opening once and switching to MODE2\n");
    int fd1 = open("/dev/a6", O_RDWR);
    ioctl(fd1, E2_IOCMODE2);

    printf("Opening again in MODE1\n");
    int fd2 = open("/dev/a6", O_RDWR);

    printf("Successfully opened device driver twice - now doing fork\n");
    if (fork() == 0) {
        // Child switches to MODE2
        printf("Inside child process - switching instance 1 to MODE1, should cause deadlock if this was printed second\n");
        ioctl(fd1, E2_IOCMODE2);
        exit(0);
    } else {
        printf("Inside parent process - switching instance 2 to MODE2, should cause deadlock if this was printed second\n");
        ioctl(fd2, E2_IOCMODE1);

        printf("Successfully did ioctl inside parent process -- SHOULD NOT REACH THIS POINT\n");
    }
    wait(NULL);

    printf("Reached end of test case 2 -- SHOULD NOT REACH THIS POINT\n");
    return;
}

/* Test Case 3 *********************************************************************/
void *testCase3ReadThread(void *arg) {
    // initial write so that there is data to read
    write(fdTest3, "Initial Write", 13);

    char buffer[1024];
    ssize_t ret;
    while(1) {
        printf("Read Thread: Reading from device\n");
        ret = read(fdTest3, buffer, 1024);
        if (ret < 0) {
            perror("Read failed");
            return NULL;
        }
        usleep(100);
    }
    return NULL;
}
void *testCase3IoctlThread(void *arg) {
    while(1) {
        printf("Ioctl Thread: switching to MODE1\n");
        if (ioctl(fdTest3, E2_IOCMODE1, 0) < 0) {
            perror("Failed to switch to MODE1");
            return NULL;
        }
        usleep(100);

        printf("Ioctl Thread: switching to MODE2\n");
        if (ioctl(fdTest3, E2_IOCMODE2, 0) < 0) {
            perror("Failed to switch to MODE2");
            return NULL;
        }
        usleep(100);
    }
    return NULL;
}

int testCase3() {
    printf("Beginning test case 3 - NOTE THAT IF PRINTS STOP OCCURING, THAT MEANS DEADLOCK HAPPENED\n");
    sleep(3);
    pthread_t testCase3Read, testCase3Ioctl;
    
    // Open the device
    printf("Test Case 3: Opening device\n");
    fdTest3 = open(DEVICE, O_RDWR);
    if (fdTest3 < 0) {
        perror("Error opening device");
        return 1;
    }

    // Create threads
    printf("Test Case 3: Spawning threads\n");
    if (pthread_create(&testCase3Read, NULL, testCase3ReadThread, NULL) != 0)
    if (pthread_create(&testCase3Ioctl, NULL, testCase3IoctlThread, NULL) != 0)

    // Wait for threads to finish
    pthread_join(testCase3Read, NULL);
    pthread_join(testCase3Ioctl, NULL);

    close(fdTest3);
    return 0;
}


/* Test Case 4 *********************************************************************/
void* testCase4Thread(void* arg) {
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Test case 4 failed to open");
    }

    pause(); // Simulate device being open forever
    return NULL;
}

void testCase4() {
    pthread_t t;
    pthread_create(&t, NULL, testCase4Thread, NULL);
    sleep(1);

    printf("Test case 4: Attempting second open. Should cause deadlock.\n");
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Test case 4 failed to open");
    }

    printf("Reached end of test case 4. Deadlock did not occur -- SHOULD NOT REACH HERE.\n");
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
            testCase4();
            break;
        default:
            printf("Invalid option. Use 1-4.\n");
    }

    return 0;
}