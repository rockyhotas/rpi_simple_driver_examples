#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/kernel.h>
/* In kernel 5.16, functions kstrto* have been moved to linux/kstrtox.h */

/* Meta Information */
/* Created by Rocky Hotas, based on the Johannes4Linux Linux Driver Tutorial:
 * https://github.com/Johannes4Linux/Linux_Driver_Tutorial
 * https://www.youtube.com/playlist?list=PLCGpd0Do5-I3b5TtyqeF1UdyD4C-S-dMa
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rocky Hotas");
MODULE_DESCRIPTION("An attempt at making a LED pulse with PWM");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "my_pulse_pwm_driver"
#define DRIVER_CLASS "MyModuleClass"
#define PWM_PERIOD 1000000
#define PWM_DEFAULT_STEPS 10000
#define PWM_DEFAULT_DELAY 10	// in microseconds
#define PWM_DEFAULT_STEPS_PER_MS 100
// number of steps per millisecond

static u32 pwm_steps;

static void steps_comp(u32 ms) {
	pwm_steps = ms*PWM_DEFAULT_STEPS_PER_MS;
	/* float is discouraged in kernel code; simply use ms here.
	 * https://stackoverflow.com/q/13886338 */
}

struct pwm_device *pwm0 = NULL;

static int duty_cycle_change(struct pwm_device *target, u32 period, u32 value) {
	struct pwm_state newstate;
	int ret;

	pwm_init_state(target, &newstate);
	newstate.enabled = true;
	newstate.period = period;
	if (pwm_set_relative_duty_cycle(&newstate, value, pwm_steps - 1) == 0) {
		ret = pwm_apply_state(pwm0, &newstate);
		return ret;
	}
	else {
		ret = -1;
		printk("Error in pwm_set_relative_duty_cycle(). Current value is %d\n", value);
		return ret;
	}
}

/**
 * @brief Write data to buffer
 */
static ssize_t driver_write(struct file *File, const char __user *user_buffer, size_t count, loff_t *offset) {
	u32 i; /* Warning: if i is u16 and PWM_RES is 65536, it will go from 0 to 65535; then, if
		* further incremented, it will again become 0. It will always be < 65536, so the
		* for loop will never end. */
	u32 value;

	/* kstrtou32: k str to u32 -> string to unsigned (int) 32 bit wide (to be used inside the
	 * kernel, k, instead of the usual userspace C functions and libraries).
	 * _from_user -> it is used to copy data from user, as `copy_from_user', but integrating
	 * the operations needed to parse and convert the string provided by userspace to u32.
	 * In header file kernel.h comments, this is recommended instead of the old kernel
	 * `simple_strto*' functions.
	 * `user_buffer' is a pointer to the string provided by the user; `count' is the number
	 * of bytes to be copied (use the same `count' provided to `driver_write': we want to
	 * copy the whole contents, hoping that it is all numeric (check it?); `10' is the number
	 * base to use (decimal, here); &value is a pointer to u32, where the result of the
	 * conversion to u32 is placed.
	 * ``Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
	 *  Preferred over simple_strtoul(). Return code must be checked''. */
	if (kstrtou32_from_user(user_buffer, count, 10, &value) < 0) {
		printk("Invalid value\n");
		return -1;
	}
	else {
		printk("Value is %d, count is %d\n", value, count);
		steps_comp(value);
		for (i = 0; i < pwm_steps; i++) {
			if (i < pwm_steps / 2) {
				if (duty_cycle_change(pwm0, PWM_PERIOD, i) != 0)
					return -1;
			}
			else {
				if (duty_cycle_change(pwm0, PWM_PERIOD, pwm_steps - 1 - i) != 0) {
					return -1;
				}
			}
			/* For some us delay, use udelay: https://www.kernel.org/doc/html/latest/timers/timers-howto.html */
			udelay(PWM_DEFAULT_DELAY);
		}
	}

	return count;
}

/**
 * @brief This function is called when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	printk("pulse_pwm_driver - open was called!\n");
	return 0;
}

/**
 * @brief This function is called when the device file is closed
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("pulse_pwm_driver - close was called!\n");
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
	printk("my_pulse_pwm_driver - Device number (with Major: %d, Minor: %d) was registered!\n", MAJOR(my_device_nr), MINOR(my_device_nr));

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

	pwm0 = pwm_request(0, "my_pulse_pwm");

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


