/* Server code
 * 
 * Jan Beran
 */

#define _BSD_SOURCE // for usleap
#include "common.h"
#define MOVIE_PACKETS 1000

// buffers for in/out packets
static unsigned char pktIn[PKTLEN_REQ] = {0};
static unsigned char pktOut[PKTLEN_DATA] = {0};

bool receiveReq(int soc, struct sockaddr_in* client) {
    unsigned int clientSize = sizeof (*client);
    pkthdr_common* hdrIn = (pkthdr_common*) & pktIn;
    pkthdr_common* hdrOut = (pkthdr_common*) & pktOut;
    unsigned int errCount = 0;

    while (errCount < MAX_TX_ATTEMPTS) {
        memset(pktIn, 0, PKTLEN_REQ);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_REQ, 0, (struct sockaddr*) client, &clientSize);
        rxRes = checkRxStatus(rxRes, hdrIn, PKTLEN_REQ, ID_SERVER);

        switch (rxRes) {
            case RX_OK:
                //rx OK, do nothing                
                break;
            case RX_UNKNOWN_PKT:
                printf("Warning: Received an unknown packet, ignoring it\n");
                errCount++;
                continue;
            default: // RX_ERR || RX_CORRUPTED_PKT               
                printf("Warning: Rx error occurred, trying again\n");
                errCount++;
                continue;
        }

        switch (hdrIn->type) {
            case TYPE_REQ:
                // expected packet
                errCount = 0;
                break;
            default:
                printf("Warning: Received an unknown packet, ignoring it\n");
                errCount++;
                continue;
        }

        // create an ACK        
        hdrOut->src = ID_SERVER;
        hdrOut->dst = ID_CLIENT;
        hdrOut->type = TYPE_REQACK;
        hdrOut->seq = 1;
        sendto(soc, pktOut, PKTLEN_REQ, 0, (struct sockaddr*) client, sizeof (*client));
        return true;
    }

    printf("Error: Maximum number of errors reached\n");
    return false;
}

bool streamFile(int soc, struct sockaddr_in* client) {
    pkthdr_common* hdrOut = (pkthdr_common*) & pktOut;
    memset(pktOut, 42, PKTLEN_DATA); // dummy value    
    hdrOut->src = ID_SERVER;
    hdrOut->dst = ID_CLIENT;
    hdrOut->type = TYPE_DATA;
    hdrOut->seq = 1;

    for (int i = 0; i < MOVIE_PACKETS; i++) {
        int res = sendto(soc, pktOut, PKTLEN_DATA, 0, (struct sockaddr*) client, sizeof (*client));        
        usleep(10000);
        if (res == -1) printf("err: %s\n", strerror(errno));
        printf("Sent data packet, SEQ=%u\n", hdrOut->seq);
        hdrOut->seq++;

    }

    hdrOut->type = TYPE_FIN;
    sendto(soc, pktOut, PKTLEN_DATA, 0, (struct sockaddr*) client, sizeof (*client));
    printf("Sent FIN packet, SEQ=%u\n", hdrOut->seq);
    return true;
}

int main(int argc, char *argv[]) {
    // check the arguments
    if (argc > 1) {
        printf("Usage: %s\n", argv[0]);
        exit(1);
    }

    int soc = udpInit(UDP_PORT, 0);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, server stopped\n");
        exit(1);
    }

    struct sockaddr_in client;

    printf("Waiting for a request from client\n");
    if (receiveReq(soc, &client) == false) {
        printf("Error: Request cannot be received, server stopped\n");
        close(soc);
        exit(1);
    }

    if (streamFile(soc, &client) == false) {
        printf("Error: Error during the file streaming, program stopped\n");
        close(soc);
        exit(1);
    }

    close(soc);
    return 0;
}


