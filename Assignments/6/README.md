# Assignment 6

## Build and Load
Use `make` to build the driver and the test app. Load the driver using `sudo insmod assignment6.ko`. The test binary takes a signle integer command line argument, to specify which test case should be run (1-4). An example is `sudo ./test 1`. Make sure to unload and reload the driver between each test case to make sure the driver is in a good state before the test is run. This is done with `sudo rmmod -f assignment6`.

## Test Cases
### NOTE - RUN THESE AT YOUR OWN RISK. I was sometimes unable to kill the associated processes required to unload the driver after running this command. Whenever I had this issue I had to restart my linux machine. You are warned that you may have to do the same. The test cases may not behave predictibly if they are not run on a fresh load of the driver.

1. This test case creates a deadlock when two threads open the device in `MODE1` but doesn't release it. The first thread opens the device, which causes it to acquire `sem2` inside `e2_open`. The second thread opens the device after and also enters `e2_open`. Since the mode is still `MODE1`, it tries to acquire `sem2`, but blocks because it is already held by the first thread. Then the first thread will attempt to switch to `MODE2`, which causes the deadlock because the `ioctl` cannot happen until the second thread releases (which it can't). Deadlock occurs on the `wait_event_interruptible` on line 158.

2. This test is a circular wait dependancy. The driver is opened and switched to `MODE2`, then it is opened a second time in `MODE1`. From here we fork, and the child process will switch the first open to `MODE1` with `ioctl`, while the parent process attempts to switch into `MODE2` on the second open. Whichever `ioctl` happens second will cause a deadlock. Deadlock occurs on either line 158 or line 177 `wait_event_interruptible`, depending on which `ioctl` call is made first.

3. Test case 4 causes a deadlock because of a `pthread_join` within the main process. The main process will spawn a thread that waits for 3 seconds, and then opens the device driver and attempts to `ioctl` ot `MODE1`. While the thread is waiting, the main process will open the device driver first, and `ioctl` into `MODE2`. Then it will `pthread_join` and wait for the thread to finish execution. When the thread attempts to execute `ioctl`, it will get stuck on the `wait_event_interruptible(devc->queue2, (devc->count2 == 1));` line 177. As a result it cannot finish executing, and thus the parent is also stuck waiting.

4. Test case 4 causes a deadlock because of a `pthread_join` within the main process. The main process will create a thread that opens the device driver, and then closes it. After it creates the thread it will open the driver itself. What ends up happening is the main process waiting for the thread to finish executing due to the `pthread_join`, but the thread will never finish executing since it is waiting for `sem2` in the `open` call. This deadlock does not happen every time. The driver may have to be freshly loaded for it to work.

## Race Conditions