/* 
 * 537 Project Server Code
 * Final version includes following features:
 * 1. Splice Ratio Calculations with acknowledges and cutoff
 * 2. Packet recovery priority
 * 3. Non-blocking operation
 *
 *
 * Created: 11/26
 * Authors: JLV & JB
 * Team: ATeam
 *
 */
//TODO real send file support: pkt recovery and sending

#define _BSD_SOURCE // for usleep
#include "common.h"

/* Variable Declarations */
//splice ratio and sequence variables
static int serverName; //local name of server (0-3)
static int spliceRatios[4] = {[0 ... 3] = (int) (.25 * SPLICE_FRAME)};
static int newSpliceRatios[4] = {};
static int b[4] = {};
static int sseq = 0;
static bool waitSpliceChange = false;
static bool emptyBucket = true;
static int seq = 1;

//in/out packet structures
static unsigned char pktIn[PKTLEN_MSG] = {};
static unsigned char pktOut[PKTLEN_DATA] = {};
static pkthdr_common* hdrIn = (pkthdr_common*) pktIn;
static unsigned char* payloadIn = pktIn + HDRLEN;

//send file variables
static char* fileDb[FILE_COUNT] = {FILE1, FILE2};
static unsigned int delayTx;


/* Function Declarations */
void checkArgs(int argc, char *argv[]);
void mainLoop(int soc);
int stream(int soc, struct sockaddr_in* client);
int getSplice();
bool rxSplice(int soc, struct sockaddr_in* client);
bool readPkt(int soc, struct sockaddr_in* client);
bool receiveReq(int soc, struct sockaddr_in* client, char** filename);
bool lookupFile(char* file);

/* Function Definitions*/
int main(int argc, char *argv[]) {
    checkArgs(argc, argv);
    int soc = udpInit(UDP_PORT, 1); //initialize port non-blocking 
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, SOCID=%d\n", soc);
    }
    mainLoop(soc);
    return 0;
}

void mainLoop(int soc) {
    int i, errCount = 0;
    struct sockaddr_in client;
    char* filename;
    bool start = false;
    delayTx = rateToDelay(RATE_MAX);

    dprintf("Initial Splice Ratios:");
    for (i = 0; i < 4; i++) dprintf(" %i ", spliceRatios[i]);
    dprintf("\n");
    printf("Server %i waiting for a request from client\n", serverName);

    while (errCount < MAX_ERR_COUNT) {
        if (!start) { //waiting for start request
            if (receiveReq(soc, &client, &filename) == false) {
                dprintf("File streaming request could not be received, waiting for a next one\n");
                continue;
            }
            printf("Request from %s received, file: '%s'\n", inet_ntoa(client.sin_addr), filename);
            printf("Beginning streaming of requested file\n");
            start = true;
        } else { //streaming file
            switch (stream(soc, &client)) {
                case 0: //pkt sent successfully
                    break;
                case 1: //stream finished
                    printf("File successfully broadcast\n");
                    close(soc);
                    exit(0);
                    break;
                case 2: //error sending packet
                    printf("Error sending packet\n");
                    errCount++;
                    continue;
                default:
                    printf("Unknown send error occurred\n");
                    errCount++;
                    continue;
            }
            readPkt(soc, &client);
        }
    }
    printf("Server exceeded error limit of %i, exiting\n", MAX_ERR_COUNT);
    close(soc);
    exit(1);
}

/* send packets based on splice ratio with delay */
int stream(int soc, struct sockaddr_in* client) {
    int i;

    //check end condition
    if (seq > EMPTY_PKT_COUNT) {
        if (fillpkt(pktOut, serverName, ID_CLIENT, TYPE_FIN, 0, NULL, 0) == false) return 2;
        return 1;
    }

    //check for splice ratio change over sequence number
    if ((seq >= sseq) && (waitSpliceChange)) {
        dprintf("Switching splice ratios\n");
        for (i = 0; i < 4; i++) spliceRatios[i] = newSpliceRatios[i];
        waitSpliceChange = false;
    }
    int tseq = getSplice();
    if (tseq == -1) return 0;


    if (fillpkt(pktOut, serverName, ID_CLIENT, TYPE_DATA, tseq, NULL, 0) == false) return 2;

    int res = sendto(soc, pktOut, PKTLEN_DATA, 0, (struct sockaddr*) client, sizeof (*client));
    if (res == -1) {
        printf("Warning: tx error occurred for SEQ=%u\n", tseq);
        return 2;
    }
    //send delay
    usleep(delayTx);

    return 0;
}

