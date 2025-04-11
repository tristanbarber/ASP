# Assignment 6

## Build and Load
Use `make` to build the driver and the test app. Load the driver using `sudo insmod assignment6.ko`. The test binary takes a signle integer command line argument, to specify which test case should be run (1-4). An example is `sudo ./test 1`. Make sure to unload and reload the driver between each test case to make sure the driver is in a good state before the test is run. This is done with `sudo rmmod -f assignment6`.

## Test Cases
### NOTE - RUN THESE AT YOUR OWN RISK. I was sometimes unable to kill the associated processes required to unload the driver after running this command. Whenever I had this issue I had to restart my linux machine. You are warned that you may have to do the same. The test cases may not behave predictibly if they are not run on a fresh load of the driver.

1. Test case 1 creates a deadlock when two threads open the device in `MODE1` but doesn't release it. The first thread opens the device, which causes it to acquire `sem2` inside `e2_open`. The second thread opens the device after and also enters `e2_open`. Since the mode is still `MODE1`, it tries to acquire `sem2`, but blocks because it is already held by the first thread. Then the first thread will attempt to switch to `MODE2`, which causes the deadlock because the `ioctl` cannot happen until the second thread releases (which it can't). Deadlock occurs on the `wait_event_interruptible` on line 158.

2. Test case 2 is a circular wait dependancy. The driver is opened and switched to `MODE2`, then it is opened a second time in `MODE1`. From here we fork, and the child process will switch the first open to `MODE1` with `ioctl`, while the parent process attempts to switch into `MODE2` on the second open. Whichever `ioctl` happens second will cause a deadlock. Deadlock occurs on either line 158 or line 177 `wait_event_interruptible`, depending on which `ioctl` call is made first.

3. Test case 3 causes a deadlock because of a `pthread_join` within the main process. The main process will spawn a thread that waits for 3 seconds, and then opens the device driver and attempts to `ioctl` ot `MODE1`. While the thread is waiting, the main process will open the device driver first, and `ioctl` into `MODE2`. Then it will `pthread_join` and wait for the thread to finish execution. When the thread attempts to execute `ioctl`, it will get stuck on the `wait_event_interruptible(devc->queue2, (devc->count2 == 1));` line 177. As a result it cannot finish executing, and thus the parent is also stuck waiting.

4. Test case 4 causes a deadlock because of a `pthread_join` within the main process. The main process will create a thread that opens the device driver, and then closes it. After it creates the thread it will open the driver itself. What ends up happening is the main process waiting for the thread to finish executing due to the `pthread_join`, but the thread will never finish executing since it is waiting for `sem2` in the `open` call. This deadlock does not happen every time. The driver may have to be freshly loaded for it to work.

## Race Conditions

1. 
Critical regions being evalulated: `MODE1` in `e2_read()` and `MODE1` in `e2_write`
If two processes or threads were both accessing the driver at the same time, and the driver is in `MODE1`, there is a race condition in the event that one thread is trying to read and one thread is trying to write at the same time. This is because for read and write in `MODE1`, the sem1 is released at the beginning of the critical region, before the ramdisk is accessed (copy_to_user or copy_from_user function calls).

2. 
Critical regions being evaluated: `e2_open()` and `e2_release()`
These two critical regions happening concurrently should not cause a race condition. All data accesses are well guarded by controlling access to `sem1`. The reference count data being modified is the counters, either `count1` or `count2` based on what mode the driver is in. 

3. 
Critical regions being evaluated: `MODE1` of `e2_write` - with 2 processes attempting to write at the same time
If two threads are sharing a file descriptor, they could both attempt to write at the same time. If they both tried to write at the same time, and the device is in `MODE1`, there is an issue. `sem1` is released before writing (using copy_from_user) and updating `f_pos`. This is similar to the Race Condition 1 evaluated above. The common denominator is `sem1` being released too early.

4. 
Critical regions being evaluated: Two processes attempting to mode switch with `ioctl` concurrently
If two processes attempt to call `ioctl` to change the mode at the same time, there should not be a race condition. Within the critical regions for both `E2_IOCMODE1` and `E2_IOCMODE2`, the `sem1` lock is acquired, then the data `mode`, `count1` and `count2` are accessed before `sem1` is released. However, this scenario can certainly cause a deadlock, as shown in test case 2 of the deadlocks portion.