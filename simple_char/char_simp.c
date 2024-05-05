#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h> //dev_t

#define DEVICE_NAME "char_simp_dev"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Simple character driver");

static int simple_char_open(struct inode *inode, struct file *instance) {
    printk(KERN_INFO "char_simp: open()\n");
    return 0;
}

static int simple_char_release(struct inode *inode, struct file *instance) {
    printk(KERN_INFO "char_simp: driver closed\n");
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = simple_char_open,
    .release = simple_char_release,
};

static int __init simple_char_init(void) {
    
    int regval;

    regval = register_chrdev(0, "char_simp", &fops); 

    if (regval < 0) {
        printk(KERN_ALERT "char_simp: Failed to load driver\n");
        return regval;
    }
    else if (regval > 0) {
        printk(KERN_ALERT "returned value:%d\n", regval);
    }

    printk(KERN_INFO "char_simp: Initialized successfully\n");
    
    return 0;
}

static void __exit simple_char_exit(void) {
    unregister_chrdev(237, "char_simp");
    printk(KERN_INFO "char_simp: Exited\n");
}

module_init(simple_char_init);
module_exit(simple_char_exit);
