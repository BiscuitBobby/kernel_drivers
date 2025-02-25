#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/fs.h>

#define MY_BLOCK_MINORS 1
#define MY_BLOCK_MAJOR 240
#define MY_BLKDEV_NAME "block_simp_dev"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Simple Block Device Driver");

static struct my_block_dev {
    struct gendisk *gd;
} dev;

static int create_block_device(struct my_block_dev *dev)
{
    int ret;
    dev->gd = blk_alloc_disk(NULL, NUMA_NO_NODE);
    if (!dev->gd) {
        printk(KERN_ERR "block_simp_dev: Failed to allocate gendisk\n");
        return -ENOMEM;
    }

    ret = add_disk(dev->gd);
    if (ret) {
        printk(KERN_ERR "block_simp_dev: Failed to add disk\n");
        return ret;
    }

    return 0;
}

static void delete_block_device(struct my_block_dev *dev)
{
    if (dev->gd)
        del_gendisk(dev->gd);
}

static int my_block_init(void)
{
    int reg_stat;

    reg_stat = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    if (reg_stat < 0) {
        printk(KERN_ERR "block_simp_dev: Failed to register block device\n");
        return reg_stat;
    }

    if (create_block_device(&dev) < 0) {
        unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "block_simp_dev: Block device registered\n");
    return 0;
}

static void my_block_exit(void)
{
    delete_block_device(&dev);
    unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    printk(KERN_INFO "block_simp_dev: Block device unregistered\n");
}

module_init(my_block_init);
module_exit(my_block_exit);
