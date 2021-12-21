#include "message_slot.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define STDOUT 1

int main(int argc, char** argv)
{
  int fd, ret_val, err=0;
  char *filepath;
  unsigned int channel;
  char *message;

  if (argc != 3) //verifying args number validity
  {
    perror(strerror(EINVAL));
    exit(USER_FAILURE);
  }

  filepath = argv[1];

  errno = 0;
  channel = strtol(argv[2],NULL,10);
  err = errno;
  if (err != 0) //verifying channel argument validty
  {
    perror(strerror(err));
  }

  message = (char*)malloc(BUF_LEN*sizeof(char));
  if (message == NULL)
  {
    err = errno;
    perror(strerror(err));
    exit(USER_FAILURE);
  }


  fd = open(filepath, O_RDWR ); //opening device file
  if(fd < 0) 
  {
    err = errno;
    perror(strerror(err));
    exit(USER_FAILURE);
  }

  ret_val = ioctl(fd, MSG_SLOT_CHANNEL, channel); //asigning channel to file descriptor
  if (ret_val < 0)
  {
    err = errno;
    perror(strerror(err));
    exit(USER_FAILURE);
  }


  ret_val = read(fd,message,BUF_LEN); //reading from channel, by passing a buffer of length BUF_LEN
  if (ret_val < 0)
  {
    err = errno;
    perror(strerror(err));
    exit(USER_FAILURE);
  }

  ret_val = write(STDOUT,message,ret_val); //writing message to stdout. Will only write ret_val cells from buffer, meaning will only write the length of recieved message!
  if (ret_val < 0)
  {
    err = errno;
    perror(strerror(err));
    exit(USER_FAILURE);
  }


  free(message);
  close(fd); 
  exit(SUCCESS);
}
