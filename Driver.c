/* ----------------------------------------------- DRIVER squeue --------------------------------------------------
 *
 *   Basic Squeue driver which queues and dequeues Data
 *
 *     ----------------------------------------------------------------------------------------------------------------*/

//---------------------------------------------------------------------------------------------------------------
// Included Libraries

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
#include<linux/init.h>
#include<linux/moduleparam.h>
#include <linux/semaphore.h>

//---------------------------------------------------------------------------------------------------------------

MODULE_LICENSE("GPL");

//---------------------------------------------------------------------------------------------------------------
// Devices to registered

#define DEVICE	"squeue"  	// device input queue to be created and registered
#define DEVICE1	"squeue1"  	// device input queue to be created and registered
#define DEVICE2	"squeue2"  	// device input queue to be created and registered
#define DEVICE3	"squeue3"  	// device input queue to be created and registered
#define DEVICE4	"squeue4"  	// device input queue to be created and registered

//---------------------------------------------------------------------------------------------------------------
//Time Stamp Counter

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}


/* Message Structure */

struct User_Message{
	int Sender_Id;
	int Receiver_Id;
	int Message_Id;
	uint64_t  Time_Stamp;
	uint64_t  Queue_Time;
	char Message[80];
};

/* Device Structure */

struct squeue_dev {
	struct cdev cdev;
	struct User_Message *Message_Pointer[10];              	// Array of Pointers
	int Head_Pointer;					// Pointer Represnting Head
	int Tail_Pointer;					// Pointer Representing Tail
	int Data_Count;						// Data present in Queue
	struct semaphore sem;					// Semaphore
} *squeue_devp;

struct squeue_dev *squeue_devp1;
struct squeue_dev *squeue_devp2;
struct squeue_dev *squeue_devp3;


static dev_t squeue_dev_number;      		// Allotted device number
struct class *squeue_dev_class;          	// Tie with the device model
static struct device *squeue_dev_device;
static struct device *squeue_dev_device1;
static struct device *squeue_dev_device2;
static struct device *squeue_dev_device3;

//---------------------------------------------------------------------------------------------------------------
//Open Method
int squeue_driver_open(struct inode *inode, struct file *file)
{
	struct squeue_dev *squeue_devp;

	//printk(KERN_ALERT"%p\n",squeue_devp);

	/* Get the per-device structure that contains this cdev */
	squeue_devp = container_of(inode->i_cdev, struct squeue_dev, cdev);

	// Initializaing the Semaphore
	sema_init(&squeue_devp->sem,1);

	/* Easy access to cmos_devp from rest of the entry points */
	file->private_data =squeue_devp;

	//printk("Opening the Device %d \n",iminor(inode));

	//Initialization of Queue
	memset(squeue_devp-> Message_Pointer, 0, sizeof(squeue_devp->Message_Pointer));
	squeue_devp->Head_Pointer = 0;
	squeue_devp->Tail_Pointer = 0;
	squeue_devp->Data_Count   = 0;

	return 0;
}

//---------------------------------------------------------------------------------------------------------------
// Realease Method
int squeue_driver_release(struct inode *inode, struct file *file)
{
	printk("Realeasing file does nothing\n");
	return 0;
}

//---------------------------------------------------------------------------------------------------------------
// Write Method
ssize_t squeue_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct squeue_dev *squeue_devp = file->private_data;
	struct User_Message *UMessage_Struct_Pointer; // Pointer of struct in User Space passed by User
	struct User_Message *KMessage_Struct_Pointer; // Pointer of struct stored in kernel space
	int res;

	//Type casting from char to stuct User_Message Pointer
	UMessage_Struct_Pointer = (struct User_Message *)buf;

	//Locking the File
	down(&squeue_devp->sem);
	// Implementation of Queue Logic
		if(squeue_devp->Data_Count >= 10){
			up(&squeue_devp->sem);
			return -1;
		}

		else{
			//Allocating Memory for incoming message
			KMessage_Struct_Pointer = kmalloc(sizeof(struct User_Message),GFP_KERNEL);

			//Copying memory from User space to Kernenl Space
			res = copy_from_user(KMessage_Struct_Pointer,UMessage_Struct_Pointer, sizeof(struct User_Message));
			if(res < 0){
				up(&squeue_devp->sem);
				return -1;
			}
			squeue_devp->Message_Pointer[squeue_devp->Head_Pointer] = KMessage_Struct_Pointer;

			//Adding Time Stamp
			KMessage_Struct_Pointer->Time_Stamp = rdtsc();

			//Incrementing Value of Head Pointer and DataCount
			if (squeue_devp->Head_Pointer < 9){
				squeue_devp->Head_Pointer = squeue_devp->Head_Pointer + 1;
				squeue_devp->Data_Count = squeue_devp->Data_Count + 1;
			}
			else{
				squeue_devp->Head_Pointer = 0;
				squeue_devp->Data_Count = squeue_devp->Data_Count + 1;
			}
			// Unlocking file
			up(&squeue_devp->sem);
	// Implementation of Queue Logic Completed

	}

	return 0;
}

