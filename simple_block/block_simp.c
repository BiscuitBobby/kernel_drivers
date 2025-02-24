#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/fs.h>

#define MY_BLOCK_MAJOR 240
#define MY_BLKDEV_NAME "block_simp_dev"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Simple Block Device Driver");

static int my_block_init(void)
{
    int reg_stat;

    reg_stat = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    if (reg_stat < 0) {
        printk(KERN_ERR "block_simp_dev: Failed to register block device\n");
        return reg_stat;
    }

    printk(KERN_INFO "block_simp_dev: Block device registered\n");
    return 0;
}

static void my_block_exit(void)
{
    unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);  //returns void value
    printk(KERN_INFO "block_simp_dev: Block device unregistered\n");
}

module_init(my_block_init);
module_exit(my_block_exit);
