/*
 * Simple repeater for internal nodes
 * edited to forward traffic from two nodes onto a single node
 * will duplicate any traffic send back from receiver to two senders
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

int main(int argc, char *argv[]) {
    if (argc!=3 && argc!=5 && argc!=7) {
        printf("Usage: %s this-ip this-port ip-c port-c ip-a ip-b\n",argv[0]);
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
        printf("Usage: %s our-ip our-port send-to-ip send-to-port\n",argv[0]);
        printf("Usage: %s our-ip our-port             # echo mode\n",argv[0]);
        exit(1);
    }

    int os=socket(PF_INET,SOCK_DGRAM,IPPROTO_IP);
    int og=socket(PF_INET,SOCK_DGRAM,IPPROTO_IP);

	 struct sockaddr_in a;
	 struct sockaddr_in b;
	 if (argc==7) {
		a.sin_family=AF_INET;
		a.sin_addr.s_addr=inet_addr(argv[5]);
		a.sin_port=htons(atoi(argv[2]));

	 	b.sin_family=AF_INET;
	 	b.sin_addr.s_addr=inet_addr(argv[6]);
	 	b.sin_port=htons(atoi(argv[2]));
	 }

    struct sockaddr_in c;
    c.sin_family=AF_INET;
    c.sin_addr.s_addr=inet_addr(argv[1]); 
	 c.sin_port=htons(atoi(argv[2]));

    if(bind(os,(struct sockaddr *)&c,sizeof(c)) == -1) {
        printf("Can't bind our address (%s:%s)\n", argv[1], argv[2]);
        exit(1); 
	 }

	 //testing TODO adding rx from second node
	 if (bind(og,(struct sockaddr *)&a,sizeof(a)) == -1) {
        printf("Can't bind our address (%s:%s)\n", argv[5], argv[2]);
        exit(1); 
	 }

    if(argc==5) { 
	 	  c.sin_addr.s_addr=inet_addr(argv[3]); 
		  c.sin_port=htons(atoi(argv[4])); 
	 }

    struct sockaddr_in sa;
    struct sockaddr_in da; 
	 da.sin_addr.s_addr=0;
    while(1) {
        char buf[65535];
		  socklen_t sn=sizeof(sa);
        //int n=recvfrom(os,buf,sizeof(buf),0,(struct sockaddr *)&sa,&sn);
        int n=recvfrom(og,buf,sizeof(buf),0,(struct sockaddr *)&sa,&sn);
        if(n<=0) continue;

		  //If in echo mode
        if(argc==3) { sendto(os,buf,n,0,(struct sockaddr *)&sa,sn);
		  //if got packet from target address, send back to receivers
        } else if(sa.sin_addr.s_addr==c.sin_addr.s_addr && sa.sin_port==c.sin_port) {
            if(da.sin_addr.s_addr) sendto(os,buf,n,0,(struct sockaddr *)&da,sizeof(da));
		  //catch all - send onto to target
        } else {
            sendto(os,buf,n,0,(struct sockaddr *)&c,sizeof(c));
            da=sa;
        }
    }
}


