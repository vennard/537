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
#define ACKLEN 20 // length of ACKs
#define TIMEOUT 2


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
    
  // set timeout
  struct timeval time;
  time.tv_sec = TIMEOUT;
  time.tv_usec = 0;
  if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) != 0) 
  { 
    perror("Timeout could not be set\n");
    close(soc);
    return -1;
  }
  
  printf("UDP initialized, sending packets to %s, port %u\n",receiverIp,portInt);
  return soc;
}


int getAck(char* buf, int rxRes, int bufLen)
{
  // timeout -> return -1
  // invalid packet -> return 0
  // otherwise return the received ACK
  
  if (rxRes < 0) return -1; // timeout
  if (rxRes != bufLen) return 0; // damaged packet
  if (strncmp(buf,"ACK=",4) != 0) return 0; // not the desired structure
  
  // parse the ACK
  unsigned int ack = strtoul(buf+4, NULL, 10); 
  if (ack==0 || ack==ULONG_MAX) return 0;
  
  return ack;  
}


void udpStopAndWait(int soc)
{
  char bufOut[MESLEN]; // outbound messages buffer
  char bufIn[ACKLEN]; // inbound ACKs buffer
  
  unsigned int seqNum = 1;
  unsigned int ackNum;
  int txRes, rxRes;
  
  while (1)
  {    
    memset(bufOut, seqNum, sizeof(bufOut)); // put some "data" in the output buffer
    sprintf(bufOut, "SEQ=%u#", seqNum); // insert the seq number
    
    // send a packet
    txRes = write(soc, bufOut, sizeof(bufOut));
    if (txRes == -1)
    {
      perror("Warning: Failed to send a packet, trying again\n");
      sleep(TIMEOUT); // wait for a while
      continue; // try again
    } else {
      printf("SEQ=%u sent\n",seqNum);
    }
    
    // receive a response
    rxRes = read(soc, bufIn, sizeof(bufIn));   
    ackNum = getAck(bufIn, rxRes, sizeof(bufIn));
    if (ackNum == -1)
    {      
      if (errno==EAGAIN || errno==EWOULDBLOCK)
      {
        printf("Timeout reached for SEQ=%u\n",seqNum);
      } else {
        perror("Warning: Receive error, trying again\n");
        sleep(TIMEOUT); // wait for a while       
      }
    }    
    else if (ackNum == 0)
    {
      perror("Warning: Received an invalid packet, ignoring it\n");
    } else {
      printf("ACK=%u received\n",ackNum);
      if (seqNum < ackNum) // a new highest ACK number?
      {
        // update SEQ number
        seqNum = ackNum;
      }
    }          
  }
}


int main(int argc, char* argv[]) {
    // check the arguments
    if (argc != 3) {
        printf("Usage: ./tcp_client <receiver ip-address> <receiver port>\n");
        exit(1);
    }
    
    // Init UDP 
    int soc = udpInit(argv[1],argv[2]);
    if (soc == -1) exit(1);
    
    udpStopAndWait(soc); // should never return
    
    return 0;
}
