#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {

  int pru_data; // file descriptors
  uint8_t databuf[480];

  //  Now, open the PRU character device.
  //  Read data from it in chunks and write to the named pipe.
  ssize_t readpru, prime_char, wrt_data;
  FILE *fp;

  //  Open the character device to PRU0.
  pru_data = open("/dev/rpmsg_pru30", O_RDWR);
  if (pru_data < 0)
    printf("Failed to open pru character device rpmsg_pru30.");

  //  The character device must be "primed".
  prime_char = write(pru_data, "prime", 6);
  if (prime_char < 0)
    printf("Failed to prime the PRU0 char device.");

  if(!(fp = fopen("test.dat", "w"))){
    printf("Failed to create test data file");
  }

  //  This is the main data transfer loop.
  //  Note that the number of transfers is finite.
  //  This can be changed to a while(1) to run forever.
  for (int i = 0; i < 3000; i++) {
    readpru = read(pru_data, databuf, 480);
    wrt_data = fwrite(databuf, 1, 480, fp);
    printf(".", readpru);
  }
  printf("\n");

}
