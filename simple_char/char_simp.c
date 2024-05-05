#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h> //dev_t
#include <linux/uaccess.h> //access userspace

#define DEVICE_NAME "char_simp_dev"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Simple character driver");

static int major_num;
static struct cdev le_cdev;
int size;
char *charArray;
static int buffer_ptr = 0;

static int simple_char_open(struct inode *inode, struct file *instance) {
    printk(KERN_INFO "char_simp_dev: open()\n");
    return 0;
}

ssize_t read(struct file *FILE, char *user_buffer, size_t count, loff_t *offset) {
    int to_cpy, not_cpy, delta;

    to_cpy = min(count, buffer_ptr - *offset);
    not_cpy = copy_to_user(user_buffer, charArray + *offset, to_cpy);

    if (not_cpy != 0) {
        return -EFAULT;
    }

    delta = to_cpy - not_cpy;

    *offset += delta;
    printk(KERN_INFO "char_simp_dev: read %d bytes\n", delta);

    return delta;
}

ssize_t write (struct file *File, const char *user_buffer, size_t count, loff_t *offset){
    buffer_ptr = 0;
    int to_cpy, not_cpy, delta;

    to_cpy = min(count, sizeof(charArray) - buffer_ptr);

    not_cpy = copy_from_user(charArray + buffer_ptr, user_buffer, to_cpy);
    
    if (not_cpy != 0) {
        return -EFAULT;
    }

    buffer_ptr += to_cpy;
    delta = to_cpy - not_cpy;

    printk(KERN_INFO "char_simp_dev: written ");

    for (int i = 0; i<buffer_ptr; i++) {
        printk(KERN_INFO "%c", charArray[i]);
    };
    
    printk(KERN_INFO "char_simp_dev: written %d bytes\n", to_cpy);
    return delta;
}

static int simple_char_release(struct inode *inode, struct file *instance) {
    for (int i = 0; i<buffer_ptr; i++) {
        printk(KERN_INFO "%c", charArray[i]);
    };
    printk(KERN_INFO "char_simp_dev: driver closed\n");
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = simple_char_open,
    .release = simple_char_release,
    .read = read,
    .write = write,
};

static int __init simple_char_init(void) {
    dev_t dev_num;
    int regval;

    size = 255;

    charArray = kmalloc(size * sizeof(char), GFP_KERNEL);
    if (charArray == NULL) {
        return -ENOMEM;
    }

    for (int i = 0; i<size; i++) {
        charArray[i]=' ';
    };

    regval = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    
    if (regval < 0) {
        printk(KERN_ALERT "char_simp_dev: Memory allocation for major number failed\n");
        return regval;
    }

    major_num = MAJOR(dev_num);
    printk(KERN_INFO "char_simp_dev: Initialized with Major number %d\n", major_num);

    cdev_init(&le_cdev, &fops);
    regval = cdev_add(&le_cdev, dev_num, 1);
    
    if (regval < 0) {
        printk(KERN_ALERT "char_simp_dev: Failed to load device\n");
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
