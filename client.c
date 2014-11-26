/* Client code 
 * Sends a request to stream a file (movie) from server, waits for acknowledgment and
 * keeps receiving data packets until a FIN packet is received.
 * 
 * Creation: Jan Beran
 *
 * Edited 11/12: JV - Commenting and adding splicing calculations
 */

#include "common.h"
#include "packet_buffer.h"

// buffers for in/out packets
static unsigned char pktIn[PKTLEN_DATA] = {0};
static unsigned char pktOut[PKTLEN_MSG] = {0};
static pkthdr_common* hdrIn = (pkthdr_common*) pktIn;
//static pkthdr_common* hdrOut = (pkthdr_common*) pktOut;
static unsigned char* payloadIn = pktIn + HDRLEN;
//static unsigned char* payloadOut = pktOut + HDRLEN;

//static int src1pkts, src2pkts, src3pkts, src4pkts = 0;
static struct timeval tvStart, tvRecv, tvCheck;
static char *s1, *s2, *s3, *s4; //server ip addresses
static unsigned int currTxRate = RATE_MAX; // server tx rate currently set

bool spliceRatio(unsigned char *pkt) {
    return pkt;
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
    // create structures for the server info
    struct sockaddr_in server, sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    if (initHostStruct(&server, s1, UDP_PORT) == false) {
        return false;
    }

    // create the request
    if (fillpkt(pktOut, ID_CLIENT, ID_SERVER1, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) {
        return false;
    }

    // send the request and receive a reply
    gettimeofday(&tvStart, NULL);
    gettimeofday(&tvCheck, NULL);
    while (errCount++ < MAX_ERR_COUNT) {
        sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server, sizeof (server));
        dprintPkt(pktOut, PKTLEN_MSG, true);
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) &sender, &senderSize);
        rxRes = checkRxStatus(rxRes, pktIn, ID_CLIENT);

        if (rxRes == RX_TERMINATED) return false;
        if (rxRes != RX_OK) continue;

        if (hdrIn->type == TYPE_REQACK) return true;
        if (hdrIn->type == TYPE_REQNAK) {
            printf("Error: Server refused to stream the requested file\n");
            return false;
        }

        printf("Warning: Received an unexpected packet type, ignoring it\n");
    }
    printf("Error: Maximum number of request attempts reached\n");
    return false;
}

bool receiveMovie(int soc, char** filename) {
    struct sockaddr_in server, sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    bool streamReady = false;
    if (initHostStruct(&server, s1, UDP_PORT) == false) {
        return false;
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

    // create graph file
    FILE* graphDataFile = fopen(GRAPH_DATA_FILE, "w");
    if (graphDataFile == NULL) {
        printf("Error: Graph file could not be created, program stopped\n");
        return false;
    }

    while (errCount < MAX_ERR_COUNT) {
        memset(pktIn, 0, PKTLEN_DATA);
        int rxLen = recvfrom(soc, pktIn, PKTLEN_DATA, 0, (struct sockaddr*) &sender, &senderSize);
        gettimeofday(&tvRecv, NULL); // get a timestamp

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
            case TYPE_DATA:
                // expected type
                break;
            case TYPE_FIN:
                fclose(graphDataFile);
                bufFinish();
                return true;
            case TYPE_SPLICE_ACK:
                // deal with splice 
                continue;
            default:
                printf("Warning: Received an unexpected packet type, ignoring it\n");
                errCount++;
                continue;
        }

        spliceRatio(pktIn);

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
                    // TODO: broadcast the request
                    if (fillpkt(pktOut, ID_CLIENT, ID_SERVER1, TYPE_RATE, 0, (unsigned char*) &currTxRate, sizeof (unsigned int)) == false) {
                        return false;
                    }
                    sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server, sizeof (server));
                    dprintPkt(pktOut, PKTLEN_MSG, true);
                } else if ((bufOc < BUF_MIN_OCCUP) && (currTxRate*2 <= RATE_MAX)) {
                    currTxRate *= 2;
                    dprintf("Increased desired tx rate, RATE=%u\n", currTxRate);
                    // TODO: broadcast the request
                    if (fillpkt(pktOut, ID_CLIENT, ID_SERVER1, TYPE_RATE, 0, (unsigned char*) &currTxRate, sizeof (unsigned int)) == false) {
                        return false;
                    }                   
                    sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server, sizeof (server));
                    dprintPkt(pktOut, PKTLEN_MSG, true);
                }
            }

            // request lost packets
            uint32_t lostSeq = bufGetFirstLost();
            while (lostSeq > 0) {
                dprintf("Detected lost packet, SEQ=%u\n", lostSeq);

                // TODO: send the request to the server with the highest splice ratio
                if (fillpkt(pktOut, ID_CLIENT, ID_SERVER1, TYPE_NAK, 0, (unsigned char*) &lostSeq, sizeof (lostSeq)) == false) {
                    return false;
                }
                sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server, sizeof (server));
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

char* checkArgs(int argc, char *argv[]) {
    char *filename;
    if ((argc != 6) && (argc != 5)) {
        printf("Usage: %s <server 1 ip> <server 2 ip> <server 3 ip> <server 4 ip> [<requested file>]\n", argv[0]);
        exit(0);
    } else if (argc == 6) {
        filename = argv[5];
        if (strlen(filename) > MAX_FILENAME_LEN) {
            printf("Error: Filename too long\n");
            exit(1);
        }
    } else { // argc == 5
        filename = TEST_FILE;
    }
    s1 = argv[1];
    s2 = argv[2];
    s3 = argv[3];
    s4 = argv[4];
    return filename;
}

int main(int argc, char *argv[]) {
    char* filename = checkArgs(argc, argv);

    // initialize UDP socket
    int soc = udpInit(UDP_PORT + 1, RECV_TIMEOUT);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, program stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, SOCID=%d\n", soc);
    }

    // start transmission of file
    printf("Requesting file '%s' from the server\n", filename);
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
