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
#include <linux/fs.h> // file_operations
#include <linux/slab.h>         /* kmalloc() */
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Anup9401");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{

	struct aesd_dev *dev; /* device information */
	PDEBUG("open\n");

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release\n");
	/**
	 * TODO: handle release
	 */
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	ssize_t retval = 0;
	struct aesd_dev *dev = filp->private_data;
	struct aesd_circular_buffer *buffer = &(dev->buffer);
	struct aesd_buffer_entry *entry;
	size_t entry_offset_byte,size;
	PDEBUG("read %zu bytes with offset %lld\n",count,*f_pos);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	entry=aesd_circular_buffer_find_entry_offset_for_fpos(buffer, *f_pos, &entry_offset_byte);

	if(entry==NULL)
		goto out;

	size = entry->size - entry_offset_byte;

	if(count<size)
		size=count;

	if(copy_to_user(buf, (entry->buffptr)+entry_offset_byte, size)){
		retval = -EFAULT;
		goto out;
	}

	*f_pos += size;
	retval = size;
out:
	mutex_unlock(&dev->lock);
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	struct aesd_dev *dev = filp->private_data;
	size_t size = dev->entry.size;
	struct aesd_buffer_entry entryTemp;
	char *buffptr;
	PDEBUG("write %zu bytes with offset %lld\n",count,*f_pos);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	entryTemp.size=size+count;
	buffptr=kmalloc(entryTemp.size, GFP_KERNEL);

	if(!(buffptr))
		goto out;

	entryTemp.buffptr=buffptr;

	if(dev->entry.buffptr){
		memcpy(buffptr, dev->entry.buffptr, size);
		kfree(dev->entry.buffptr);
		dev->entry.buffptr=NULL;
		dev->entry.size=0;
	}

	if (copy_from_user(buffptr+size, buf, count)) {
		retval = -EFAULT;
		kfree(buffptr);
		goto out;
	}

	if(memchr(entryTemp.buffptr+size, '\n', count)){
		const char *buffptrRet=aesd_circular_buffer_add_entry(&(dev->buffer),&entryTemp);
		if(buffptrRet){
			kfree(buffptrRet);
		}
	}
	else{
		dev->entry.buffptr=entryTemp.buffptr;
		dev->entry.size=entryTemp.size;
	}

	retval=count;

out:
	mutex_unlock(&dev->lock);
	return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos,size=0;
	struct aesd_dev *dev = filp->private_data;
	uint8_t index;
	struct aesd_circular_buffer *buffer = &(dev->buffer);
	struct aesd_buffer_entry *entry;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	AESD_CIRCULAR_BUFFER_FOREACH(entry,buffer,index) {
		if(entry->buffptr)
			size=size+entry->size;
	}

	if(size!=0){
		newpos = fixed_size_llseek(filp, off, whence, size);
	}
	else{
		newpos = -EINVAL;
	}

	mutex_unlock(&dev->lock);
	return newpos;
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd,
		unsigned int write_cmd_offset)
{
	long retval = 0;
	struct aesd_dev *dev = filp->private_data;
	struct aesd_circular_buffer *buffer = &(dev->buffer);
	uint8_t i;
	size_t size=0;

	if((write_cmd<0) || (write_cmd>=AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED))
		return -EINVAL;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	if(buffer->entry[write_cmd].buffptr == NULL){
		retval = -EINVAL;
		goto out;
	}

	if(buffer->entry[write_cmd].size <= write_cmd_offset){
		retval = -EINVAL;
		goto out;
	}

	for(i=0; i<write_cmd; i++){
		size=size+(buffer->entry[i]).size;
	}

	filp->f_pos = size + write_cmd_offset;

out:
	mutex_unlock(&dev->lock);
	return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = -ENOTTY;
	struct aesd_seekto seekto;

	if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return retval;
	if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return retval;

	switch(cmd)
	{
		case AESDCHAR_IOCSEEKTO:
			if(copy_from_user(&seekto, (const void __user*) arg, sizeof(seekto))){
				retval = -EFAULT;
			}
			else{
				retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
			}
			break;
	}

	return retval;
}

struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
	.llseek =   aesd_llseek,
	.unlocked_ioctl = aesd_ioctl,
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

	aesd_circular_buffer_init(&(aesd_device.buffer));  //initialize circular buffer
	aesd_device.entry.buffptr=NULL;     //initialize entry buffptr
	aesd_device.entry.size=0;           //initialize entry size
	mutex_init(&(aesd_device.lock));    //initialize mutex

	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);
	uint8_t index;
	struct aesd_buffer_entry *entry;

	cdev_del(&aesd_device.cdev);

	AESD_CIRCULAR_BUFFER_FOREACH(entry,&(aesd_device.buffer),index) {
		if(entry->buffptr)
			kfree(entry->buffptr);
	}

	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
