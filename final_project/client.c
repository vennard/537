/* 
 * 537 Project Client Code
 * Final version
 *
 * Created: 11/26
 * Authors: JLV & JB
 * Team: ATeam
 *
 * TODO: 
 * 1. improve splice ack handler to not require acks from already ack'd servers after timeout - unless seq >= sseq then we'd need to recalculate and send
 * 2. check double tap sends of splice ratios
 * 3. check for splice error is lastSeq > sseq! need to handle
 * 4. splice change value does not currently scale with frame size - must check
 */

#include <signal.h>
#include "common.h"
#include "packet_buffer.h"

/* Variable Declarations */
static char *saddr[4]; //server ip addresses
static struct sockaddr_in server[4];
static int sock;

//in/out packet structures
static unsigned char pktIn[PKTLEN_DATA] = {};
static unsigned char pktOut[PKTLEN_MSG] = {};
static pkthdr_common* hdrIn = (pkthdr_common*) pktIn;
static unsigned char* payloadIn = pktIn + HDRLEN;

//rate calculations, splice variables and timers
static struct timeval tvStart, tvRecv, tvCheck, tvSplice, tvSpliceAck;
static float srcpkts[4] = {};
static uint8_t sendRatio[4] = {};
static int oldRatio[4] = {}; //holds old ratios to check threshold for change
bool started = false;
bool startedSplice = false;
bool ackdNewRatios = true;
static bool ackdRatio[4] = {[0 ... 3] = false};
static int lastPkt = 0;
static unsigned int currTxRate = RATE_MAX; // server tx rate currently set
FILE* graphDataFile;

/* Function Declarations */
char* checkArgs(int argc, char *argv[]);
bool plotGraph(void);
void sigintHandler();
bool spliceTx();
void spliceAckCheck(int rxLen);
bool spliceRatio(int rxLen);
bool reqFile(int soc, char** filename);
bool receiveMovie(int soc);

int main(int argc, char *argv[]) {
    signal(SIGINT, sigintHandler);
    char* filename = checkArgs(argc, argv);

    int soc = udpInit(UDP_PORT + 1, RECV_TIMEOUT);
    sock = soc;
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, program stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, SOCID=%d\n", soc);
    }

    // start transmission of file
    printf("Requesting file '%s' from servers\n", filename);
    if (reqFile(soc, &filename) == false) {
        printf("Error: Request failed, program stopped\n");
        close(soc);
        exit(1);
    } else {
        printf("Request for '%s' successful, receiving the data\n", filename);
    }

    // receive movie
    if (receiveMovie(soc) == false) {
        printf("Error: Error during the file streaming, program stopped\n");
        close(soc);
        exit(1);
    } else {
        printf("File successfully transfered, local copy: client_%s\n", filename);
    }

    // clean up resources and plot 
    close(soc);
    if (plotGraph() == false) {
        printf("Warning: Graph could not be plotted\n");
    } else {
        dprintf("RX packet rate graph: '%s', raw data: '%s'\n", GRAPH_OUTPUT_FILE, GRAPH_DATA_FILE);
    }

    return 0;
}

