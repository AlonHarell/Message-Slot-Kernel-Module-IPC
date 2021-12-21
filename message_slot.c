#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>


MODULE_LICENSE("GPL");

#include "message_slot.h"

typedef struct slot_node slot_node;


struct slot_node
{
  unsigned int channel_id;
  size_t size;
  char *channel_buffer;
  slot_node *next;
  slot_node *first; //Pointer back to beginning of list (sentinel)
};

slot_node* get_slot_channel(slot_node *first, unsigned int channel);


/* slots is an array, initialized in init. Will contain contain 256 cells, once for reach minor, all of which are pointers to a slot_node struct.
 * Each of the above is a sentinel - a first item in a linked list, distinguished by channel_id = 0.
 * Each list belongs to a minor. The nodes in the list are channels, containig buffers. The array will be initialized in the init function.  */
static slot_node *slots;


//======================= Helper methods =======================

/* Recieves the sentinel node of the linked list of channels assigned to the minor, and a channel number.
  Searches for the channel's node in the list. If doesnt exist, creates it.
   Returns a pointer to the slot_node which represents the wanted channel. If encounters an error, will return NULL.*/
slot_node* get_slot_channel(slot_node *first, unsigned int channel)
{
  slot_node *cur;
  cur = first;

  while (cur->next != NULL) //Searching for the node
  {
    cur = cur->next;
    if (cur->channel_id == channel)
    {
      return cur;
    }
  }

  //Not found, will be created
  
  cur->next = (slot_node*)kmalloc(sizeof(slot_node),GFP_KERNEL);
  if (cur -> next == NULL)
  {
    return NULL; //Will return NULL to notify a memory allocation error as occured
  }

  cur = cur->next;
  cur->channel_id = channel;
  cur->first = first;
  cur->size = 0;
  cur->next = NULL;
  cur->channel_buffer = NULL;
  return cur;

}

//======================= Device operations =======================

/* Opens the device */
static int device_open(struct inode* inode, struct file* file)
{
  int minor;
  minor = iminor(inode);
  file->private_data = (slot_node*)(&slots[minor]);
  //Upon first opening of the device, pointing to the sentinel of the minor in private_data (until a channel is assigned by ioctl)
  return SUCCESS;
}


/** Sets a channel for the file descriptor */
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
  slot_node *first;
  if((MSG_SLOT_CHANNEL == ioctl_command_id) && (ioctl_param != 0))
  {
    first = ((slot_node*)(file->private_data))->first;
    file->private_data = (slot_node*)get_slot_channel(first, (unsigned int)ioctl_param); //Gets a pointer to the required channel
    if (file->private_data == NULL)
    {
      return -ENOMEM; //If returned NULL, it's because of a memory allocation error
    }
  }
  else
  {
    return -EINVAL;
  }

  return SUCCESS;
}


/* Writes to the channel assigned to the fd */
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset)
{
  int i;
  slot_node *slot;
  slot = (slot_node*)file->private_data; //Getting the desired channel's slot_node struct from private_data

  if (slot->channel_id == SENTINEL) //If no channel has been set (private_data is still the sentinel)
  {
    return -EINVAL;
  }

  if ((length > BUF_LEN) || (length == 0))
  {
    return -EMSGSIZE;
  }

  if (slot->size != 0)
  {
    kfree(slot->channel_buffer); // Previous message will be overwritten
  }
  slot->channel_buffer = (char*)kmalloc(sizeof(char)*length, GFP_KERNEL); //Length of new message
  if (slot->channel_buffer == NULL)
  {
    return -ENOMEM;
  }


  for(i = 0; i < length; ++i )
  {
    if (get_user(slot->channel_buffer[i], &(buffer[i])) < 0) //get_user copies buffer cell content from userspace to channel_buffer
    {
      return -EFAULT;
    }

  }

  slot->size = length;

  return length;
}


/* Reads from the channel assigned to the fd */
static ssize_t device_read(struct file* file, char __user* buffer,size_t length, loff_t* offset )
{
  int i;
  slot_node *slot;
  slot = (slot_node*)file->private_data;


  if (slot->channel_id == SENTINEL) //If no channel has been set (private_data is still the sentinel)
  {
    return -EINVAL;
  }

  if (slot->size == 0) //If no message has been set on the channel
  {
    return -EWOULDBLOCK;
  }

  if (length < slot->size) //If buffer length will not suffice
  {
    return -ENOSPC;
  }

  for (i=0; i<length; i++)
  {
    if (put_user(slot->channel_buffer[i], buffer+i) < 0) //put_user copies channel_buffer cell content to userspace buffer
    {
      return -EFAULT;
    }
  }


  return slot->size;
}


/* activated upon close(fd), closes and frees the file descriptor */
static int device_release( struct inode* inode, struct file*  file)
{
  return SUCCESS;
}



//======================= Module and device setup =======================

struct file_operations Fops =
{
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release
};

/*Module initialization function (activated upon insmod)*/
static int __init my_init(void)
{
  int i;
  int rc = -1;

  slots = (slot_node*)kmalloc(sizeof(slot_node)*MINORS_MAX, GFP_KERNEL); //allocating the array of sentinels, one for each minor
  if (slots == NULL)
  {
    printk(KERN_ERR "Error: Memory allocation failed.");
    return -ENOMEM; 
  }

  for (i=0; i<MINORS_MAX; i++) //Initializing values of cells in the array, particularly channel_id = SENTINEL = 0
  {
    slots[i].channel_id = SENTINEL;
    slots[i].first = &(slots[i]);
    slots[i].size = 0;
    slots[i].next = NULL;
    slots[i].channel_buffer = NULL;
  }

  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops ); //registering as a chracter device
  if (rc < 0)
  {
    printk(KERN_ERR "Error: Could not register character device.");
    return rc;
  }

  return SUCCESS;
}


/*Module cleanup function (activated upon rmmod)*/
static void __exit my_cleanup(void)
{
  int i;
  slot_node *cur, *next;
  for (i=0; i<MINORS_MAX; i++) //Freeing memory: freeing the slots array, the nodes in the lists, and the buffers in the nodes.
  {
    cur = &(slots[i]);
    while (cur != NULL)
    {
      next = cur->next;
      if (cur->channel_id != SENTINEL) //Only non-sentinel nodes can have non-empty buffers, and should be freed separately from array
      {
        if (cur->channel_buffer != NULL)
        {
          kfree(cur->channel_buffer);
        }

        kfree(cur); //frees slot_node itself (struct for the channel)
      }
      
      cur = next;
    }
  }

  kfree(slots); //Will free all 256 sentinels, since they were allocated together

  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}


module_init(my_init);
module_exit(my_cleanup);







