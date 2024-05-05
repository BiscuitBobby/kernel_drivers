#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h> //dev_t

#define DEVICE_NAME "char_simp_dev"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Simple character driver");

static int major_num;
static struct cdev le_cdev;

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
    dev_t dev_num;
    int regval;

    regval = alloc_chrdev_region(&dev_num, 0, 1, "char_simp");
    
    if (regval < 0) {
        printk(KERN_ALERT "char_simp: Memory allocation for major number failed\n");
        return regval;
    }

    major_num = MAJOR(dev_num);
    printk(KERN_INFO "char_simp: Initialized with Major number %d\n", major_num);

    cdev_init(&le_cdev, &fops);
    regval = cdev_add(&le_cdev, dev_num, 1);
    
    if (regval < 0) {
        printk(KERN_ALERT "char_simp: Failed to load device\n");
        return regval;
    }
    
    else if (regval > 0) {
        printk(KERN_ALERT "returned value:%d\n", regval);
    }
    
    return 0;
}

static void __exit simple_char_exit(void) {
    dev_t dev_num = MKDEV(major_num, 0);

    cdev_del(&le_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "char_simp: Exited\n");
}

module_init(simple_char_init);
module_exit(simple_char_exit);