bool receiveMovie(int soc) {
    struct sockaddr_in sender;
    bool streamReady = false;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;

    // create graph file
    graphDataFile = fopen(GRAPH_DATA_FILE, "w");
    if (graphDataFile == NULL) {
        printf("Error: Graph file could not be created, program stopped\n");
        return false;
    }

    //FILE* streamedFile = fopen(streamedFilename, "wb");

    while (errCount < MAX_ERR_COUNT) {
        memset(pktIn, 0, PKTLEN_DATA);
        int rxLen = recvfrom(soc, pktIn, PKTLEN_DATA, 0, (struct sockaddr*) &sender, &senderSize);
        gettimeofday(&tvRecv, NULL);

        int rxRes = checkRxStatus(rxLen, pktIn, ID_CLIENT);
        if (rxRes == RX_TERMINATED) {
            fclose(graphDataFile);
            bufFinish();
            return false;
        } else if (rxRes != RX_OK) {
            errCount++;
            continue;
        }

        switch (hdrIn->type) {
            case TYPE_SPLICE_ACK:
            case TYPE_DATA:
                // expected type - deal with splice ratios
                if (spliceRatio(rxLen) == false) {
                    printf("Error in spliceRatio function\n");
                    continue;
                }
                break;
            case TYPE_FIN:
                fclose(graphDataFile);
                bufFinish();
                return true;
            default:
                printf("Warning: Received an unexpected packet type, ignoring it\n");
                errCount++;
                continue;
        }

        //store last sequence number received
        lastPkt = hdrIn->seq;

        // add received packet in the buffer
        if (bufAdd(hdrIn->seq, payloadIn) == false) {
            printf("Warning: Buffer write error, SEQ=%u\n", hdrIn->seq);
        }
        unsigned int diff = timeDiff(&tvStart, &tvRecv);
        if ((diff == UINT_MAX) || (fprintf(graphDataFile, "%u %u\n", diff, hdrIn->seq) < 0)) {
            printf("Warning: Graph data file write error\n");
        }

        diff = timeDiff(&tvCheck, &tvRecv);
        if (diff >= 100) {
            gettimeofday(&tvCheck, NULL);

            /*************************************
             * TRIGGERED BY THE TIMER (SECTION START)*/

            if (streamReady == true || bufGetSubseqCount() >= BUF_BUFFER_PKT) {
                streamReady = true;
                // "play" a frame every 10 ms
                for (unsigned int i = 0; i < diff / 10; i++) {
                    if (bufFlushFrame() == false) {
                        // frame missing
                        return false;
                    }
                }
            }
            // adjust tx rates
            double bufOc = bufGetOccupancy();
            dprintf("Buffer occupancy OCC=%f\n", bufOc);
            if ((bufOc > BUF_MAX_OCCUP) || (bufOc < BUF_MIN_OCCUP)) {
                if ((bufOc > BUF_MAX_OCCUP) && (currTxRate >= 2)) {
                    currTxRate /= 2;
                    dprintf("Decreased desired tx rate, RATE=%u\n", currTxRate);
                    // broadcast decrease rate request to all servers
                    for (int i = 0; i < 4; i++) {
                        if (fillpkt(pktOut, ID_CLIENT, i, TYPE_RATE, 0, (unsigned char*) &currTxRate, sizeof (unsigned int)) == false) {
                            return false;
                        }
                        sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof (server[i]));
                        dprintPkt(pktOut, PKTLEN_MSG, true);
                    }
                } else if ((bufOc < BUF_MIN_OCCUP) && (currTxRate * 2 <= RATE_MAX)) {
                    currTxRate *= 2;
                    dprintf("Increased desired tx rate, RATE=%u\n", currTxRate);
                    // broadcast increase rate request to all servers
                    for (int i = 0; i < 4; i++) {
                        if (fillpkt(pktOut, ID_CLIENT, ID_SERVER1, TYPE_RATE, 0, (unsigned char*) &currTxRate, sizeof (unsigned int)) == false) {
                            return false;
                        }
                        sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof (server[i]));
                        dprintPkt(pktOut, PKTLEN_MSG, true);
                    }
                }
            }

            // request lost packets
            uint32_t lostSeq = bufGetFirstLost();
            while (lostSeq > 0) {
                dprintf("Detected lost packet, SEQ=%u\n", lostSeq);

                // send the request to the server with the highest splice ratio
                int max = 0;
                int maxServer = 0;
                for (int i = 0; i < 4; i++) {
                    if (sendRatio[i] > max) {
                        max = sendRatio[i];
                        maxServer = i;
                    }
                }
                if (fillpkt(pktOut, ID_CLIENT, maxServer, TYPE_NAK, 0, (unsigned char*) &lostSeq, sizeof (lostSeq)) == false) {
                    return false;
                }
                sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server[maxServer], sizeof (server[maxServer]));
                dprintPkt(pktOut, PKTLEN_MSG, true);

                lostSeq = bufGetNextLost();
            }

            /* TRIGGERED BY THE TIMER (SECTION END)
             ***************************************/
        }
    }

    printf("Error: Received maximum number of subsequent bad packets\n");
    fclose(graphDataFile);
    bufFinish();
    return false;
}

