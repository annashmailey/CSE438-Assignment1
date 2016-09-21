/*
Names: Anna Bailey, Daniel Wong
Group: #3
Class: CSE 438 - T&TH @ 4:30pm
Description:   Assignment 1 
File:  sharedQueue.c
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>

#include <linux/init.h>
#include <linux/moduleparam.h>

#define BUS_IN 	 		"bus_in_q"
#define BUS_OUT_1		"bus_out_q1"
#define BUS_OUT_2		"bus_out_q2"
#define BUS_OUT_3		"bus_out_q3"

static dev_t device_number[4];      /* Allotted device number */
static char device_name[4][10] = {BUS_IN, BUS_OUT_1, BUS_OUT_2, BUS_OUT_3};
static struct class *device_class[4];
static struct bus_dev* bus_devp[4];
static struct device *bus_device[4];
 
/* Structure of message */
struct msg {
	int msg_id;      // global sequence number of message
	int source_id;	
	int dest_id;
	struct timespec start_time;  // accumulated queueing time 
	char msgString[80];  // arbitrary character message with uniformly distributed length of 10 to 80 
};

/* Device structure */
struct bus_dev {
	struct cdev cdev;               /* The cdev structure */
	int bus_id;
	char name[20];				/* Device name */
	struct msg* msg_q[10];
	int count; 
	int head;
	int tail;
};



/* Open driver */
int bus_driver_open(struct inode *inode, struct file *file)
{
	struct bus_dev *bus_devp;
	
	/* Get the per-device structure that contains this cdev */
	bus_devp = container_of(inode->i_cdev, struct bus_dev, cdev);

	/* Easy access to bus_devp from rest of entry points */
	file->private_data = bus_devp;
	printk("\n %s is opening \n", bus_devp->name);
	return 0;
}

/* Release driver */
int bus_driver_release(struct inode *inode, struct file *file)
{
	struct bus_dev *bus_devp = file->private_data;
	printk("\n %s is closing \n", bus_devp->name);
	return 0;
}

/* Write to driver */
ssize_t bus_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	/* Enqueue a message to the device. If the queue is full, -1 is returned
		and errno is set to EINVAL.                                */
	int ret = 0;
	struct bus_dev *bus_devp = file->private_data;

	/* confirmation message */
	printk("Device %d received \n", bus_devp->bus_id); 

	if( bus_devp->count < 10)
	{
		struct msg* new_msg = kmalloc(sizeof(struct msg), GFP_KERNEL);
		ret = copy_from_user((char*)new_msg, buf, sizeof(struct msg));
		
		if (ret < 0)
			return -EINVAL;

		bus_devp->msg_q[bus_devp->head] = new_msg;  // or bus_devp->tail
		bus_devp->tail++;  // or head?
		bus_devp->tail = (bus_devp->tail) % 10;  // or head?
		bus_devp->count++;
	}
	else // if((bus_devp->count) >= 10)
		return -EINVAL; 	//ret = -1;

	return ret;  // OR    return sizeof(struct msg);
}

/* Read to driver */
ssize_t bus_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	/* To dequeue a message (with the valid queueing time) from the device
		and copy it to the user buffer. If the queue is empty, -1 is returned
		and errno is set to EINVAL				*/
	int ret = 0;
	struct bus_dev *bus_devp = file->private_data;
	
	// Remove from queue
	if(bus_devp->count > 0)
	{
		ret = copy_to_user(buf, (char*)bus_devp->msg_q[bus_devp->tail], sizeof(struct msg));
		
		//check if empty
		if(ret < 0)
		{
			return -EINVAL;
		}

		kfree(bus_devp->msg_q[bus_devp->head]); // or msg_q[bus_devp->tail]

		bus_devp->head++;
		bus_devp->head %= 10;
		bus_devp->count--;
		printk(KERN_DEBUG "MSG was read");
	}
	
	return ret;  //  OR    sizeof(struct msg);
	
}

/* File operations structure defined in linux/fs.h */
static struct file_operations bus_fops = {
	.owner		= THIS_MODULE,			/* Owner */
	.open 		= bus_driver_open,		/* Open method */
	.release	= bus_driver_release, 	/* Release method */
	.write 		= bus_driver_write,		/* Write method */
	.read 		= bus_driver_read,		/* Read method */
};

/* Driver Initialization */
int __init bus_driver_init(void)
{
	int ret, i;
	for (i = 0; i < 4; i++)
	{
		/* Request dynamic allocation of a device major number */	
		if (alloc_chrdev_region(&device_number[i], 0, 1, device_name[i]) < 0)
		{
			printk(KERN_DEBUG "Can't register device %s\n", device_name[i]); 
			return -1;
		}
		
		/* Populate sysfs entries */
		device_class[i] = class_create(THIS_MODULE, device_name[i]);

		/* Allocate memory for the per-device structure */
		bus_devp[i] = kmalloc(sizeof(struct bus_dev), GFP_KERNEL);
		if (!bus_devp[i])
		{
			printk("Bad Kmalloc\n");
			return -ENOMEM;
		}

		/* Connect the file operations with the cdev */
		cdev_init(&bus_devp[i]->cdev, &bus_fops);
		bus_devp[i]->cdev.owner = THIS_MODULE;
		
		/* Connect the major/minor number to the cdev */
		ret = cdev_add(&bus_devp[i]->cdev, device_number[i], 1);
		if (ret)
		{
			printk("Bad cdev\n");
			return ret;
		}

		/* Send uevents to udev, so it'll create /dev nodes */
		bus_device[i] = device_create(device_class[i], NULL, MKDEV(MAJOR(device_number[i]), 0), NULL, device_name[i]);

		/* Memory initialization per device */
		bus_devp[i]->bus_id = i;
		strcpy(bus_devp[i]->name, device_name[i]);
		bus_devp[i]->count = 0;
		bus_devp[i]->head = 0;
		bus_devp[i]->tail = 0;


	} // end of for loop (for 4 devices)
	printk("Bus drivers initialization successful\n");	
	return 0;
}

/* Driver exit */
void __exit bus_driver_exit(void)
{	
	int i;
	for(i=0; i < 4; i++)
	{
		/* Release the major number for device */
		unregister_chrdev_region((device_number[i]), 1);

		/* Destroy device */
		device_destroy(device_class[i], MKDEV(MAJOR(device_number[i]), 0));
		cdev_del(&bus_devp[i]->cdev);
		
		/* 	
		while (bus devp[i]->count > 0)
		{
			MIGHT need to flush buffers - linked list stuff
			bus devp[i]->count--;
			bus devp[i]->head;
		} */
		
		
		kfree(bus_devp[i]);

		/* Destroy driver class */
		class_destroy(device_class[i]);

	} // end of for loop (for 4 devices)
	printk ("Bus drivers successfully destroyed\n");
}

module_init(bus_driver_init);
module_exit(bus_driver_exit);
MODULE_LICENSE("GPL v2");
