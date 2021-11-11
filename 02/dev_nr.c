#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

/* Meta Information */

/* Created by Rocky Hotas, based on the Johannes4Linux Linux Driver Tutorial:
 * https://github.com/Johannes4Linux/Linux_Driver_Tutorial
 * https://www.youtube.com/playlist?list=PLCGpd0Do5-I3b5TtyqeF1UdyD4C-S-dMa
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rocky Hotas");
MODULE_DESCRIPTION("A very basic LKM for a char device");

/* References:
 * https://tldp.org/LDP/lkmpg/2.6/html/x569.html
 * https://www.win.tue.nl/~aeb/linux/lk/lk-11.html
 */

/*
 * @brief This function is called when the device file is opened
 */

/*
 * As regards brief: https://stackoverflow.com/q/10797377
 */

static int driver_open(struct inode *device_file, struct file *instance) {
	printk("dev_nr - open was called!\n");
	return 0;
}

/**
 * @brief This function is called when the device file is opened
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("dev_nr - close was called!\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	/* See https://stackoverflow.com/a/49812248 and 
	 * https://stackoverflow.com/a/34215915
	 */
	.open = driver_open,
	.release = driver_close
};

#define MYMAJOR 0

/**
 * @brief This function is called when the module is loaded into the kernel
 */

static int __init ModuleInit(void) {
	int retval;
	printk("Hello, Kernel!\n");
	/* register device nr. */
	retval = register_chrdev(MYMAJOR, "mycustomdev", &fops);
	/* register_chdev is defined here:
	 * https://elixir.bootlin.com/linux/v5.14.15/source/include/linux/fs.h#L2835
	 * __register_chrdev is defined here:
	 * https://elixir.bootlin.com/linux/latest/source/fs/char_dev.c#L247
	 */
#if MYMAJOR == 0
	if(retval >= 0) {
	/* With a zero MYMAJOR, register_chrdev tries to dynamically allocate a major and return its
	 * number. The major 0 is reserved to ``Unnamed devices (e.g. non-device mounts)''
	 * (https://www.kernel.org/doc/Documentation/admin-guide/devices.txt), maybe it will never be
	 * returned as a result of this operation, but here it is included anyway.
	 */
		printk("dev_nr - registered Device number Major: %d, Minor: %d\n", retval, 0);
		/* The purpose of register_chrdev (and of __register_chrdev, which is called by register_chdev)
		 * is to register a major in any case, potentially with a subset of minors; not a single minor.
		 * The minor number is decided by the device driver, which in this case is this same kernel module.
		 * However, here the minor number is simply guessed. There is no instruction here to establish
		 * the minor number.
		 */
	}
#else
	if(retval == 0) {
		printk("dev_nr - registered Device number Major: %d, Minor: %d\n", MYMAJOR, 0);
	}
#endif
	else {
		printk("Could not register device number!\n");
		return -1;
	}
	return 0;
}

/**
 * @brief This function is called when the module is removed from the kernel
 */
static void __exit ModuleExit(void) {
	unregister_chrdev(MYMAJOR, "mycustomdev");
	printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