bool reqFile(int soc, char** filename) {
    struct sockaddr_in sender;
    unsigned char pkt[4][PKTLEN_MSG];
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    unsigned int serverAck[4] = {[0 ... 3] = false};
    bool done = false;

    //create targets and fill request data for all servers
    int i;
    for (i = 0; i < 4; i++) {
        if (initHostStruct(&server[i], saddr[i], UDP_PORT) == false) return false;
        if (fillpkt(pkt[i], ID_CLIENT, i, TYPE_REQ, 0, (unsigned char*) *filename, strlen(*filename)) == false) return false;
        sendto(soc, pkt[i], PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof (server[i]));
    }

    // set local data file name
    if (strcmp(*filename, TEST_FILE) == 0) *filename = "random";
    char streamedFilename[strlen(*filename) + 10];
    snprintf(streamedFilename, strlen(*filename) + 10, "client_%s", *filename);

    // init packet buffer
    if (bufInit(streamedFilename) == false) {
        printf("Error: packet buffer could not be initialized, program stopped\n");
        return false;
    }

    // send the request and receive a reply
    gettimeofday(&tvStart, NULL); //start time from acknowledge of start request
    while (errCount < MAX_ERR_COUNT) {
        //check for acks from each server
        done = true;
        for (i = 0; i < 4; i++) if (!serverAck[i]) done = false;
        if (done) {
            printf("Received all acks from servers!\n");
            return true;
        }
        //read acks from servers
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_DATA, 0, (struct sockaddr*) &sender, &senderSize);
        gettimeofday(&tvRecv, NULL);
        rxRes = checkRxStatus(rxRes, pktIn, ID_CLIENT);
        if (rxRes == RX_TERMINATED) return false;
        if (rxRes != RX_OK) continue;
        if (hdrIn->src > 3) {
            printf("Error: invalid server source\n");
            return false;
        }
        switch (hdrIn->type) {
            case TYPE_REQACK:
                serverAck[hdrIn->src] = true;
                continue;
            case TYPE_DATA:
                //Receive packet before all acks from servers received 
                if (serverAck[hdrIn->src]) {
                    // add received packet in the buffer
                    if (bufAdd(hdrIn->seq, payloadIn) == false) {
                        printf("Warning: Buffer write error, SEQ=%u\n", hdrIn->seq);
                    }
                    printf("GOT HERE: 1\n");
                    unsigned int diff = timeDiff(&tvStart, &tvRecv);
                    if ((diff == UINT_MAX) || (fprintf(graphDataFile, "%u %u\n", diff, hdrIn->seq) < 0)) {
                        printf("Warning: Graph data file write error\n");
                    }
                    printf("GOT HERE: 2\n");
                } else {
                    dprintf("Warning: got data pkt before acknowledge from that server\n");
                }
                continue;
                break;
            case TYPE_NAK:
                printf("Got missing pkt request before all start request acks\n");
                break;
            default:
                printf("Warning: received unexpected packet type, ignoring it\n");
                break;
        }
        errCount++;
    }
    printf("Error: Maximum number of request attempts reached\n");
    return false;
}

bool spliceRatio(int rxLen) {
    int checkTime, i;

    //check for splice acks
    if (!ackdNewRatios) spliceAckCheck(rxLen);

    if (hdrIn->type == TYPE_SPLICE_ACK) return true;

    //check that packet is of valid type before recording
    if (hdrIn->type != TYPE_DATA) return false;

    //record where packet came from
    int src = checkRxSrc(rxLen, pktIn, ID_CLIENT);
    if ((src < 0) || (src > 3)) return false;
    srcpkts[src]++;

    //timer trigger for splice ratio calculations
    if (!started) {
        gettimeofday(&tvSplice, NULL);
        started = true;
        return true;
    } else {
        gettimeofday(&tvCheck, NULL);
        checkTime = timeDiff(&tvSplice, &tvCheck);
    }
    if (checkTime > SPLICE_DELAY) {
        gettimeofday(&tvSplice, NULL);
        float total = srcpkts[0] + srcpkts[1] + srcpkts[2] + srcpkts[3];
        float srcRatio[4];
        for (i = 0; i < 4; i++) srcRatio[i] = (srcpkts[i] / total);
        float check = 0;
        for (i = 0; i < 4; i++) check += srcRatio[i];
        for (i = 0; i < 4; i++) srcpkts[i] = 0; //clear packet data
        if (check != 1) {
            printf("Error with splice ratio check (= %.6f)\n", check);
            return false;
        }
        //multiply ratio * SPLICE_FRAME to find final ratio
        for (i = 0; i < 4; i++) sendRatio[i] = (int) (srcRatio[i] * SPLICE_FRAME);
        if (!startedSplice) {
            for (i = 0; i < 4; i++) oldRatio[i] = sendRatio[i];
            startedSplice = true;
        } else {
            //calculate total of absolute value of change of each ratio
            int change = 0;
            for (i = 0; i < 4; i++) change += abs(sendRatio[i] - oldRatio[i]); //TODO must scale with frame size
            for (i = 0; i < 4; i++) oldRatio[i] = sendRatio[i];
            if (1) {
                printf("Splice Check: #pkts = %f, time = %i, ratios:\n", total, checkTime);
                for (i = 0; i < 4; i++) printf("%i: %i\n", i, sendRatio[i]);
                printf("change value: %i\n", change);
            }
            if ((change >= SPLICE_THRESH)&&(ackdNewRatios)) {
                //send ratio to servers
                printf("Change (%i) exceeded at time %i, sending new splice ratios\n", change, checkTime);
                if (!spliceTx()) return false;
            }
        }
    }
    return true;
}

