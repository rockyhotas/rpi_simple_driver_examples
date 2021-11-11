#include <linux/module.h>
#include <linux/init.h>

/* Meta information */
/* The only required meta information is the license you are going to use.
 * Some distributions, in fact, only load GPL-licensed modules. */

/* Created by Rocky Hotas, based on the Johannes4Linux Linux Driver Tutorial:
 * https://github.com/Johannes4Linux/Linux_Driver_Tutorial
 * https://www.youtube.com/playlist?list=PLCGpd0Do5-I3b5TtyqeF1UdyD4C-S-dMa
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rocky Hotas");
MODULE_DESCRIPTION("A hello world LKM");

/* Now, two functions must be declared */

/* This function is executed whenever the module is loaded into the kernel */

static int __init ModuleInit(void) {
	/* The macro `__init' simply will tell the code that this is our init function */
	printk("Hello, Kernel!\n");
	/* printf is not available inside the kernel, because there is no CLI to print to */
	return 0;
	/* This is important, because normally the init function of a module also performs
	 * some initialization. If something fails, the return value is the way to notify this
	 * failure, with an integer non-zero value. If instead the init function has
	 * successfully completed all its operations, it returns 0 */
}

/* This function is executed if the module is removed from the kernel */

static void __exit ModuleExit(void) {
	/* `__exit' is a macro as `__init0 */
	printk("Goodbye, Kernel!\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);

/* This instructs the kernel that ModuleInit must be called when this module is loaded and
 * ModuleExit when this module is removed. */