bool readPkt(int soc, struct sockaddr_in* client) {
    uint32_t misSeq;
    unsigned int size = sizeof (*client);
    
    int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &size);
    if (rxRes < 0) return false;
    rxRes = checkRxStatus(rxRes, pktIn, serverName);
    if (rxRes == RX_TERMINATED) return false;
    if (rxRes != RX_OK) return false;
    
    switch (hdrIn->type) {
        case TYPE_FIN: //kill signal
            printf("Got kill signal from client, exiting\n");
            close(soc);
            exit(0);
            break;
        case TYPE_NAK: //missing pkt request
            misSeq = (uint32_t) * payloadIn;
            dprintf("Missing pkt request: SEQ=%u\n", misSeq);
            fillpkt(pktOut, serverName, ID_CLIENT, TYPE_DATA, misSeq, NULL, 0);
            sendto(soc, pktOut, PKTLEN_DATA, 0, (struct sockaddr*) client, sizeof (*client));
            dprintPkt(pktOut, PKTLEN_DATA, true);
            break;
        case TYPE_SPLICE: //new splice ratio
            rxSplice(soc, client);
            break;
        default:
            printf("Read packet of incorrect type, continuing\n");
            return false;
    }
    return true;
}

/* reads new splice ratio from client and handles data accordingly*/
bool rxSplice(int soc, struct sockaddr_in* client) {
    int i;
    pkthdr_spl* splIn = (pkthdr_spl*) pktIn;
    printf("New splice ratios received - start at pkt #%i\n", splIn->sseq);
    dprintf("Splice Change Packet Info:\n");
    dprintf("\tsrc: %i\n", splIn->src);
    dprintf("\tdst: %i\n", splIn->dst);
    dprintf("\ttype: %i\n", splIn->type);
    dprintf("\tsseq: %i\n", splIn->sseq);
    dprintf("\tratios:");
    for (i = 0; i < 4; i++) dprintf(" %i ", splIn->ratios[i]);
    dprintf("\n");

    //save new values
    for (i = 0; i < 4; i++) newSpliceRatios[i] = splIn->ratios[i];
    sseq = splIn->sseq;
    waitSpliceChange = true;

    //check valid sequence number
    if (sseq <= seq) {
        printf("Error: sseq number below current seq number\n");
        return false;
    }

    //send acknowledge
    if (!fillpkt(pktOut, serverName, ID_CLIENT, TYPE_SPLICE_ACK, 0, NULL, 0)) {
        printf("Error: failed to construct splice ack msg\n");
        return false;
    }
    sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) client, sizeof (*client));
    return true;
}

bool receiveReq(int soc, struct sockaddr_in* client, char** filename) {
    unsigned int size = sizeof (*client);
    memset(pktIn, 0, PKTLEN_MSG);
    int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) client, &size);
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

int getSplice() {
    int i;
    emptyBucket = true;
    for (i = 0; i < 4; i++) if (b[i] > 0) emptyBucket = false;
    if (emptyBucket) {
        for (i = 0; i < 4; i++) b[i] = spliceRatios[i];
        emptyBucket = false;
    }
    int out = -1;
    for (i = 0; i < 4; i++) {
        if (b[i] > 0) {
            b[i]--;
            seq++;
            if (i == serverName) out = seq - 1;
        }
    }
    return out;
}

void checkArgs(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server number>\n", argv[0]);
        exit(1);
    } else {
        char *ptr;
        serverName = (int) strtol(argv[1], &ptr, 10);
        if ((serverName < 0) || (serverName > 3)) {
            printf("Invalid server number, must be 0-3\n");
            exit(1);
        }
    }
}

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
