/*
 * UDP Repeater
 * Forwards traffic from nodes a and b to node c, any packets recieved from
 * c will be duplicated and sent back to both a and b
 * all traffic uses same designated port
 * a      b
 *  \    /
 *   \  /
 *    me
 *    |
 *    |
 *    c
 * JLV - 10/30/14
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEBUG 1

int main(int argc, char *argv[]) {
	if (argc!=5) {
		printf("Usage: %s this-port a-ip b-ip c-ip\n",argv[0]);
		printf("Note - uses same port for all connected nodes, see below for configuration\n");
		printf("     <node a>      <node b>\n");
		printf("       \\              /\n");
		printf("        \\            /\n");
		printf("         \\          /\n");
		printf("          \\        /\n");
		printf("          <this node>\n");
		printf("               |\n");
		printf("               |\n");
		printf("               |\n");
		printf("           <node c>\n");
		exit(1);
	}

	int os=socket(PF_INET,SOCK_DGRAM,IPPROTO_IP);

	struct sockaddr_in a;
	a.sin_family=AF_INET;
	a.sin_addr.s_addr=inet_addr(argv[2]);
	a.sin_port=htons(atoi(argv[1]));

	struct sockaddr_in b;
	b.sin_family=AF_INET;
	b.sin_addr.s_addr=inet_addr(argv[3]);
	b.sin_port=htons(atoi(argv[1]));

	struct sockaddr_in c;
	c.sin_family=AF_INET;
	c.sin_port=htons(atoi(argv[1]));

	if(bind(os,(struct sockaddr *)&c,sizeof(c)) == -1) {
		printf("Can't bind port (%s)\n", argv[1]);
		exit(1); 
	}

	c.sin_addr.s_addr=inet_addr(argv[4]); 
	c.sin_port=htons(atoi(argv[1])); 
	if (DEBUG) { printf("TARGET ID: %i\n Starting repeater...\n",(int)c.sin_addr.s_addr); }

	struct sockaddr_in sa;
	while(1) {
		char buf[65535]; //TODO reduce to appropriate size
		socklen_t sn=sizeof(sa);

		//read udp packet
		int n=recvfrom(os,buf,sizeof(buf),0,(struct sockaddr *)&sa,&sn);
		if(n<=0) continue;

		//send udp packet
		if (sa.sin_addr.s_addr==c.sin_addr.s_addr) {
			if (DEBUG) { printf("REVERSE: %s from %i\n",buf,(int)sa.sin_addr.s_addr); }
			sendto(os,buf,n,0,(struct sockaddr *)&a,sizeof(a)); 
			sendto(os,buf,n,0,(struct sockaddr *)&b,sizeof(b)); 
		} else {  
			if (DEBUG) { printf("FORWARD: %s from %i\n",buf,(int)sa.sin_addr.s_addr); }
			sendto(os,buf,n,0,(struct sockaddr *)&c,sizeof(c)); 
		}
	}
}


