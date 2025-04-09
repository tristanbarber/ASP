# Assignment 6

## Build and Load
Use `make` to build the driver and the test app. Load the driver using `sudo insmod assignment6.ko`. The test binary takes a signle integer command line argument, to specify which test case should be run (1-4). An example is `sudo ./test 1`. Make sure to unload and reload the driver between each test case to make sure the driver is in a good state before the test is run. This is done with `sudo rmmod assignment6`.

## Test Cases
1. This test case creates a deadlock when two threads attempt to open the device in `MODE1` and then do nothing else. `sem2` is initialized to 1. When the first `e2_open` is called, `sem2` is acquired, but not released. `sem2` would typically be released in the `e2_release` function. Thus, when the second thread attempts to open the device in `MODE1`, that thread will deadlock being stuck waiting on `sem2`.

2. This test case causes a deadlock when multiple threads open the device in `MODE2`, and then an attempt is made to switch to `MODE1` via ioctl. In the `e2_ioctl` function, the deadlock occurs when the `count2` variable is greater than 1. The second thread attempts to switch to `MODE1`. However, the thread blocks in the `wait_event_interruptible(devc->queue2, (devc->count2 == 1))` call because count2 is still greater than 1, since we opened too many devices in `MODE2`.

3. If there are multiple processes holding `sem1` in `MODE1` and one process attempts to switch the mode to `MODE2`, the mode change is blocked until the processes in `MODE1` are finished. If those processes do not release `sem1`, the mode change cannot proceed, causing a deadlock. This test is similar to the above test case, with `MODE1` and `MODE2` switched. However, the primary difference is that this is being run in a single thread instead of simulating multiple opens from "different users".

## Race Conditions