//---------------------------------------------------------------------------------------------------------------
// Read Method
ssize_t squeue_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct squeue_dev *squeue_devp = file->private_data;
	struct User_Message *UMessage_Struct_Pointer; // Pointer of struct in User Space passed by User
	struct User_Message *KMessage_Struct_Pointer; // Pointer of struct in Kernal Space
	int res;

	UMessage_Struct_Pointer = (struct User_Message *)buf; //type casting from char to stuct User_Message Pointer

	//Locking the File
	down(&squeue_devp->sem);
	// Implementation of Dequeue Logic
		if(squeue_devp->Data_Count == 0){
			up(&squeue_devp->sem);
			return -1;
		}
		else{
			// Retrieving Message pointer
			KMessage_Struct_Pointer = squeue_devp->Message_Pointer[squeue_devp->Tail_Pointer];

			//Calculating and storing Queue Time
			KMessage_Struct_Pointer->Queue_Time = KMessage_Struct_Pointer->Queue_Time + (rdtsc() - KMessage_Struct_Pointer->Time_Stamp);

			// Copying from kernel memory to User memory
			res = copy_to_user(UMessage_Struct_Pointer, KMessage_Struct_Pointer, sizeof(struct User_Message));
			if(res < 0){
				up(&squeue_devp->sem);
				return -1;
			}

			//Incrementing Tail Pointer and Decrementing Data Count
			if (squeue_devp->Tail_Pointer < 9){
				squeue_devp->Tail_Pointer = squeue_devp->Tail_Pointer + 1;
				squeue_devp->Data_Count = squeue_devp->Data_Count - 1;
			}
			else{
				squeue_devp->Tail_Pointer = 0;
				squeue_devp->Data_Count = squeue_devp->Data_Count - 1;
			}
			// Unlocking the File
			up(&squeue_devp->sem);
		}
	//Implementation of Dequeue Logic Completed


	//Releasing Message Memory in Kernel Space
	kfree(KMessage_Struct_Pointer);

	return 0;

}

//---------------------------------------------------------------------------------------------------------------
// Registrering the Device

static struct file_operations squeue_fops = {
    .owner		= THIS_MODULE,  	     // Owner
    .open		= squeue_driver_open,        // Open method
    .release		= squeue_driver_release,     // Release method
    .write		= squeue_driver_write,       // Write method
    .read		= squeue_driver_read,        // Read method
};

int __init squeue_device_init(void)
{
	int ret;
	//printk(KERN_ALERT "Starting Registration of Squeue devices\n");

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&squeue_dev_number, 0, 4, DEVICE) < 0) {
			printk(KERN_ALERT "Can't register devices\n");
			return -1;
	}

	/* Populate sysfs entries */
	squeue_dev_class = class_create(THIS_MODULE, DEVICE);

	/* Allocate memory for the per-device structure */
	squeue_devp = kmalloc(sizeof(struct squeue_dev), GFP_KERNEL);
	squeue_devp1 = kmalloc(sizeof(struct squeue_dev), GFP_KERNEL);
	squeue_devp2 = kmalloc(sizeof(struct squeue_dev), GFP_KERNEL);
	squeue_devp3 = kmalloc(sizeof(struct squeue_dev), GFP_KERNEL);

	//printk(KERN_ALERT"Memory Allocated to all devices\n");

	/* Connect the file operations with the cdev */
	cdev_init(&squeue_devp->cdev, &squeue_fops);
	cdev_init(&squeue_devp1->cdev, &squeue_fops);
	cdev_init(&squeue_devp2->cdev, &squeue_fops);
	cdev_init(&squeue_devp3->cdev, &squeue_fops);

	squeue_devp->cdev.owner  = THIS_MODULE;
	squeue_devp1->cdev.owner = THIS_MODULE;
	squeue_devp2->cdev.owner = THIS_MODULE;
	squeue_devp3->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&squeue_devp->cdev, MKDEV(MAJOR(squeue_dev_number), 0), 4);
	ret = cdev_add(&squeue_devp1->cdev,MKDEV(MAJOR(squeue_dev_number), 1), 4);
	ret = cdev_add(&squeue_devp2->cdev,MKDEV(MAJOR(squeue_dev_number), 2), 4);
	ret = cdev_add(&squeue_devp3->cdev,MKDEV(MAJOR(squeue_dev_number), 3), 4);

	if (ret) {
		printk(KERN_ALERT"Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	squeue_dev_device =  device_create(squeue_dev_class, NULL, MKDEV(MAJOR(squeue_dev_number), 0), NULL, DEVICE);
	squeue_dev_device1 = device_create(squeue_dev_class, NULL, MKDEV(MAJOR(squeue_dev_number), 1), NULL, DEVICE1);
	squeue_dev_device2 = device_create(squeue_dev_class, NULL, MKDEV(MAJOR(squeue_dev_number), 2), NULL, DEVICE2);
	squeue_dev_device3 = device_create(squeue_dev_class, NULL, MKDEV(MAJOR(squeue_dev_number), 3), NULL, DEVICE3);

	printk(KERN_ALERT"Devices Successfully Added\n");

	return 0;
}

//---------------------------------------------------------------------------------------------------------------
// Opening the Device

void __exit squeue_device_exit(void)
{
	/* Release the major number */
	unregister_chrdev_region((squeue_dev_number),4);

	/* Destroy device */
	device_destroy (squeue_dev_class, MKDEV(MAJOR(squeue_dev_number), 0));
	device_destroy (squeue_dev_class, MKDEV(MAJOR(squeue_dev_number), 1));
	device_destroy (squeue_dev_class, MKDEV(MAJOR(squeue_dev_number), 2));
	device_destroy (squeue_dev_class, MKDEV(MAJOR(squeue_dev_number), 3));

	cdev_del(&squeue_devp->cdev);
	cdev_del(&squeue_devp1->cdev);
	cdev_del(&squeue_devp2->cdev);
	cdev_del(&squeue_devp3->cdev);

	kfree(squeue_devp);
	kfree(squeue_devp1);
	kfree(squeue_devp2);
	kfree(squeue_devp3);

	/* Destroy driver_class */
	class_destroy(squeue_dev_class);

	printk(KERN_ALERT "All Squeue devices removed\n");
}

module_init(squeue_device_init);
module_exit(squeue_device_exit);
