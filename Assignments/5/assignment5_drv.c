#include <linux/module.h>	/* for modules */
#include <linux/fs.h>		/* file_operations */
#include <linux/uaccess.h>	/* copy_(to,from)_user */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/cdev.h>		/* cdev utilities */
#include <linux/errno.h>	/* error codes */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/semaphore.h>

#define MYDEV_NAME "mycdev"
#define RAMDISK_SIZE (size_t) (16*PAGE_SIZE)
#define CDRV_IOC_MAGIC 'X'
#define ASP_CLEAR_BUF _IO(CDRV_IOC_MAGIC, 1)

struct asp_mycdev {
    struct cdev dev;
    char *ramdisk;
    size_t ramdisk_size; // added this to allow copying of functions from tuxdrv.c
    struct semaphore sem;
    int devNo;
}

// params
static int majorno = 500, minorno = 0;
static size_t size = RAMDISK_SIZE;
static int numdevices = 3;
module_param(majorno, int, S_IRUGO);
module_param(size, long, S_IRUGO);
module_param(numdevices, int, S_IRUGO);

// global pointers for multiple driver implementation
static struct asp_mycdev *devices;
static struct class *device_nodes;

static ssize_t mycdev_read(struct file *file, char __user *buf, size_t lbuf, loff_t *ppos)
{
    // get the asp_mycdev struct
    struct asp_mycdev *dev = file->private_data;

    // enter critical region
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    // this implementation is the same as the tuxdrv from activity 3
	int nbytes;
	if ((lbuf + *ppos) > dev->ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_to_user(buf, dev->ramdisk + *ppos, lbuf);
	*ppos += nbytes;
	pr_info("READ: nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

    // exit critical region, so post sem
    up(&dev->sem);
	return nbytes;
}

static ssize_t mycdev_write(struct file *file, const char __user *buf, size_t lbuf, loff_t *ppos)
{
    // get the asp_mycdev struct
    struct asp_mycdev *dev = file->private_data;

    // enter critical region
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

	int nbytes;
	if ((lbuf + *ppos) > dev->ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_from_user(dev->ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;
	pr_info("WRITE: nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

    // exit critical region, so post sem
    up(&dev->sem);
	return nbytes;
}

static int mycdev_open(struct inode *inode, struct file *file)
{
    struct ASP_mycdev *dev = container_of(inode->i_cdev, struct ASP_mycdev, cdev);
    file->private_data = dev;
    pr_info("OPEN: %s%d:\n", MYDEV_NAME, dev->devNo);
    return 0;
}

static int mycdev_release(struct inode *inode, struct file *file)
{
    struct ASP_mycdev *dev = file->private_data;
    pr_info("RELEASE: %s%d:\n\n", MYDEV_NAME, dev->devNo);
    return 0;
}

static loff_t mycdev_llseek(struct file *file, loff_t off, unsigned int whence)
{
    struct ASP_mycdev *dev = file->private_data;
    loff_t newpos;

    // enter critical region
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    switch (whence) {
    case SEEK_SET:
        newpos = off;
        break;
    case SEEK_CUR:
        newpos = file->f_pos + off;
        break;
    case SEEK_END:
        newpos = dev->ramdisk_size + off;
        break;
    default:
        up(&dev->sem);
        return -EINVAL;
    }

    // Ensure new position is valid
    if (newpos < 0) {
        up(&dev->sem);
        return -EDOM;
    }

    // Expand buffer if seeking beyond current ramdisk size
    if (newpos > dev->ramdisk_size) {
        char *tmp = krealloc(dev->ramdisk, newpos, GFP_KERNEL);
        if (!tmp) {
            up(&dev->sem);
            return -ENOMEM;  // Memory allocation failed
        }

        // Zero out the newly allocated memory region
        memset(tmp + dev->ramdisk_size, 0, newpos - dev->ramdisk_size);

        dev->ramdisk = tmp;
        dev->ramdisk_size = newpos;
    }

    // Update file position
    file->f_pos = newpos;
    pr_info("SEEK: pos=%lld\n", file->f_pos);

    // exit critical region, so post sem
    up(&dev->sem);
    return newpos;
}

static long mycdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) 
{
    // Verify the cmd is the same as our magic
    if (_IOC_TYPE(cmd) != CDRV_IOC_MAGIC) 
        return -ENOTTY;
    
    struct ASP_mycdev *dev = file->private_data;

    // enter critical region
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    long retval = 0;
    switch (cmd) {
    case ASP_CLEAR_BUF:
        // Clear the ramdisk and reset file position
        memset(dev->ramdisk, 0, dev->ramdisk_size);
        file->f_pos = 0;
        break;
    default:
        retval = -ENOTTY;
        break;
    }

    // exit critical region, so post sem
    up(&dev->sem);
    return retval;
}

static const struct file_operations mycdev_fops = {
	.owner = THIS_MODULE,
	.read = mycdev_read,
	.write = mycdev_write,
	.open = mycdev_open,
	.release = mycdev_release,
    .llseek = mycdev_llseek,
    .unlocked_ioctl = mycdev_ioctl,
};

static int __init my_init(void)
{
	ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
	first = MKDEV(my_major, my_minor);
	register_chrdev_region(first, count, MYDEV_NAME);
	my_cdev = cdev_alloc();
	cdev_init(my_cdev, &mycdev_fops);
	cdev_add(my_cdev, first, count);
	pr_info("\nTristan Barber: Succeeded in registering character device %s\n", MYDEV_NAME);
	return 0;
}

static void __exit my_exit(void)
{
	cdev_del(my_cdev);
	unregister_chrdev_region(first, count);
	pr_info("\nTristan Barber: device unregistered\n");
	kfree(ramdisk);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("na");
MODULE_LICENSE("GPL v2");