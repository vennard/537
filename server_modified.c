/* Server code
 * After receiving a request from client, sends an acknowledgment and starts
 * sending data packets. When the transfer is completed, sends a FIN packet.
 * 
 * Jan Beran
 */

#define _BSD_SOURCE // for usleap
#include "common.h"
#include "common.c"
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

static int serverName; //holds name of server (1-4)
static int spliceRatios[4] = {(int) (.25 * SPLICE_FRAME),(int) (.25 * SPLICE_FRAME),(int) (.25 * SPLICE_FRAME),(int) (.25 * SPLICE_FRAME)};
static int nSpliceRatios[4] = {0,0,0,0}; //new splice ratios
static int sBucket[4] = {0,0,0,0};
static int spliceSeqSwitch = 0; //frame to switch to new ratios on
static bool emptyBucket = true;
static int syncSeq = 0; //overall sequence number to keep track of splice

int getSplice() {
    int i;
    int j = 0;
    if (emptyBucket) {
       for (i = 0;i < 4;i++) sBucket[i] = spliceRatios[i];
       emptyBucket = false;
    }
    //TODO needs to be finished
    return 0;
}

bool lookupFile(char* file) {
    if (file == NULL) {
        return false;
    }
    unsigned int arrSize = sizeof (fileDb) / sizeof (fileDb[0]);
    int i;
    for (i = 0; i < arrSize; i++) {
        if (strcmp(fileDb[i], file) == 0) {
            return true;
        }
    }
    return false;
}

bool receiveSplice(int soc, struct sockaddr_in* client) {
    unsigned int clientSize = sizeof (*client);
    pkthdr_spl* splIn = (pkthdr_spl*) pktIn;
    int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &clientSize);
        rxRes = checkRxStatus(rxRes, pktIn, serverName);

        if (rxRes == RX_TERMINATED) return false;
        if (rxRes != RX_OK) return false;

        if (splIn->type != TYPE_SPLICE) {
            printf("Warning: DID NOT GET SPLICE!\n");
            return false;
        }
        printf("Splice Comm Info:\n");
        printf("    src: %i\n",splIn->src);
        printf("    dst: %i\n",splIn->dst);
        printf("    type: %i\n",splIn->type);
        printf("    sseq: %i\n",splIn->sseq);
        printf("finished splice check\n");

    return true;
}

bool receiveReq(int soc, struct sockaddr_in* client, char** filename) {
    unsigned int clientSize = sizeof (*client);
    unsigned int errCount = 0;

    while (errCount++ < MAX_ERR_COUNT) {
        memset(pktIn, 0, PKTLEN_MSG);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &clientSize);
        rxRes = checkRxStatus(rxRes, pktIn, serverName);

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

        if (fillpkt(pktOut, serverName, ID_CLIENT, typeOut, 0, NULL, 0) == false) {
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
        //TODO get sequence number from splice ratio
        //int sendseq = getSplice(seq); 
        if (fillpkt(pktOut, serverName, ID_CLIENT, TYPE_DATA, seq, NULL, 0) == false) {
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

        //TODO check for new splice ratio from client & for splice update frame

        usleep(TX_DELAY);
    }

    fclose(streamFile);
    if (fillpkt(pktOut, serverName, ID_CLIENT, TYPE_FIN, seq, NULL, 0) == false) {
        return false;
    }
    sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) client, sizeof (*client));
    dprintPkt(pktOut, PKTLEN_DATA, true);
    return true;
}

void checkArgs(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server number>\n",argv[0]);
        exit(1);
    } else {
        char *ptr;
        serverName = (int) strtol(argv[1],&ptr,10);
        if ((serverName < 1) || (serverName > 4)) {
            printf("Invalid server number, must be 1-4\n");
            exit(1);
        }
        printf("Server %i selected.\n",serverName);
    }
}
int main(int argc, char *argv[]) {
    checkArgs(argc, argv);

    int soc = udpInit(UDP_PORT, 0);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, server stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, SOCID=%d\n", soc);
    }
    /*
    printf("SpliceRatios:\n");
    int i;
    for (i = 0;i < 4;i++) printf("Server%i - %i\n", i, spliceRatios[i]);
    */

    struct sockaddr_in client;
    char* filename;
    while (1) {
        printf("Waiting for a request from client\n");
        //TODO below testing
        if (receiveSplice(soc, &client) == false) {
            printf("Error: Invalid splice ratio request, server stopped\n");
            continue;
        } else {
            printf("Got splice ratio!\n");
        }
        if (receiveReq(soc, &client, &filename) == false) {
            printf("Error: Invalid client request, server stopped\n");
            continue;
        } else {
            printf("A request from %s received, requested file: '%s'\n",inet_ntoa(client.sin_addr), filename);
            printf("Streaming the requested file with initial splice number = %i...\n",spliceRatios[serverName-1]);
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


