/*
 *  UDP Node
 *  	Relays info recieved onto target
 *
 *  	John Vennard 
 *  	10.21.14
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

#define MESLEN 128
#define ACKLEN 20

//sendMode = 0 - receive mode
//			  = 1 - tx mode
int udpInit(char* rxIp, char* rxPort, int sendMode) {
	int soc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (soc == -1)
	{
		perror("failed to create a socket\n");
		return -1;
	} else {
		printf("node socket created\n");
	}

	//prepare structure
	struct sockaddr_in rx;
	memset(&rx, 0, sizeof(rx));
	rx.sin_family = AF_INET;

	//import ip address
	if (inet_pton(AF_INET, rxIp, &(rx.sin_addr)) != 1) {
		perror("receiver IP address not valid\n");
		close(soc);
		return -1;
	}

	//import port number
	unsigned int portInt = (unsigned int) atoi(rxPort);
	if ((portInt < 1) || (portInt > 65535)) {
		perror("Invalid port number\n");
		close(soc);
		return -1;
	}
	rx.sin_port = htons(portInt); //convert to network byte order

	//set in tx or rx mode
	if (sendMode == 0) {
		if (bind(soc, (struct sockaddr*) &rx, sizeof(rx)) == -1) {
			perror("Unable to bind listening port\n");
			return -1;
		}
	} else {
		if (connect(soc, (struct sockaddr*) &rx, sizeof(rx)) == -1) {
			perror("unable to assign rx address to socket\n");
			close(soc);
			return -1;
		}
	}

	printf("UDP node started, sending to %s, port %u\n", rxIp, portInt);
	return soc;	
}

void udpStopAndWait(char* ip, char* port) {
	char bufMsg[MESLEN];
	char bufOut[MESLEN];
	int txRes, rxRes, soc;

	sprintf(bufOut, "WHATBITCH");

	printf("Node relay service started\n");
	while (1) {
		//initialize in receive mode
		soc = udpInit(ip, port, 0);
		if (soc == -1) exit(1);

		//receive response	
		rxRes = read(soc, bufMsg, sizeof(bufMsg));
		printf("Received message: %s\n", bufMsg);

		//switch modes
		soc = udpInit(ip, port, 1);
		if (soc == -1) exit(1);

		//send out msg
		txRes = write(soc, bufOut, sizeof(bufOut));
		if (txRes == -1) {
			perror("Failed to send packet\n");
			continue;
		} else {
			printf("sent test packet\n");
		}
		sleep(5);
		exit(0);
	}
}

int main(int argc, char* argv[]) {
	//check args
	if (argc != 3) {
		printf("Usage: ./tcp_node <target ip-address> <target port/listening port>\n");
		exit(1);
	}

	udpStopAndWait(argv[1], argv[2]); //never returns

	return 0;
}


