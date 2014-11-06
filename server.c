/* Server code
 * After receiving a request from client, sends an acknowledgment and starts
 * sending data packets. When the transfer is completed, sends a FIN packet.
 * 
 * Jan Beran
 */

#define _BSD_SOURCE // for usleap
#include "common.h"
#define TX_DELAY 100000

// File database
#define FILE_COUNT 2
#define FILE1 "pic.bmp" // approx 30 packets
#define FILE2 "testpackets" // EMPTY_PKT_COUNT empty packets
#define EMPTY_PKT_COUNT 100
static char* fileDb[FILE_COUNT] = {FILE1, FILE2}; // file DB

// buffers for in/out packets
static unsigned char pktIn[PKTLEN_MSG] = {0};
static unsigned char pktOut[PKTLEN_DATA] = {0};

bool lookupFile(char* file) {
    if (file == NULL) {
        return false;
    }

    for (unsigned int i = 0; i < sizeof (fileDb); i++) {
        if (strcmp(fileDb[i], file) == 0) {
            return true;
        }
    }
    return false;
}

bool receiveReq(int soc, struct sockaddr_in* client) {
    unsigned int clientSize = sizeof (*client);
    unsigned int errCount = 0;

    while (errCount < MAX_TX_ATTEMPTS) {
        memset(pktIn, 0, PKTLEN_MSG);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &clientSize);
        rxRes = checkRxStatus(rxRes, pktIn, ID_SERVER);

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

        switch (((pkthdr_common*) pktIn)->type) {
            case TYPE_REQ:
                // expected packet
                errCount = 0;
                break;
            default:
                printf("Warning: Received an unexpected packet type, ignoring it\n");
                errCount++;
                continue;
        }

        // TODO: broadcast a real file

        // create an ACK
        if (fillPktHdr(pktOut, ID_SERVER, ID_CLIENT, TYPE_REQACK, 0, NULL, 0) == false) {
            dprintf("Error: Outbound request acknowledge packet could not be created\n");
            return false;
        }
        sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) client, sizeof (*client));
        return true;
    }

    printf("Error: Received maximum number of subsequent bad packets\n");
    return false;
}

bool streamFile(int soc, struct sockaddr_in* client) {
    pkthdr_common* hdrOut = (pkthdr_common*) pktOut;
    unsigned char payload[PKTLEN_DATA - HDRLEN] = {77};

    // create a data packet
    if (fillPktHdr(pktOut, ID_SERVER, ID_CLIENT, TYPE_DATA, 1, payload, sizeof (payload)) == false) {
        dprintf("Error: Outbound data packet could not be created\n");
        return false;
    }

    for (int i = 0; i < EMPTY_PKT_COUNT; i++) {
        int res = sendto(soc, pktOut, PKTLEN_DATA, 0, (struct sockaddr*) client, sizeof (*client));
        if (res == -1) {
            printf("Warning: tx error occurred for SEQ=%u\n", hdrOut->seq);
        }
        dprintf("Sent data packet, SEQ=%u\n", hdrOut->seq);
        hdrOut->seq++;
        usleep(TX_DELAY);
    }

    if (fillPktHdr(pktOut, ID_SERVER, ID_CLIENT, TYPE_FIN, hdrOut->seq, NULL, 0) == false) {
        dprintf("Error: Outbound FIN packet could not be created\n");
        return false;
    }
    sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) client, sizeof (*client));
    dprintf("Sent FIN packet, SEQ=%u\n", hdrOut->seq);
    return true;
}

int main(int argc, char *argv[]) {
    // check the arguments
    if (argc > 1) {
        printf("No arguments expected, Usage: %s\n", argv[0]);
        exit(1);
    }

    int soc = udpInit(UDP_PORT, 0);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, server stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, SOCID=%d\n", soc);
    }

    struct sockaddr_in client;

    printf("Waiting for a request from client\n");
    if (receiveReq(soc, &client) == false) {
        printf("Error: Request cannot be received, server stopped\n");
        close(soc);
        exit(1);
    } else {
        printf("A request from %s received, streaming the requested file\n", inet_ntoa(client.sin_addr));
    }

    if (streamFile(soc, &client) == false) {
        printf("Error: Error during the file streaming, server stopped\n");
        close(soc);
        exit(1);
    } else {
        printf("File successfully broadcasted\n");
    }

    close(soc);
    return 0;
}


