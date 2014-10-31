/*
 * UDP sender
 *
 * Jan Beran (jberan@wisc.edu)
 */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define MESLEN 128 // length of inbound/outbound messages


int udpInit(char* receiverIp, char* receiverPort) {
  // create a socket
  int soc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (soc == -1)
  {
    perror("failed to create a socket\n");
    return -1;
  } else {
    printf("Client socket created\n");
  }
  
  // prepare a structure for the remote host info
  struct sockaddr_in receiver;
  memset(&receiver, 0, sizeof (receiver));
  receiver.sin_family = AF_INET;

  // import the IP address from a C string
  if (inet_pton(AF_INET, receiverIp, &(receiver.sin_addr)) != 1) {
      perror("receiver IP address is not valid (expected format xxx.xxx.xxx.xxx)\n");
      close(soc);
      return -1;
  }

  // import the port number from a string
  unsigned int portInt = (unsigned int) atoi(receiverPort);
  if ((portInt < 1) || (portInt > 65535)) {
      perror("Invalid port number (expected 0 < port < 65536)\n");
      close(soc);
      return -1;
  }
  receiver.sin_port = htons(portInt); // convert to network byte order
  
  // set the destination for the socket traffic
  if (connect(soc, (struct sockaddr*) &receiver, sizeof(receiver)) == -1) 
  {
    perror("Unable to assign the receiver address to the socket\n");
    close(soc);
    return -1;
  }
    
  printf("UDP initialized, sending packets to %s, port %u\n",receiverIp,portInt);
  return soc;
}



void udpStopAndWait(int soc)
{
  char bufOut[MESLEN]; // outbound messages buffer
  int txRes, rxRes;
  
  while (1)
  {    
    sprintf(bufOut, "!ATEAM TEST PACKET!"); 
    
    // send a packet
    txRes = write(soc, bufOut, sizeof(bufOut));
    if (txRes == -1)
    {
      perror("Warning: Failed to send a packet, trying again\n");
      continue; // try again
    } 
   sleep(2); 
  }
}


int main(int argc, char* argv[]) {
    // check the arguments
    if (argc != 3) {
        printf("Usage: ./%s <receiver ip-address> <receiver port>\n",argv[0]);
        exit(1);
    }
    
    // Init UDP 
    int soc = udpInit(argv[1],argv[2]);
    if (soc == -1) exit(1);
    
    udpStopAndWait(soc); // should never return
    
    return 0;
}
