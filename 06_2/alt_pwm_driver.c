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
MODULE_DESCRIPTION("An alternative simple driver to make a LED dim with PWM");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "my_alt_pwm_driver"
#define DRIVER_CLASS "MyModuleClass"
#define PWM_PERIOD 1000000

/* Variables for pwm. Timings are measured in ns. */

struct pwm_device *pwm0 = NULL;

/**
 * @brief Write data to buffer
 */
static ssize_t driver_write(struct file *File, const char __user *user_buffer, size_t count, loff_t *offset) {
	int to_copy, not_copied, delta;
	char value;
	struct pwm_state newstate;

	/* Get amount of data to copy. This driver is structured so that just a single character may
	 * be accepted by the user. Therefore, if `count' has more than 1 bytes (the size of `value'),
	 * only the first 1 byte is considered. */
	to_copy = min(count, sizeof(value));

	/* Copy data to user */
	not_copied = copy_from_user(&value, user_buffer, to_copy);

	if (value < 'a' || value > 'k')
		printk("Invalid value\n");
	else {
		pwm_init_state(pwm0, &newstate);
		newstate.enabled = true;
		newstate.period = PWM_PERIOD;
		pwm_set_relative_duty_cycle(&newstate, (value - 'a'), 10);
		pwm_apply_state(pwm0, &newstate);
	}

	/* Calculate data */
	delta = to_copy - not_copied;

	return delta;
}

/**
 * @brief This function is called when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	printk("alt_pwm_driver - open was called!\n");
	return 0;
}

/**
 * @brief This function is called when the device file is closed
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("alt_pwm_driver - close was called!\n");
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
	printk("my-alt-pwm-driver - Device number (with Major: %d, Minor: %d) was registered!\n", MAJOR(my_device_nr), MINOR(my_device_nr));

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

	pwm0 = pwm_request(0, "my_alt_pwm");

	if (pwm0 == NULL) {
		printk("Could not get pwm0!\n");
		goto AddError;
	}
	
	pwm_config(pwm0, PWM_PERIOD / 10, PWM_PERIOD);
	/* Dimming starts becoming visibile when the on time is a small fraction of the period,
	 * for example 1/10. */
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


