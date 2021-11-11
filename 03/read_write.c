#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

/* Meta Information */
/* Created by Rocky Hotas, based on the Johannes4Linux Linux Driver Tutorial:
 * https://github.com/Johannes4Linux/Linux_Driver_Tutorial
 * https://www.youtube.com/playlist?list=PLCGpd0Do5-I3b5TtyqeF1UdyD4C-S-dMa
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rocky Hotas");
MODULE_DESCRIPTION("Create a char device; read from it or write to it some strings");

/* This device actually consists in a buffer of characters: they
 * can be written, or they can be read, by the user.
 */

#define DRIVER_NAME "custom-device-driver"
#define DRIVER_CLASS "CustomDeviceClass"
#define BUFFER_LENGTH 1024

static char cust_dev_buffer[BUFFER_LENGTH];
static size_t cust_dev_buffer_index = 0;

/* This is set by driver_write, but it will only be used inside driver_read.
 * If cust_dev_buffer is not empty, it is filled till position `cust_dev_buffer_index - 1'.
 * So, only up to cust_dev_buffer_index characters can be read from cust_dev_buffer.
 */

/* As regards the cust_dev_buffer_index data type: this variable is used later in
 * `to_copy = min(count, cust_dev_buffer_index);
 * This comparison may work better if the data types of the two variables match. `count' is by
 * definition in linux/fs.h a `size_t', whose definition is machine-dependent, but with the
 * constraint that it is an unsigned integer type (char, int, long, short, ...). Therefore,
 * size_t is perfectly suitable for an array index, which is an unsigned integer number.
 *
 * https://stackoverflow.com/q/2550774/2501622
 * https://stackoverflow.com/a/131833/2501622
 * https://stackoverflow.com/q/29475226/2501622
 * https://stackoverflow.com/a/19732413/2501622
 *
 */

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

/**
 * @brief Read data from the buffer. Note that the prototype of such a function (as well as driver_write) is
 * provided in the definition of the `struct file_operations' in the included file linux/fs.h.
 */

static ssize_t driver_read(struct file *File, char __user *user_buffer, size_t count, loff_t *offset) {
	int to_copy, not_copied, delta;

	/* Determine the amount of data to be read from the buffer. This also prevents the user to read
	 * from some other kernel-space area, if `count' is greater than `cust_dev_buffer_index'. This
	 * precaution is important as regards security: the user should never be given unauthorized access
	 * to kernel-space. */
	to_copy = min(count, cust_dev_buffer_index);

	/* Copy the internal data from the internal buffer to the user. */
	not_copied = copy_to_user(user_buffer, cust_dev_buffer, to_copy);

	/* Determine the actual number of copied bytes */
	delta = to_copy - not_copied;

	printk("User requested to read %d bytes from the device: actually %d bytes have been read\n", count, delta);

	return delta;
}

/**
 * @brief Write data to the buffer
 */

static ssize_t driver_write(struct file *File, const char __user *user_buffer, size_t count, loff_t *offset) {

	/* This time, the user buffer is a source of data, so it is not an internal variable: it contains fixed, given data */

	int to_copy, not_copied, delta;

	/* Determine the amount of data to be written into the buffer. If `count' exceeds the size of the buffer,
	 * write only sizeof(cust_dev_buffer) characters. This is a security precaution similar to the one for driver_read,
	 * as regards unauthorized user writes in the kernel-space. */
	to_copy = min(count, sizeof(cust_dev_buffer));

	/* Write into the internal buffer the data provided by the user. If cust_dev_buffer_index was non-zero, that is if the
	 * cust_dev_buffer was non-empty, this overwrites it starting from its beginning. */
	not_copied = copy_from_user(cust_dev_buffer, user_buffer, to_copy);

	/* Determine the actual number of written bytes */
	delta = to_copy - not_copied;

	/* The new actual amount of data inside the buffer */
	cust_dev_buffer_index = delta;

	printk("User requested to write %d bytes into the device internal buffer: actually %d bytes have been written\n", count, delta);

	/* Terminate the string with NULL into the internal buffer */
	cust_dev_buffer[delta] = 0;

	printk("The device internal buffer has the following contents: %s\n", cust_dev_buffer);

	return delta;
}

/*
 * @brief This function is called when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	printk("dev_nr - open was called!\n");
	return 0;
}

/**
 * @brief This function is called when the device file is closed
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("dev_nr - close was called!\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.read = driver_read,
	.write = driver_write
};

/**
 * @brief This function is called when the module is loaded into the kernel
 */

static int __init ModuleInit(void) {
	printk("Hello, Kernel!\n");

	/* Use dynamic allocation for device number */
	if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
		printk("Device number could not be allocated!\n");
		return -1;
	}
	printk("custom-device-driver - Device number (with Major: %d, Minor: %d) was registered!\n", MAJOR(my_device_nr), MINOR(my_device_nr));

	/* Create a device class */
	if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		printk("Device class can not be created!\n");
		goto ClassError;
	}

	/* Create a device file */
	if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
		printk("Can not create device file!\n");
		goto FileError;
	}

	/* Initialize device file */
	cdev_init(&my_device, &fops);

	/* Register device to kernel */
	if (cdev_add(&my_device, my_device_nr, 1) == -1) {
		printk("Registering of device to kernel failed!\n");
		goto AddError;
	}

	return 0;

AddError:
	device_destroy(my_class, my_device_nr);
FileError:
	class_destroy(my_class);
ClassError:
	unregister_chrdev_region(my_device_nr, 1);
	return -1;

	/* gotos are undesirable in C, but in Linux device drivers they are very useful and used. */
}

/**
 * @brief This function is called when the module is removed from the kernel
 */
static void __exit ModuleExit(void) {
	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
