#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h> //dev_t
#include <linux/uaccess.h> //access userspace
#include <linux/moduleparam.h> //access userspace
#include <linux/ioctl.h>

#define DEVICE_NAME "ioctl_char_dev"
#define MAX_SZ _IOR(MAJOR(0), 0, int)
#define CUR_SZ _IOR(MAJOR(0), 1, int)
#define INC_SZ _IOR(MAJOR(0), 2, int)


MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Simple character driver with ioctl");

static int major_num;
static struct cdev le_cdev;
static int size = 255;
char *charArray;
static int buffer_ptr = 0;

module_param(size, int, 0644);

static int simple_char_open(struct inode *inode, struct file *instance) {
    printk(KERN_INFO "ioctl_char_dev: opened device\n");
    return 0;
}

ssize_t dev_read(struct file *FILE, char *user_buffer, size_t count, loff_t *offset) {
    int to_cpy, not_cpy, delta;

    to_cpy = min_t(int, count, buffer_ptr - *offset);
    not_cpy = copy_to_user(user_buffer, charArray + *offset, to_cpy);

    if (not_cpy != 0) {
        return -EFAULT;
    }

    delta = to_cpy - not_cpy;

    *offset += delta;
    printk(KERN_INFO "ioctl_char_dev: read %d bytes\n", delta);

    return delta;
}

ssize_t dev_write (struct file *File, const char *user_buffer, size_t count, loff_t *offset){
    buffer_ptr = 0;
    int to_cpy, not_cpy, delta;

    to_cpy = min_t(int, count, size - buffer_ptr);
    printk(KERN_INFO "ioctl_char_dev: count: %d length of charArray: %d buffer_ptr: %d bytes to copy: %d", count, size, buffer_ptr, to_cpy);

    not_cpy = copy_from_user(charArray + buffer_ptr, user_buffer, to_cpy);
    
    if (not_cpy != 0) {
        return -EFAULT;
    }

    buffer_ptr += to_cpy;
    delta = to_cpy - not_cpy;

    printk(KERN_INFO "ioctl_char_dev: written ");
    
    /*for (int i = 0; i<buffer_ptr; i++) {  //for debugging
        printk(KERN_INFO "%c", charArray[i]);
    };*/
    
    printk(KERN_INFO "ioctl_char_dev: written %d bytes\n", to_cpy);
    return delta;
}

static int simple_char_release(struct inode *inode, struct file *instance) {
    /*for (int i = 0; i<buffer_ptr; i++) {  //for debugging
        printk(KERN_INFO "%c", charArray[i]);
    };*/
    printk(KERN_INFO "ioctl_char_dev: driver closed\n");
    return 0;
}

static long int mon_ioctl(struct file *file, unsigned cmd, unsigned long arg){
    printk(KERN_INFO "ioctl_char_dev: ioctl function call, cmd: %d", cmd); 
    int answer;
    switch (cmd)
    {
    case MAX_SZ:
        printk(KERN_INFO "ioctl_char_dev: device size: %d",size);
        break;
    case CUR_SZ:
        printk(KERN_INFO "ioctl_char_dev: current size: %d",buffer_ptr);
    case INC_SZ:
        int inc_value = (int)arg; 
        printk("ioctl_char_dev: new size - %d", inc_value);      
        char *newArray = kmalloc(inc_value * sizeof(char), GFP_KERNEL);
        for (int i = 0; i<buffer_ptr; i++) {
            newArray[i] = charArray[i];
        };
        kfree(charArray);
        charArray = newArray;
        printk(KERN_INFO "ioctl_char_dev: array content - %s\n", charArray);
    default:
        break;
    }
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = simple_char_open,
    .release = simple_char_release,
    .read = dev_read,
    .write = dev_write,
    .unlocked_ioctl = mon_ioctl
};

static int __init simple_char_init(void) {
    dev_t dev_num;
    int regval;

    printk(KERN_INFO "ioctl_char_dev: Buffer size: %d\n", size);

    printk(KERN_INFO "ioctl_char_dev: ioctl option MAX_SZ: %d", MAX_SZ);
    printk(KERN_INFO "ioctl_char_dev: ioctl option CUR_SZ: %d", CUR_SZ);
    printk(KERN_INFO "ioctl_char_dev: ioctl option INC_SZ: %d", INC_SZ);

    charArray = kmalloc(size * sizeof(char), GFP_KERNEL);
    if (charArray == NULL) {
        return -ENOMEM;
    }

    for (int i = 0; i<size; i++) {
        charArray[i]=' ';
    };

    regval = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    
    if (regval < 0) {
        printk(KERN_ALERT "ioctl_char_dev: Memory allocation for major number failed\n");
        return regval;
    }

    major_num = MAJOR(dev_num);
    printk(KERN_INFO "ioctl_char_dev: Initialized with Major number %d\n", major_num);

    cdev_init(&le_cdev, &fops);
    regval = cdev_add(&le_cdev, dev_num, 1);
    
    if (regval < 0) {
        printk(KERN_ALERT "ioctl_char_dev: Failed to load device\n");
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
