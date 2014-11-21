/* Client code 
 * Sends a request to stream a file (movie) from server, waits for acknowledgment and
 * keeps receiving data packets until a FIN packet is received.
 * 
 * Creation: Jan Beran
 *
 * Edited 11/12: JV - Commenting and adding splicing calculations
 * 	adding splice ratio calculations
 * 	adding support for communication to all 4 servers
 */

#include <signal.h>
#include "common.h"
#include "common.c"


// buffers for in/out packets
static unsigned char pktIn[PKTLEN_DATA] = {0};
static unsigned char pktOut[PKTLEN_MSG] = {0};
static pkthdr_common* hdrIn = (pkthdr_common*) pktIn;
static unsigned char* payloadIn = pktIn + HDRLEN;
static int sock;
static struct sockaddr_in server[4];

// data rate storage
static float srcpkts[4] = {0};
static uint8_t sendRatio[4] = {0,0,0,0};
static int oldRatio[4] = {0}; //holds old ratios to check threshold for change
static struct timeval tvStart, tvRecv, tvCheck, tvSplice, tvSpliceAck;
static char *saddr[4]; //server ip addresses
bool started = false;
bool startedSplice = false;
bool ackdNewRatios = true;
static bool ackdRatio[4] = {false,false,false,false}; //all true if got ack for each new splice ratio
static int lastPkt = 0;

//signal handler to catch ctrl+c exit
void sigintHandler(int sig){
    signal(SIGINT, sigintHandler);
    printf("\nShutting down streaming service...\n");

    //send kill signal to servers
    int i,j; 
    for (j = 0;j < 2;j++) {
        for (i = 0;i < 4;i++) {
            fillpkt(pktOut, ID_CLIENT, i, TYPE_FIN, 0, NULL, 0); 
            initHostStruct(&server[i], saddr[i], UDP_PORT);
            sendto(sock, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof(server[i]));
        }
    }
    close(sock);
    exit(0);
}

bool spliceTx() {
    uint8_t i, j;
    //calculate splice ratio start frame
    uint32_t seqGap = lastPkt + SPLICE_GAP;
    unsigned char sPkt[4][PKTLEN_MSG];
    for (i = 0;i < 4;i++) {
       if (fillpktSplice(sPkt[i], i, seqGap, sendRatio) == false) return false; 
    }
    //send new splice ratios
    for (i = 0;i < 4;i++) {
        if (initHostStruct(&server[i], saddr[i], UDP_PORT) == false) return false;
        sendto(sock, sPkt[i], PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof(server[i]));
    }
    printf("Sent splice ratios!!!\n");
    ackdNewRatios = false;
    for (i = 0;i < 4;i++) ackdRatio[i] = false;
    gettimeofday(&tvSpliceAck, NULL);
    return true;
}

//TODO add splice error if lastSeq > sseq that was last sent!
void spliceAckCheck(int rxLen){
    int i;
    //check splice timeout
    gettimeofday(&tvCheck, NULL);
    int check = timeDiff(&tvSpliceAck, &tvCheck);
    if (check > (SPLICE_DELAY / 4)) {
        printf("Warning: Splice ack timeout, resending ratios\n");
        if (!spliceTx()) {
            printf("Error: Failed to resend splice ratios\n");
        }
    }
    //read splice ack
    if (hdrIn->type == TYPE_SPLICE_ACK) {
        int src = checkRxSrc(rxLen, pktIn, ID_CLIENT); 
        if ((src < 0)||(src > 3)) {
            printf("Error: Splice ack contains invalid src\n");
        } else {
            ackdRatio[src] = true;
        }
    }
    //exclude extremely congested lines from needing ack
    int tthresh = (SPLICE_FRAME / SPLICE_IGNORE_THRESH);
    for (i = 0;i < 4;i++) if (sendRatio[i] <= tthresh) ackdRatio[i] = true;

    //check for all four acks
    ackdNewRatios = true;
    for (i = 0;i < 4;i++) if (!ackdRatio[i]) ackdNewRatios = false;
    if (ackdNewRatios) printf("Splice ack success! Got all 4 acks\n");
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
    if ((src < 0)||(src > 3)) return false;
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
        for (i = 0;i < 4;i++) srcRatio[i] = (srcpkts[i] / total);
        float check = 0;
        for (i = 0;i < 4;i++) check+=srcRatio[i];
        for (i = 0;i < 4;i++) srcpkts[i] = 0; //clear packet data
        if (check != 1) {
            printf("Error with splice ratio check (= %.6f)\n",check);
            return false;
        }
        //multiply ratio * SPLICE_FRAME to find final ratio
        for (i = 0;i < 4;i++) sendRatio[i] = (int) (srcRatio[i] * SPLICE_FRAME);
        if (!startedSplice) {
            for (i = 0;i < 4;i++) oldRatio[i] = sendRatio[i];
            startedSplice = true;
        } else {
            //calculate total of absolute value of change of each ratio
            int change = 0;
            for (i = 0;i < 4;i++) change += abs(sendRatio[i] - oldRatio[i]); //TODO must scale with frame size
            for (i = 0;i < 4;i++) oldRatio[i] = sendRatio[i];
            if (1) {
                printf("Splice Check: #pkts = %f, time = %i, ratios:\n",total,checkTime);
                for (i = 0;i < 4;i++) printf("%i: %i\n",i,sendRatio[i]);
                printf("change value: %i\n",change);
            }
            if ((change >= SPLICE_THRESH)&&(ackdNewRatios)) {
                //send ratio to servers
                printf("Change (%i) exceeded at time %i, sending new splice ratios\n",change,checkTime);
                if (!spliceTx()) return false;
            }
        }
    }
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

