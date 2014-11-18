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
#define FILE2 TEST_FILE // EMPTY_PKT_COUNT empty packets
#define EMPTY_PKT_COUNT 100
static char* fileDb[FILE_COUNT] = {FILE1, FILE2}; // file DB

// in/out packet structures
static unsigned char pktIn[PKTLEN_MSG] = {0};
static unsigned char pktOut[PKTLEN_DATA] = {0};
static pkthdr_common* hdrIn = (pkthdr_common*) pktIn;
//static pkthdr_common* hdrOut = (pkthdr_common*) pktOut;
static unsigned char* payloadIn = pktIn + HDRLEN;
static unsigned char* payloadOut = pktOut + HDRLEN;

bool lookupFile(char* file) {
    if (file == NULL) {
        return false;
    }
    unsigned int arrSize = sizeof (fileDb) / sizeof (fileDb[0]);
    for (unsigned int i = 0; i < arrSize; i++) {
        if (strcmp(fileDb[i], file) == 0) {
            return true;
        }
    }
    return false;
}

bool receiveReq(int soc, struct sockaddr_in* client, char** filename) {
    unsigned int clientSize = sizeof (*client);
    unsigned int errCount = 0;

    while (errCount++ < MAX_ERR_COUNT) {
        memset(pktIn, 0, PKTLEN_MSG);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &clientSize);
        rxRes = checkRxStatus(rxRes, pktIn, ID_SERVER);

        if (rxRes == RX_TERMINATED) return false;
        if (rxRes != RX_OK) continue;

        if (hdrIn->type != TYPE_REQ) {
            printf("Warning: Received an unexpected packet type, ignoring it\n");
            continue;
        }

        *filename = (char*) payloadIn;
        int typeOut;
        if (lookupFile(*filename) == true) {
            typeOut = TYPE_REQACK;
        } else {
            typeOut = TYPE_REQNAK;
            printf("Error: Requested file does not exist\n");
        }

        if (fillpkt(pktOut, ID_SERVER, ID_CLIENT, typeOut, 0, NULL, 0) == false) {
            return false;
        }
        sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) client, sizeof (*client));
        dprintPkt(pktOut, PKTLEN_MSG, true);

        if (typeOut == TYPE_REQACK) {
            return true;
        } else {
            return false;
        }
    }

    printf("Error: Received maximum number of subsequent bad packets\n");
    return false;
}

bool streamFile(int soc, struct sockaddr_in* client, char* filename) {
    bool isTest = false;
    if (strcmp(filename, TEST_FILE) == 0) isTest = true;

    FILE* streamFile = fopen(filename, "rb");
    if (streamFile == NULL) {
        printf("Error: the requested file cannot be opened, Server stopped\n");
        return false;
    }

    unsigned int readSize = PKTLEN_DATA - HDRLEN;
    unsigned int seq = 1;
    while (readSize == PKTLEN_DATA - HDRLEN) {        
        if (fillpkt(pktOut, ID_SERVER, ID_CLIENT, TYPE_DATA, seq, NULL, 0) == false) {
            return false;
        }
        readSize = fread(payloadOut, 1, PKTLEN_DATA - HDRLEN, streamFile);
        int res = sendto(soc, pktOut, PKTLEN_DATA, 0, (struct sockaddr*) client, sizeof (*client));
        if (res == -1) {
            printf("Warning: tx error occurred for SEQ=%u\n", seq);
        } else {
            dprintPkt(pktOut, PKTLEN_DATA, true);
        }

        seq++;
        if ((isTest == true) && (seq == EMPTY_PKT_COUNT)) break;
        usleep(TX_DELAY);
    }

    fclose(streamFile);
    if (fillpkt(pktOut, ID_SERVER, ID_CLIENT, TYPE_FIN, seq, NULL, 0) == false) {
        return false;
    }
    sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) client, sizeof (*client));
    dprintPkt(pktOut, PKTLEN_DATA, true);
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
    char* filename;

    while (1) {
        printf("Waiting for a request from client\n");
        if (receiveReq(soc, &client, &filename) == false) {
            printf("Error: Invalid client request, server stopped\n");
            continue;
        } else {
            printf("A request from %s received, requested file: '%s'\n",inet_ntoa(client.sin_addr), filename);
            printf("Streaming the requested file...\n");
            exit(0); //DEBUG STOP TODO
        }

        if (streamFile(soc, &client, filename) == false) {
            printf("Error: Error during the file streaming, server stopped\n");
            continue;
        } else {
            printf("File successfully broadcasted\n");
        }
    }

    close(soc);
    return 0;
}


