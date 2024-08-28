/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("sbaidachni"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;


loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    size_t totalSize = 0;
    loff_t updated_fpos = 0;

    struct aesd_dev* dev = (struct aesd_dev* )filp->private_data;
    mutex_lock_interruptible(&dev->lock);

    size_t index = 0;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, dev->circular_buffer, index)
    {
        totalSize += entry->size;
    }

    updated_fpos = fixed_size_llseek(filp, offset, whence, totalSize);

    filp->f_pos = updated_fpos;

    mutex_unlock(&dev->lock);

    return updated_fpos;
}

long aesd_ioctl(struct file *filp, unsigned int kind, unsigned long arg)
{
    long retval = 0;
    
    struct aesd_dev* dev = (struct aesd_dev* )filp->private_data;
    mutex_lock_interruptible(&dev->lock);

    switch(kind)
    {
        case AESDCHAR_IOCSEEKTO:
        {
            struct aesd_seekto seekto;
            size_t totalSize = 0;

            copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto));

            size_t i=0;

            while(i<seekto.write_cmd)
            {
                totalSize += dev->circular_buffer->entry[i].size;
                i++;
            }

            totalSize += seekto.write_cmd_offset;

            filp->f_pos = totalSize;
            break;
        }
        default:
        {
            retval = -EINVAL;
        }
    }

    mutex_unlock(&dev->lock);

    return retval;
}


int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
	
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    size_t entry_offset_byte_rtn = 0;
    size_t read_count = 0;

    struct aesd_dev* dev = (struct aesd_dev* )filp->private_data;
    mutex_lock_interruptible(&dev->lock);

    struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->circular_buffer, *f_pos, &entry_offset_byte_rtn);
	
    if (entry == NULL)
    {	
	    retval = 0;
    }
    else
    {
        read_count = entry->size - entry_offset_byte_rtn;
	    if (read_count > count)
        {
            read_count = count;
        }
	    copy_to_user(buf, (char *)(entry->buffptr + entry_offset_byte_rtn), read_count);

        *f_pos += read_count;
	    retval = read_count;
    }
	
    mutex_unlock(&dev->lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    struct aesd_dev* dev = filp->private_data;
    char* aux_buff = NULL;
    size_t buf_size = 0;
	
    mutex_lock_interruptible(&dev->lock);

    if(dev->entry == NULL)
    {
	    dev->entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);

	    dev->entry->buffptr = kmalloc(count, GFP_KERNEL);

	    copy_from_user((char *)dev->entry->buffptr, buf, count);

	    dev->entry->size = count;
    }
    else
    {
	    buf_size = dev->entry->size + count;
	    aux_buff = (char *)dev->entry->buffptr;
	    dev->entry->buffptr = kmalloc(buf_size, GFP_KERNEL);

        copy_from_user((char *)(dev->entry->buffptr + dev->entry->size), buf, count);
	    memcpy((char *)dev->entry->buffptr, aux_buff, dev->entry->size);
	    dev->entry->size = buf_size;
	    kfree(aux_buff);
    }

    retval = count;

    if(dev->entry->size > 0 && strnstr(dev->entry->buffptr, "\n", dev->entry->size) != NULL)
    {
    	aesd_circular_buffer_add_entry(dev->circular_buffer, dev->entry);
	    kfree(dev->entry);
	    dev->entry = NULL;
    }

    *f_pos = *f_pos + retval;
    
    mutex_unlock(&dev->lock);

    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =           aesd_llseek,
    .unlocked_ioctl =   aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    mutex_init(&aesd_device.lock);
    aesd_device.circular_buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    aesd_circular_buffer_init(aesd_device.circular_buffer);
    aesd_device.entry = NULL;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    size_t idx;
    struct aesd_buffer_entry* entry_ptr;
    AESD_CIRCULAR_BUFFER_FOREACH(entry_ptr, aesd_device.circular_buffer, idx)
    {
	    if(entry_ptr->buffptr != NULL)
		    kfree(entry_ptr->buffptr);	
    }

    kfree(aesd_device.circular_buffer);
    kfree(aesd_device.entry);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);