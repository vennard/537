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
static int b[4] = {0,0,0,0};
static int sseq = 0; //frame to switch to new ratios on
static bool waitSpliceChange = false;
static bool emptyBucket = true;
static int seq = 0; //overall sequence number to keep track of splice

int getSplice() {
    int i;
    emptyBucket = true;
    for (i = 0;i < 4;i++) if (b[i] > 0) emptyBucket = false;
    if (emptyBucket) {
        for (i = 0;i < 4;i++) b[i] = spliceRatios[i];
        emptyBucket = false;
    }
    int out = -1;
    for (i = 0;i < 4;i++) {
        if (b[i] > 0) {
            b[i]--;
            seq++;
            if (i == serverName) out = seq-1;
        }
    }
    return out;
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
    int i;
    unsigned int clientSize = sizeof (*client);
    pkthdr_spl* splIn = (pkthdr_spl*) pktIn;
    int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &clientSize);
    if (rxRes < 0) return false;

    rxRes = checkRxStatus(rxRes, pktIn, serverName);
    if (rxRes == RX_TERMINATED) return false;
    if (rxRes != RX_OK) return false;

    if (splIn->type != TYPE_SPLICE) {
        printf("Warning: Read of incorrect type!\n");
        return false;
    }


    if (DEBUG) {
        printf("Splice Comm Info:\n");
        printf("    src: %i\n",splIn->src);
        printf("    dst: %i\n",splIn->dst);
        printf("    type: %i\n",splIn->type);
        printf("    sseq: %i\n",splIn->sseq);
        printf("    ratios: \n");
        for (i = 0;i < 4;i++) printf(" %i-%i \n",i,splIn->ratios[i]);
        printf("finished splice check\n");
    }
    printf("Got new splice ratios: "); 
    for (i = 0;i < 4;i++) printf(" %i ",splIn->ratios[i]);
    printf(" with sseq = %i\n", splIn->sseq);

    for (i = 0;i < 4;i++) nSpliceRatios[i] = splIn->ratios[i];
    sseq = splIn->sseq;
    waitSpliceChange = true;

    //check sequence change number
    if (sseq <= seq) {
        printf("Error: Got invalid sseq number!\n");
        return false;
    }

    //send splice acknowledge
    if (!fillpkt(pktOut, serverName, ID_CLIENT, TYPE_SPLICE_ACK, 0, NULL, 0)) {
        printf("Error: failed to construct splice ack msg\n");
        return false;
    } 
    sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) client, sizeof (*client)); 
    return true;
}

bool receiveReq(int soc, struct sockaddr_in* client, char** filename) {
    unsigned int clientSize = sizeof (*client);
    unsigned int errCount = 0;

    //non blocking read for start request
    memset(pktIn, 0, PKTLEN_MSG);
    int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &clientSize);
    if (rxRes < 0) return false; //return if no packet to read

    rxRes = checkRxStatus(rxRes, pktIn, serverName);
    if (rxRes == RX_TERMINATED) return false;
    if (rxRes != RX_OK) return false;
    if (hdrIn->type != TYPE_REQ) return false;

    //send response
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

//return values:
//0 - success
//1 - finished streaming
//2 - streaming error
int stream(int soc, struct sockaddr_in* client, char* filename) {
    int i;
    //check splice ratio change, then get new seq
    if ((seq >= sseq) && (waitSpliceChange)) {
        printf("Switching splice ratios\n");
        for (i = 0;i < 4;i++) spliceRatios[i] = nSpliceRatios[i];
        waitSpliceChange = false;
    }
    int tseq = getSplice();
    if (tseq == -1) return 0;
    printf("Sending %i\n",tseq);

    if (fillpkt(pktOut, serverName, ID_CLIENT, TYPE_DATA, tseq, NULL, 0) == false) {
        return 2;
    }

    int res = sendto(soc, pktOut, PKTLEN_DATA, 0, (struct sockaddr*) client, sizeof(*client));
    if (res == -1) {
        printf("Warning: tx error occurred for SEQ=%u\n", tseq);
        return 2;
    }
    usleep(TX_DELAY);

    //TODO add completed file transfer check

    return 0;
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

void safeExit(int soc, int exitCode){
    printf("server shutting down\n");
    close(soc);
    exit(exitCode);
}

int main(int argc, char *argv[]) {
    checkArgs(argc, argv);

    //int soc = udpInit(UDP_PORT, 0);
    int soc = udpInit(UDP_PORT, 1); //create socket with timeout
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, server stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, SOCID=%d\n", soc);
    }

    int i;
    printf("Initial Splice Ratios:");
    for (i = 0;i < 4;i++) printf(" %i ", spliceRatios[i]);
    printf("\n");

    struct sockaddr_in client;
    char* filename;
    bool start = false;
    int errCount = 0;
    printf("Waiting for a request from client\n");
    while (errCount < MAX_ERR_COUNT) {
        if (!start) {
            if (receiveReq(soc, &client, &filename) == false) {
                continue;
            } else {
                printf("A request from %s received, requested file: '%s'\n",inet_ntoa(client.sin_addr), filename);
                printf("Streaming the requested file with initial splice number = %i\n",spliceRatios[serverName-1]);
                start = true;
            }
        } else {
            //stream file 
            int streamCode = stream(soc, &client, filename);
            switch (streamCode) {
                case 0: //pkt send success
                    break;
                case 1: //streaming done
                    printf("File successfully broadcasted\n");
                    safeExit(soc,0);
                    break;
                case 2: //error with streaming
                    printf("Error with streaming service, continuing\n");
                    errCount++;
                    continue;
                default: 
                    printf("Unknown streaming error occured, exiting\n");
                    safeExit(soc,1);
                    break;
            } 
            //check for new splice ratio
            receiveSplice(soc, &client); 
        }
    }
    printf("Server exceeded error limit of %i, exiting\n",MAX_ERR_COUNT);
    safeExit(soc,1);
    return 0;
}


