#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/* Meta Information */
/* Created by Rocky Hotas, based on the Johannes4Linux Linux Driver Tutorial:
 * https://github.com/Johannes4Linux/Linux_Driver_Tutorial
 * https://www.youtube.com/playlist?list=PLCGpd0Do5-I3b5TtyqeF1UdyD4C-S-dMa
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rocky Hotas");
MODULE_DESCRIPTION("Create a kernel thread within a kernel module; end it when removing the module");

#define DRIVER_NAME "custom-device-driver"
#define DRIVER_CLASS "CustomDeviceClass"
#define BUFFER_LENGTH 1024

static char cust_dev_buffer[BUFFER_LENGTH];
static size_t cust_dev_buffer_index = 0;

static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

static struct task_struct *my_thread;

int in_background(void *pv) 
{
	int i=0;
	while(!kthread_should_stop()) {
		printk(KERN_INFO "in_background function %d\n", i++);
		msleep(1000);
	} 
	printk("Exiting from kthread...\n");
	return 0; 
}

/**
 * @brief Read data from the buffer. Note that the prototype of such a function (as well as driver_write) is
 * provided in the definition of the `struct file_operations' in the included file linux/fs.h.
 */

static ssize_t driver_read(struct file *File, char __user *user_buffer, size_t count, loff_t *offset) {
	int to_copy, not_copied, delta;

	to_copy = min(count, cust_dev_buffer_index);

	not_copied = copy_to_user(user_buffer, cust_dev_buffer, to_copy);

	delta = to_copy - not_copied;

	printk("User requested to read %d bytes from the device: actually %d bytes have been read\n", count, delta);

	return delta;
}

/**
 * @brief Write data to the buffer
 */

static ssize_t driver_write(struct file *File, const char __user *user_buffer, size_t count, loff_t *offset) {

	int to_copy, not_copied, delta;

	to_copy = min(count, sizeof(cust_dev_buffer));

	not_copied = copy_from_user(cust_dev_buffer, user_buffer, to_copy);

	delta = to_copy - not_copied;

	cust_dev_buffer_index = delta;

	printk("User requested to write %d bytes into the device internal buffer: actually %d bytes have been written\n", count, delta);

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

	if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
		printk("Device number could not be allocated!\n");
		return -1;
	}
	printk("custom-device-driver - Device number (with Major: %d, Minor: %d) was registered!\n", MAJOR(my_device_nr), MINOR(my_device_nr));

	if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		printk("Device class can not be created!\n");
		goto ClassError;
	}

	if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
		printk("Can not create device file!\n");
		goto FileError;
	}

	cdev_init(&my_device, &fops);
	my_device.owner = THIS_MODULE;

	if (cdev_add(&my_device, my_device_nr, 1) == -1) {
		printk("Registering of device to kernel failed!\n");
		goto AddError;
	}

	printk("Module name: %s\n", THIS_MODULE->name);
	printk("cdev owner: %s\n", my_device.owner->name);

	/* Create and run a new kernel thread:
	 *
	 * https://elixir.bootlin.com/linux/v5.15.5/source/include/linux/kthread.h#L16
	 * https://elixir.bootlin.com/linux/v5.15.5/source/include/linux/kthread.h#L41
	 * https://docs.huihoo.com/linux/kernel/2.6.26/kernel-api/re77.html
	 *
	 * Note that the name apparently is truncated after 15 characters, so that it
	 * is stored in a 16 elements, NULL-terminated, char array. */
	my_thread = kthread_run(in_background, NULL, "A my_thread attempt");

	if (my_thread)
		printk("Kthread created successfully\n");
	else
		printk("There was an error while trying to create a kthread!\n");

	return 0;

AddError:
	device_destroy(my_class, my_device_nr);
FileError:
	class_destroy(my_class);
ClassError:
	unregister_chrdev_region(my_device_nr, 1);
	return -1;
}

/**
 * @brief This function is called when the module is removed from the kernel
 */
static void __exit ModuleExit(void) {
	kthread_stop(my_thread);
	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
