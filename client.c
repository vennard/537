/* Client code 
 * Sends a request to stream a file (movie) from server, waits for acknowledgment and
 * keeps receiving data packets until a FIN packet is received.
 * 
 * Jan Beran
 */

#include "common.h"

// buffers for in/out packets
static unsigned char pktIn[PKTLEN_DATA] = {0};
static unsigned char pktOut[PKTLEN_MSG] = {0};

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

bool processRx(FILE* graphFile, struct timeval* tvStart, struct timeval* tvRecv, unsigned int seq) {
    unsigned int diff = timeDiff(tvStart, tvRecv);
    if (diff == UINT_MAX) return false;

    if (fprintf(graphFile, "%u %u\n", diff, seq) < 0) {
        return false;
    }
    return true;
}

bool reqFile(int soc, char* serverIpStr, char* filename) {
    // create structures for the server info
    struct sockaddr_in server, sender;
    unsigned int senderSize = sizeof (sender);
    if (initHostStruct(&server, serverIpStr, UDP_PORT) == false) {
        return false;
    }

    // create the request
    if (fillpkt(pktOut, ID_CLIENT, ID_SERVER, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) {
        return false;
    }

    // send the request and receive a reply
    for (int i = 0; i < MAX_TX_ATTEMPTS; i++) {
        sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server, sizeof (server));
        dprintPkt(pktOut, PKTLEN_MSG, true);
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) &sender, &senderSize);
        rxRes = checkRxStatus(rxRes, pktIn, ID_CLIENT);

        if (rxRes == RX_TERMINATED) return false;
        if (rxRes != RX_OK) continue;

        uint8_t type = ((pkthdr_common*) pktIn)->type;
        if (type == TYPE_REQACK) return true;
        if (type == TYPE_REQNAK) {
            printf("Error: Server refused to stream the requested file\n");
            return false;
        }

        printf("Warning: Received an unexpected packet type, ignoring it\n");
    }
    printf("Error: Maximum number of request attempts reached\n");
    return false;
}

bool receiveMovie(int soc, FILE * graphFile) {
    struct sockaddr_in sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    struct timeval tvStart, tvRecv;

    gettimeofday(&tvStart, NULL);
    while (errCount < MAX_TX_ATTEMPTS) {
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_DATA, 0, (struct sockaddr*) &sender, &senderSize);
        gettimeofday(&tvRecv, NULL); // get a timestamp

        rxRes = checkRxStatus(rxRes, pktIn, ID_CLIENT);
        if (rxRes == RX_TERMINATED) return false;
        if (rxRes != RX_OK) {
            errCount++;
            continue; // err, try again
        }
        uint8_t type = ((pkthdr_common*) pktIn)->type;
        if (type == TYPE_FIN) return true;
        if (type != TYPE_DATA) {
            printf("Warning: Received an unexpected packet type, ignoring it\n");
            errCount++;
            continue;
        }

        // process the received data packet       
        if (processRx(graphFile, &tvStart, &tvRecv, ((pkthdr_common*) pktIn)->seq) == UINT_MAX) {
            printf("Warning: Graph data file write error\n");
        }
    }

    printf("Error: Received maximum number of subsequent bad packets\n");
    return false;
}

int main(int argc, char *argv[]) {
    // check the arguments   
    char* filename;
    if (argc == 3) {
        filename = argv[2];
        if (strlen(filename) > MAX_FILENAME_LEN) {
            printf("Error: Filename too long\n");
            exit(1);
        }
    } else if (argc == 2) {
        filename = "testpackets";
    } else {
        printf("Usage: %s <server ip> [<requested file>]\n", argv[0]);
        exit(1);
    }

    //int soc = udpInit(UDP_PORT, RECV_TIMEOUT);
    int soc = udpInit(UDP_PORT + 1, RECV_TIMEOUT);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, program stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, IP=%s, SOCID=%d\n", argv[1], soc);
    }

    printf("Requesting file '%s' from the server\n", filename);
    if (reqFile(soc, argv[1], filename) == false) {
        printf("Error: Request failed, program stopped\n");
        close(soc);
        exit(1);
    } else {
        printf("Request for '%s' successful, receiving the data\n",filename);
    }

    FILE* graphFile = fopen(GRAPH_DATA_FILE, "w");
    if (graphFile == NULL) {
        printf("Error: Graph data file could not be opened, program stopped\n");
        exit(1);
    }

    if (receiveMovie(soc, graphFile) == false) {
        printf("Error: Error during the file streaming, program stopped\n");
        fclose(graphFile);
        close(soc);
        exit(1);
    } else {
        printf("File '%s' successfully transfered\n", filename);
    }

    // "delete" resources
    close(soc);
    fclose(graphFile);

    // plot a graph
    if (plotGraph() == false) {
        printf("Warning: Graph could not be plotted\n");
    } else {
        dprintf("RX packet rate graph: '%s', raw data: '%s'\n", GRAPH_OUTPUT_FILE, GRAPH_DATA_FILE);
    }

    return 0;
}