bool reqFile(int soc, char* filename) {
    struct sockaddr_in sender;
    unsigned char pkt[4][PKTLEN_MSG];
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    unsigned int serverReq[4] = {0, 0, 0, 0};
    unsigned int serverAck[4] = {0, 0, 0, 0};
    bool done = false;

    //create targets and fill request data for all servers
    int i;
    for (i = 0;i < 4;i++) {
        if (initHostStruct(&server[i], saddr[i], UDP_PORT) == false) return false;
        if (fillpkt(pkt[i], ID_CLIENT, i, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) return false; 
    }

    // send the request and receive a reply
    gettimeofday(&tvStart, NULL); //start time from acknowledge of start request
    while (errCount++ < MAX_ERR_COUNT) {
        int i;
        for (i = 0;i < 4;i++) {
            //send request if not received ack
            if (serverAck[i] != 1) sendto(soc, pkt[i], PKTLEN_MSG, 0, (struct sockaddr*) &server[i], sizeof(server[i]));

            //read acks from servers
            memset(pktIn, 0, PKTLEN_DATA);
            int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) &sender, &senderSize);
            rxRes = checkRxStatus(rxRes, pktIn, ID_CLIENT);
            if (rxRes == RX_TERMINATED) return false;
            if (rxRes != RX_OK) continue; 
            if ((hdrIn->src > 3)||(hdrIn->src < 0)) {
                printf("Error: invalid server source\n");
                return false;
            }
            if (hdrIn->type == TYPE_REQACK) {
                serverAck[hdrIn->src] = 1; 
                //printf("Got ack from server %i!\n",hdrIn->src);
            } else if (hdrIn->type == TYPE_REQNAK) {
                printf("Error: Server %i refused to stream the requested file\n",hdrIn->src);
                return false;
            } else { //got invalid type
                printf("Warning: received unexpected packet type, ignoring it\n");
            }
        }
        //check for acks from each server
        done = true;
        for (i = 0;i < 4;i++) {
            if (serverAck[i] == 0) done = false;
        }
        if (done) {
            printf("Received acks from all servers!\n");
            return true;
        }
    }
    printf("Error: Maximum number of request attempts reached\n");
    return false;
}

bool receiveMovie(int soc, char** filename) {
    struct sockaddr_in sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    if (strcmp(*filename, TEST_FILE) == 0) *filename = "random";
    char streamedFilename[strlen(*filename) + 10];
    snprintf(streamedFilename, strlen(*filename) + 10, "client_%s", *filename);
    FILE* graphDataFile = fopen(GRAPH_DATA_FILE, "w");
    FILE* streamedFile = fopen(streamedFilename, "wb");
    if ((graphDataFile == NULL) || (streamedFile == NULL)) {
        printf("Error: Local data file could not be opened, program stopped\n");
        return false;
    }

    while (errCount < MAX_ERR_COUNT) {
        memset(pktIn, 0, PKTLEN_DATA);
        int rxLen = recvfrom(soc, pktIn, PKTLEN_DATA, 0, (struct sockaddr*) &sender, &senderSize);
        gettimeofday(&tvRecv, NULL); // get a timestamp

        int rxRes = checkRxStatus(rxLen, pktIn, ID_CLIENT);
        if (rxRes == RX_TERMINATED) {
            fclose(graphDataFile);
            fclose(streamedFile);
            return false;
        } else if (rxRes != RX_OK) {
            errCount++;
            continue;
        }

        if (spliceRatio(rxLen) == false) {
            printf("Error in spliceRatio function\n");
            continue;
        }
        //TODO check for missing packets

        if (hdrIn->type == TYPE_FIN) {
            fclose(graphDataFile);
            fclose(streamedFile);
            return true;
        } else if (hdrIn->type == TYPE_SPLICE_ACK) {
            continue;
        } else if (hdrIn->type != TYPE_DATA) { 
            printf("Warning: Received an unexpected packet type, ignoring it\n");
            errCount++;
            continue;
        }

        //store last sequence number received
        lastPkt = hdrIn->seq;

        unsigned int diff = timeDiff(&tvStart, &tvRecv);
        if ((diff == UINT_MAX) || (fprintf(graphDataFile, "%u %u\n", diff, hdrIn->seq) < 0)) {
            printf("Warning: Graph data file write error\n");
        }
        if (fwrite(payloadIn, 1, rxLen - HDRLEN, streamedFile) != (rxLen - HDRLEN)) {
            printf("Warning: Streamed local file write error\n");
        }
    }

    printf("Error: Received maximum number of subsequent bad packets\n");
    fclose(graphDataFile);
    fclose(streamedFile);
    return false;
}

char* checkArgs(int argc, char *argv[]) {
  char *filename;
  if ((argc != 6) && (argc != 5)) {
	 printf("Usage: %s <server 0 ip> <server 1 ip> <server 2 ip> <server 3 ip> [<requested file>]\n",argv[0]);
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

int main(int argc, char *argv[]) {
    signal(SIGINT, sigintHandler);
    char* filename = checkArgs(argc, argv);

	 // initialize UDP socket
    int soc = udpInit(UDP_PORT + 1, RECV_TIMEOUT);
    sock = soc;
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, program stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, SOCID=%d\n", soc);
    }

	 // start transmission of file
    printf("Requesting file '%s' from servers with the following addresses\n", filename);
    int i;
    for (i = 0;i < 4;i++) printf("server%i: %s\n", i, saddr[i]);

    if (reqFile(soc, filename) == false) { 
        printf("Error: Request failed, program stopped\n");
        close(soc);
        exit(1);
    } else {
        printf("Request for '%s' successful, receiving the data\n", filename);
    }

	 // receive movie
    if (receiveMovie(soc, &filename) == false) {
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
