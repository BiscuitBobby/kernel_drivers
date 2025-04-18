#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/spinlock.h>

// --- Configuration ---
#define SRD_DEVICE_NAME "simple_ramdisk"
#define SRD_CAPACITY_MB 16   // Define RAM disk size in MiB
#define SRD_SECTOR_SIZE 512
// Calculate capacity in 512-byte sectors
#define SRD_SECTORS (SRD_CAPACITY_MB * 1024 * 1024 / SRD_SECTOR_SIZE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitBobby");
MODULE_DESCRIPTION("Simple RAM Disk Block Driver");

// Forward declaration for submit_bio
static void srd_submit_bio(struct bio *bio);

// Device specific structure
struct simple_ramdisk {
    struct gendisk *gd;        // The generic disk structure
    unsigned char *data;       // Pointer to the allocated RAM buffer
    size_t size;               // Size of the RAM buffer in bytes
    spinlock_t lock;           // Lock to protect buffer access
};

// Global storage for our single device instance and major number
static struct simple_ramdisk *srd_dev;
static int srd_major;

// Block device operations
static const struct block_device_operations srd_ops = {
    .owner = THIS_MODULE,
    // Add .open/.release if needed for more complex state management
    // .open = srd_open,
    // .release = srd_release,
    .submit_bio = srd_submit_bio, // Main I/O handler
};

// --- I/O Handling ---
static void srd_handle_bio(struct simple_ramdisk *dev, struct bio *bio)
{
    struct bvec_iter iter = bio->bi_iter; // Get the iterator from the BIO
    sector_t sector_off = iter.bi_sector; // Starting sector
    size_t dev_offset = sector_off * SRD_SECTOR_SIZE; // Byte offset in our RAM buffer

    do {
        struct bio_vec bvec = bio_iter_iovec(bio, iter); // Get current bio_vec segment
        size_t len = bvec.bv_len;
        unsigned char *ram_addr;
        unsigned char *bio_addr;

        // Check bounds for this segment
        if (dev_offset + len > dev->size) {
            printk("%s: Access beyond end of device (sector %llu, offset %zu, len %zu > size %zu)\n",
                   SRD_DEVICE_NAME, (unsigned long long)sector_off, dev_offset, len, dev->size);
            bio->bi_status = BLK_STS_IOERR;
            break; // Error for this segment (and thus BIO)
        }

        // Get kernel virtual address for the BIO's page
        bio_addr = kmap_local_page(bvec.bv_page) + bvec.bv_offset;
        ram_addr = dev->data + dev_offset; // Address in our RAM buffer

        // --- Perform Read/Write/Discard ---
        spin_lock(&dev->lock);
        switch (bio_op(bio)) {
            case REQ_OP_READ:
                memcpy(bio_addr, ram_addr, len);
                printk(KERN_DEBUG "%s: Read %zu bytes at offset %zu\n", SRD_DEVICE_NAME, len, dev_offset);
                break;
            case REQ_OP_WRITE:
                memcpy(ram_addr, bio_addr, len);
                printk(KERN_DEBUG "%s: Write %zu bytes at offset %zu\n", SRD_DEVICE_NAME, len, dev_offset);
                break;
            case REQ_OP_DISCARD:
            case REQ_OP_WRITE_ZEROES:
                memset(ram_addr, 0, len);
                printk(KERN_DEBUG "%s: Discard/Zero %zu bytes at offset %zu\n", SRD_DEVICE_NAME, len, dev_offset);
                break;
            default:
                pr_warn("%s: Unsupported BIO operation: %d\n", SRD_DEVICE_NAME, bio_op(bio));
                spin_unlock(&dev->lock);
                kunmap_local(bio_addr); // Unmap before erroring out
                bio->bi_status = BLK_STS_IOERR; // Mark BIO with error
                goto bio_loop_end; // Exit loop on error
        }
        spin_unlock(&dev->lock);

        // Clean up mapping for this segment
        kunmap_local(bio_addr);

        // Move to the next offset in our RAM buffer AND advance iterator
        dev_offset += len;
        bio_advance_iter_single(bio, &iter, len); // Advance the iterator

    } while (iter.bi_size > 0); // Continue while there's more data in the BIO

bio_loop_end:
    // If status wasn't already set to error during the loop, mark as OK
    if (bio->bi_status == BLK_STS_OK) {
        bio->bi_status = BLK_STS_OK; // No error encountered
    }
    return;
}

// submit_bio callback
static void srd_submit_bio(struct bio *bio) {
    struct simple_ramdisk *dev = bio->bi_bdev->bd_disk->private_data;

    // Handle the actual data transfer or operation
    srd_handle_bio(dev, bio);

    // Signal completion of the BIO
    bio_endio(bio);
}


// --- Device Creation & Deletion ---

