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
#define ACKLEN 20 // length of ACKs
#define TIMEOUT 2

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


int getSeq(char* buf, int rxRes, int bufLen)
{
  // major recv error -> return -1
  // invalid packet -> return 0
  // otherwise return the received SEQ number
  
  if (rxRes < 0) return -1;
  if (rxRes != bufLen) return 0;
  if (strncmp(buf,"SEQ=",4) != 0) return 0;
  
  unsigned int ack = strtoul(buf+4, NULL, 10);
  if (ack==0 || ack==ULONG_MAX) return 0;
  
  return ack;  
}


void udpStopAndWait(int soc)
{
  char bufOut[ACKLEN];
  char bufIn[MESLEN];  
  
  unsigned int seqNum;
  unsigned int ackNum = 1;
  int txRes, rxRes;
  
  // prepare a structure for the remote host info
  struct sockaddr_in sender;
  unsigned int senderSize = sizeof(sender);
  
  while (1)
  {                
    // receive a packet
    rxRes = recvfrom(soc, bufIn, sizeof(bufIn), 0, (struct sockaddr*) &sender, &senderSize);   
    seqNum = getSeq(bufIn, rxRes, sizeof(bufIn));
    if (seqNum == -1)
    {
      perror("Warning: Unknown receive error, trying again\n");
      sleep(TIMEOUT); // wait for a while
      continue;
    }    
    else if (seqNum == 0)
    {
      perror("Warning: Received an invalid packet, ignoring it\n");
      continue; // do not send an ACK
    } else {
      printf("SEQ=%u received\n",seqNum);    
      if (ackNum == seqNum)
      {
        ackNum++;
      }
    } 
    
    // send a response
    memset(bufOut, seqNum, sizeof(bufOut)); // put "data" in the output buffer
    sprintf(bufOut, "ACK=%u#", ackNum);
                    
    txRes = sendto(soc, bufOut, sizeof(bufOut),0,(struct sockaddr*) &sender, senderSize);
    if (txRes == -1)
    {
      perror("Warning: Failed to send a packet\n");    
    } else {
      printf("ACK=%u sent\n",ackNum);      
    }                 
  }
}


int main(int argc, char* argv[]) {
    // check the arguments
    if (argc != 2) {
        printf("Usage: ./tcp_receiver <local port>\n");
        exit(1);
    }
    
    // Init UDP 
    int soc = udpInit(argv[1]);
    if (soc == -1) exit(1);
    
    udpStopAndWait(soc); // should never return
    
    return 0;
}
