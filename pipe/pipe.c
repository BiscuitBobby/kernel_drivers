#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "char_pipe_dev"
#define BUFFER_SIZE 8192

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Circular buffer pipe driver");

static int major_num;
static struct cdev le_cdev;

typedef struct {
    char buffer[BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    wait_queue_head_t read_queue;
    wait_queue_head_t write_queue;
} circular_buffer_t;

static circular_buffer_t cb;

static void circular_buffer_init(circular_buffer_t *cb) {
    cb->count = 0;
    init_waitqueue_head(&cb->read_queue);
    init_waitqueue_head(&cb->write_queue);
}

static ssize_t circular_buffer_write(circular_buffer_t *cb, const char *user_buffer, size_t count) {
    size_t written = 0;

    while (written < count) {
        while (cb->count == BUFFER_SIZE) {
            if (wait_event_interruptible(cb->write_queue, cb->count < BUFFER_SIZE)) {
                return -ERESTARTSYS;
            }
        }

        if (copy_from_user(&cb->buffer[cb->head], &user_buffer[written], 1)) {
            return -EFAULT;
        }
        cb->head = (cb->head + 1) % BUFFER_SIZE;
        cb->count++;
        written++;

        wake_up_interruptible(&cb->read_queue);
    }

    return written;
}

static ssize_t circular_buffer_read(circular_buffer_t *cb, char *user_buffer, size_t count) {
    size_t read = 0;

    while (read < count) {
        while (cb->count == 0) {
            if (wait_event_interruptible(cb->read_queue, cb->count > 0)) {
                return -ERESTARTSYS;
            }
        }

        if (copy_to_user(&user_buffer[read], &cb->buffer[cb->tail], 1)) {
            return -EFAULT;
        }
        cb->tail = (cb->tail + 1) % BUFFER_SIZE;
        cb->count--;
        read++;

        wake_up_interruptible(&cb->write_queue);
    }

    return read;
}

static int simple_char_open(struct inode *inode, struct file *instance) {
    printk(KERN_INFO "char_pipe_dev: opened device\n");
    return 0;
}

ssize_t dev_read(struct file *FILE, char *user_buffer, size_t count, loff_t *offset) {
    ssize_t result = circular_buffer_read(&cb, user_buffer, count);
    if (result < 0) {
        return result;
    }
    printk(KERN_INFO "char_pipe_dev: read %zu bytes\n", result);
    return result;
}

ssize_t dev_write(struct file *File, const char *user_buffer, size_t count, loff_t *offset){
    ssize_t result = circular_buffer_write(&cb, user_buffer, count);
    if (result < 0) {
        return result;
    }
    printk(KERN_INFO "char_pipe_dev: written %zu bytes\n", result);
    return result;
}

static int simple_char_release(struct inode *inode, struct file *instance) {
    printk(KERN_INFO "char_pipe_dev: driver closed\n");
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = simple_char_open,
    .release = simple_char_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init simple_char_init(void) {
    dev_t dev_num;
    int regval;

    printk(KERN_INFO "char_pipe_dev: Initializing device\n");
    circular_buffer_init(&cb);

    regval = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    
    if (regval < 0) {
        printk(KERN_ALERT "char_pipe_dev: Memory allocation for major number failed\n");
        return regval;
    }

    major_num = MAJOR(dev_num);
    printk(KERN_INFO "char_pipe_dev: Initialized with Major number %d\n", major_num);

    cdev_init(&le_cdev, &fops);
    regval = cdev_add(&le_cdev, dev_num, 1);
    
    if (regval < 0) {
        printk(KERN_ALERT "char_pipe_dev: Failed to load device\n");
        return regval;
    }
    
    return 0;
}

static void __exit simple_char_exit(void) {
    dev_t dev_num = MKDEV(major_num, 0);
    cdev_del(&le_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "char_pipe: Exited\n");
}

module_init(simple_char_init);
module_exit(simple_char_exit);