// Function to create the block device resources
static int create_simple_ramdisk(struct simple_ramdisk **dev_ptr)
{
    struct simple_ramdisk *dev;
    int ret = -ENOMEM; // Assume memory allocation failure initially

    // 1. Allocate our device structure
    dev = kmalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        printk("%s: Failed to allocate device structure\n", SRD_DEVICE_NAME);
        return -ENOMEM;
    }
    memset(dev, 0, sizeof(*dev));
    spin_lock_init(&dev->lock);

    // 2. Allocate the RAM buffer (using vmalloc for potentially large sizes)
    dev->size = (size_t)SRD_CAPACITY_MB * 1024 * 1024;
    dev->data = vzalloc(dev->size); // vzalloc zeros the memory
    if (!dev->data) {
        printk("%s: Failed to allocate RAM buffer (%zu bytes)\n", SRD_DEVICE_NAME, dev->size);
        kfree(dev);
        return -ENOMEM;
    }
    pr_info("%s: Allocated RAM buffer of %d MiB\n", SRD_DEVICE_NAME, SRD_CAPACITY_MB);

    // 3. Configure Queue Limits
    //    Physical block size often matches logical for simple RAM disks
    struct queue_limits lim = {
        .logical_block_size     = SRD_SECTOR_SIZE,
        .physical_block_size    = SRD_SECTOR_SIZE, // Can be PAGE_SIZE too
        .io_min                 = SRD_SECTOR_SIZE,
        .io_opt                 = PAGE_SIZE, // Optimal I/O is often page size
        .max_sectors            = UINT_MAX, // No real hardware limit
        .max_hw_discard_sectors = UINT_MAX, // Can discard everything
        .max_write_zeroes_sectors = UINT_MAX, // Can write zeroes to everything
    };


    // 4. Allocate Gendisk structure
    //    blk_alloc_disk implicitly creates and sets up the request queue
    dev->gd = blk_alloc_disk(&lim, NUMA_NO_NODE);
    if (IS_ERR(dev->gd)) {
        printk("%s: Failed to allocate gendisk\n", SRD_DEVICE_NAME);
        ret = PTR_ERR(dev->gd);
        goto cleanup_buffer;
    }

    // 5. Initialize Gendisk fields
    dev->gd->major = srd_major;
    dev->gd->first_minor = 0;     // First minor number for this major
    dev->gd->minors = 1;          // Only one device (no partitions)
    dev->gd->fops = &srd_ops;
    dev->gd->private_data = dev;  // Link back to our structure
    snprintf(dev->gd->disk_name, DISK_NAME_LEN, "srd%d", 0); // e.g., srd0

    // 6. Set Capacity
    set_capacity(dev->gd, SRD_SECTORS);
    pr_info("%s: Disk capacity set to %llu sectors (%d MiB)\n",
           SRD_DEVICE_NAME, (unsigned long long)SRD_SECTORS, SRD_CAPACITY_MB);

    // 7. Add Gendisk to System
    ret = add_disk(dev->gd);
    if (ret) {
        printk("%s: Failed to add disk: %d\n", SRD_DEVICE_NAME, ret);
        goto cleanup_disk_obj;
    }

    pr_info("%s: Disk '%s' added successfully\n", SRD_DEVICE_NAME, dev->gd->disk_name);
    *dev_ptr = dev; // Return the successfully created device
    return 0; // Success

// --- Error Handling Cleanup ---
cleanup_disk_obj:
    put_disk(dev->gd); // Release gendisk resources (including queue)
cleanup_buffer:
    vfree(dev->data);
    kfree(dev);
    *dev_ptr = NULL;
    return ret;
}

// Function to delete the block device resources
static void delete_simple_ramdisk(struct simple_ramdisk *dev)
{
    if (!dev) return;

    if (dev->gd) {
        del_gendisk(dev->gd); // Remove from system first
        put_disk(dev->gd);    // Then release resources
    }
    if (dev->data) {
        vfree(dev->data);     // Free the RAM buffer
    }
    kfree(dev);               // Free the device structure
    pr_info("%s: Device resources released\n", SRD_DEVICE_NAME);
}

// --- Module Init & Exit ---
static int __init srd_init(void)
{
    int ret = 0;

    // Register the block device major number
    srd_major = register_blkdev(0, SRD_DEVICE_NAME); // Request dynamic major
    if (srd_major < 0) {
        printk("%s: Failed to register block device: %d\n",
               SRD_DEVICE_NAME, srd_major);
        return srd_major;
    }
    pr_info("%s: Registered with major number %d\n", SRD_DEVICE_NAME, srd_major);

    // Create the actual RAM disk device
    ret = create_simple_ramdisk(&srd_dev);
    if (ret) {
        unregister_blkdev(srd_major, SRD_DEVICE_NAME);
        return ret;
    }

    pr_info("%s: Module loaded successfully\n", SRD_DEVICE_NAME);
    return 0;
}

static void __exit srd_exit(void)
{
    // Delete the block device resources
    delete_simple_ramdisk(srd_dev);

    // Unregister the major number
    if (srd_major > 0) {
        unregister_blkdev(srd_major, SRD_DEVICE_NAME);
        pr_info("%s: Unregistered major number %d\n", SRD_DEVICE_NAME, srd_major);
    }

    pr_info("%s: Module unloaded\n", SRD_DEVICE_NAME);
}

module_init(srd_init);
module_exit(srd_exit);