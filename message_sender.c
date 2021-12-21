#include "message_slot.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv)
{
  int fd, ret_val, err=0;
  char *filepath;
  char *message;
  unsigned int channel;

  if (argc != 4) //verifying args number validity
  {
    perror(strerror(EINVAL));
    exit(USER_FAILURE);
  }

  filepath = argv[1];

  errno = 0;
  channel = strtol(argv[2],NULL,10); //verifying channel argument validty
  err = errno;
  if (err != 0)
  {
    perror(strerror(err));
  }

  message = argv[3];

  fd = open(filepath, O_RDWR ); //opening device file
  if (fd < 0)
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

  ret_val = write(fd,message, strlen(message)); //writing to channel buffer. strlen doesnt count the last 0 cell of a string, so it wont be written.
  if (ret_val < 0)
  {
    err = errno;
    perror(strerror(err));
    exit(USER_FAILURE);
  }
 
  close(fd); 
  exit(SUCCESS);
}
