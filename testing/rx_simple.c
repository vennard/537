/*
 * UDP receiver
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

#define MESLEN 128 // length of inbound/outbound messages

int udpInit(char* localPort) {
  // create a socket
  int soc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (soc == -1)
  {
    perror("failed to create a socket\n");
    return -1;
  } else {
    printf("Client socket created\n");
  }
  
  // prepare a structure for the local host info
  struct sockaddr_in local;
  memset(&local, 0, sizeof (local));
  local.sin_family = AF_INET;

  // import the port number from a string
  unsigned int portInt = (unsigned int) atoi(localPort);
  if ((portInt < 1) || (portInt > 65535)) {
      perror("Invalid port number (expected 0 < port < 65536)\n");
      close(soc);
      return -1;
  }
  local.sin_port = htons(portInt); // convert to network byte order
  
  // bind the socket to the given local port port
  if (bind(soc, (struct sockaddr*) &local, sizeof(local)) == -1) 
  {
    perror("Unable to bind the socket\n");
    return -1;
  }
 
  printf("UDP initialized, listening on port %u\n",portInt);
  return soc;
}


void udpStopAndWait(int soc)
{
  char bufIn[MESLEN];  
  int txRes, rxRes;
  
  // prepare a structure for the remote host info
  struct sockaddr_in sender;
  unsigned int senderSize = sizeof(sender);
  
  while (1)
  {                
    // receive a packet
    rxRes = recvfrom(soc, bufIn, sizeof(bufIn), 0, (struct sockaddr*) &sender, &senderSize);   
    printf("Received: %s\n",bufIn);    
  } 
    
}


int main(int argc, char* argv[]) {
    // check the arguments
    if (argc != 2) {
        printf("Usage: %s <local port>\n", argv[0]);
        exit(1);
    }
    
    // Init UDP 
    int soc = udpInit(argv[1]);
    if (soc == -1) exit(1);
    
    udpStopAndWait(soc); // should never return
    
    return 0;
}