void spliceAckCheck(int rxLen) {
    int i;
    //check splice timeout
    gettimeofday(&tvCheck, NULL);
    int check = timeDiff(&tvSpliceAck, &tvCheck);
    if (check > (SPLICE_DELAY / 8)) { //TODO mess with this timiing - important
        printf("Warning: Splice ack timeout, resending ratios\n");
        gettimeofday(&tvSplice, NULL);
        if (!spliceTx()) {
            printf("Error: Failed to resend splice ratios\n");
        }
    }
    //read splice ack
    if (hdrIn->type == TYPE_SPLICE_ACK) {
        int src = checkRxSrc(rxLen, pktIn, ID_CLIENT);
        if ((src < 0) || (src > 3)) {
            printf("Error: Splice ack contains invalid src\n");
        } else {
            ackdRatio[src] = true;
        }
    }
    //exclude extremely congested lines from needing ack
    int tthresh = (SPLICE_FRAME / SPLICE_IGNORE_THRESH);
    for (i = 0; i < 4; i++) if (sendRatio[i] <= tthresh) ackdRatio[i] = true;

    //check for all four acks
    ackdNewRatios = true;
    for (i = 0; i < 4; i++) if (!ackdRatio[i]) ackdNewRatios = false;
    if (ackdNewRatios) printf("Splice ack success! Got all 4 acks\n");
}

bool spliceTx() {
    uint8_t i, j;
    uint32_t seqGap = lastPkt + SPLICE_GAP;
    ackdNewRatios = false;
    for (i = 0; i < 4; i++) ackdRatio[i] = false;

    //double tap sending splice ratios - TODO see if necessary
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 4; i++) {
            if (fillpktSplice(pktOut, i, seqGap, sendRatio) == false) return false;
            if (initHostStruct(&server[i], saddr[i], UDP_PORT) == false) return false;
            sendto(sock, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof (server[i]));
        }
    }
    printf("Sent splice ratios!!!\n");
    gettimeofday(&tvSpliceAck, NULL);
    return true;
}

bool plotGraph(void) {
    char cmd[strlen(GRAPH_DATA_FILE) + strlen(GNUPLOT_SCRIPT) + strlen(GRAPH_OUTPUT_FILE) + 10];

    // set gnuplot script permissions
    snprintf(cmd, sizeof (cmd), "chmod +x %s", GNUPLOT_SCRIPT);
    if (system(cmd) != 0) {
        return false;
    }

    // execute the script
    snprintf(cmd, sizeof (cmd), "./%s %s %s", GNUPLOT_SCRIPT, GRAPH_DATA_FILE, GRAPH_OUTPUT_FILE);
    if (system(cmd) != 0) {
        return false;
    }
    return true;
}

//signal handler to catch ctrl+c exit

void sigintHandler() {
    signal(SIGINT, sigintHandler);
    printf("\nShutting down streaming service...\n");
    //send kill signal to servers
    int i, j;
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 4; i++) {
            fillpkt(pktOut, ID_CLIENT, i, TYPE_FIN, 0, NULL, 0);
            initHostStruct(&server[i], saddr[i], UDP_PORT);
            sendto(sock, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof (server[i]));
        }
    }
    close(sock);
    exit(0);
}

char* checkArgs(int argc, char *argv[]) {
    char *filename;
    if ((argc != 6) && (argc != 5)) {
        printf("Usage: %s <server 0 ip> <server 1 ip> <server 2 ip> <server 3 ip> [<requested file>]\n", argv[0]);
        exit(1);
    } else if (argc == 6) {
        filename = argv[5];
        if (strlen(filename) > MAX_FILENAME_LEN) {
            printf("Error: Filename too long\n");
            exit(1);
        }
    } else {
        filename = TEST_FILE;
    }
    saddr[0] = argv[1];
    saddr[1] = argv[2];
    saddr[2] = argv[3];
    saddr[3] = argv[4];
    return filename;
}
