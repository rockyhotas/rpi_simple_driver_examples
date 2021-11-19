#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>

/* Meta Information */
/* Created by Rocky Hotas, based on the Johannes4Linux Linux Driver Tutorial:
 * https://github.com/Johannes4Linux/Linux_Driver_Tutorial
 * https://www.youtube.com/playlist?list=PLCGpd0Do5-I3b5TtyqeF1UdyD4C-S-dMa
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rocky Hotas");
MODULE_DESCRIPTION("A simple driver to access the hardware pwm to make a LED blink");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "my_pwm_driver"
#define DRIVER_CLASS "MyModuleClass"

/* Variables for pwm. Timings are measured in ns. */

struct pwm_device *pwm0 = NULL;
u32 pwm_on_time = 500000000;

/* We need an integer data type with the guarantee that it is 32-bit-wide, because
 * it must contain such a large number as 500000000.
 * u32 is uint32_t: https://stackoverflow.com/q/30896489 
 * (predating = pre-dating, belonging to a preceding date).
 * Such data types (u64) are used also in pwm.h. */

/**
 * @brief Write data to buffer
 */
static ssize_t driver_write(struct file *File, const char __user *user_buffer, size_t count, loff_t *offset) {
	int to_copy, not_copied, delta;
	char value;

	/* Get amount of data to copy. This driver is structured so that just a single character may
	 * be accepted by the user. Therefore, if `count' has more than 1 bytes (the size of `value'),
	 * only the first 1 byte is considered. */
	to_copy = min(count, sizeof(value));

	/* Copy data to user */
	not_copied = copy_from_user(&value, user_buffer, to_copy);

	/* Set the PWM "on time", according to the single character provided by the user.
	 * `value' belongs to the ASCII range a-j (97-106 in decimal). So, the on time
	 * may vary between 100000000 (when `value' is `a') and 100000000 * 9 (when `value'
	 * is `j'); each step increases the on time by 100000000. The initial `if' also checks
	 * that the value is in the permitted range. */
	if (value < 'a' || value > 'j')
		printk("Invalid value\n");
	else
		pwm_config(pwm0, 100000000 * (value - 'a'), 1000000000);

	/* Calculate data */
	delta = to_copy - not_copied;

	return delta;
}

/**
 * @brief This function is called when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	printk("pwm_driver - open was called!\n");
	return 0;
}

/**
 * @brief This function is called when the device file is closed
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("pwm_driver - close was called!\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
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

	pwm0 = pwm_request(0, "my_pwm");

	if (pwm0 == NULL) {
		printk("Could not get pwm0!\n");
		goto AddError;
	}
	
	pwm_config(pwm0, pwm_on_time, 1000000000);
	pwm_enable(pwm0);

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
	pwm_disable(pwm0);
	pwm_free(pwm0);
	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);


