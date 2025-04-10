# Assignment 6

## Build and Load
Use `make` to build the driver and the test app. Load the driver using `sudo insmod assignment6.ko`. The test binary takes a signle integer command line argument, to specify which test case should be run (1-4). An example is `sudo ./test 1`. Make sure to unload and reload the driver between each test case to make sure the driver is in a good state before the test is run. This is done with `sudo rmmod -f assignment6`.

## Test Cases
### NOTE - RUN THESE AT YOUR OWN RISK. I was sometimes unable to kill the associated processes required to unload the driver after running this command. Whenever I had this issue I had to restart my linux machine. You are warned that you may have to do the same. The test cases may not behave predictibly if they are not run on a fresh load of the driver.

1. This test case creates a deadlock when two threads open the device in `MODE1` but doesn't release it. The first thread opens the device, which causes it to acquire `sem2` inside `e2_open`. The second thread opens the device after and also enters `e2_open`. Since the mode is still `MODE1`, it tries to acquire `sem2`, but blocks because it is already held by the first thread. Then the first thread will attempt to switch to `MODE2`, which causes the deadlock because the `ioctl` cannot happen until the second thread releases (which it can't). Deadlock occurs on the `wait_event_interruptible` on line 158.

2. This test is a circular wait dependancy. The driver is opened and switched to `MODE2`, then it is opened a second time in `MODE1`. From here we fork, and the child process will switch the first open to `MODE1` with `ioctl`, while the parent process attempts to switch into `MODE2` on the second open. Whichever `ioctl` happens second will cause a deadlock. Deadlock occurs on either line 158 or line 177 `wait_event_interruptible`, depending on which `ioctl` call is made first.

3. Test case 3 causes a deadlock by exploiting a synchronization issues between reading and changing modes. The device is opened in the main process. Two threads are spawned. One reads repeatedly from the driver, while one switches between modes through ioctl. Eventually both threads will get stuck on the down_interruptible call within read, and one of the down_interruptible calls within the ioctl function.

4. Test case 4 simulates a situation where a user is using the device driver as a logger of sorts. The device will be opened in MODE1, and then it will pause(). This simulates the device logging data, etc. it doesn't actually matter. The point is that the device is never closed or mode switched. A second user then comes along and attempts to open the device. That open will block on the down_interruptible(&devc->sem2); on line 49, since the first instance has sem2 locked. This causes a deadlock only for the second thread, which will remain blocked indefinitely since the first instance will not close.

## Race Conditions