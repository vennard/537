/* Client code 
 * Sends a request to stream a file (movie) from server, waits for acknowledgment and
 * keeps receiving data packets until a FIN packet is received.
 * 
 * Jan Beran
 */

#include "common.h"
#define GRAPH_DATA_FILE "graph_datafile"
#define GNUPLOT_SCRIPT "graph_plot.sh"
#define GRAPH_OUTPUT_FILE "graph.png"

// buffers for in/out packets
static unsigned char pktIn[PKTLEN_DATA] = {0};
static unsigned char pktOut[PKTLEN_REQ] = {0};

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

bool printRx(FILE* graphFile, struct timeval* tvStart, struct timeval* tvRecv, unsigned int seq) {
    unsigned int diff = timeDiff(tvStart, tvRecv);
    if (diff == UINT_MAX) return false;

    printf("Received data packet, TIME=%u, SEQ=%u\n", diff, seq);
    if (fprintf(graphFile, "%u %u\n", diff, seq) < 0) {
        return false;
    }
    return true;
}

bool reqFile(int soc, char* serverIpStr) {
    // create structures for the server info
    struct sockaddr_in server, sender;
    unsigned int senderSize = sizeof (sender);
    if (initHostStruct(&server, serverIpStr, UDP_PORT) == false) {
        close(soc);
        return false;
    }

    // Packet headers
    pkthdr_common* hdrIn = (pkthdr_common*) & pktIn;
    pkthdr_common* hdrOut = (pkthdr_common*) & pktOut;

    // create the request
    hdrOut->src = ID_CLIENT;
    hdrOut->dst = ID_SERVER;
    hdrOut->type = TYPE_REQ;
    hdrOut->seq = 0;

    // send the request and receive a reply
    for (int i = 0; i < MAX_TX_ATTEMPTS; i++) {
        sendto(soc, pktOut, PKTLEN_REQ, 0, (struct sockaddr*) &server, sizeof (server));
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_REQ, 0, (struct sockaddr*) &sender, &senderSize);
        rxRes = checkRxStatus(rxRes, hdrIn, PKTLEN_REQ, ID_CLIENT);

        switch (rxRes) {
            case RX_OK:
                //rx OK, do nothing
                break;
            case RX_TIMEOUT:
                printf("Warning: Rx timeout on socket, trying again\n");
                continue;
            case RX_UNKNOWN_PKT:
                printf("Warning: Received an unknown packet, ignoring it\n");
                continue;
            default: // RX_ERR || RX_CORRUPTED_PKT               
                printf("Warning: Rx error occurred, trying again\n");
                continue;
        }

        switch (hdrIn->type) {
            case TYPE_REQACK:
                printf("Request acknowledgment received\n");
                return true;
            case TYPE_REQNAK:
                printf("Error: The requested file cannot be streamed\n");
                return false;
            default:
                printf("Warning: Received an unexpected packet type, ignoring it\n");
                continue;
        }
    }
    printf("Error: Maximum number of request attempts reached\n");
    return false;
}

bool receiveMovie(int soc, FILE * graphFile) {
    struct sockaddr_in sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    struct timeval tvStart, tvRecv;

    // Packet headers
    pkthdr_common* hdrIn = (pkthdr_common*) & pktIn;

    gettimeofday(&tvStart, NULL);
    while (errCount < MAX_TX_ATTEMPTS) {
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_DATA, 0, (struct sockaddr*) &sender, &senderSize);
        gettimeofday(&tvRecv, NULL); // get a timestamp
        rxRes = checkRxStatus(rxRes, hdrIn, PKTLEN_DATA, ID_CLIENT);

        switch (rxRes) {
            case RX_OK:
                //rx OK, do nothing                
                break;
            case RX_TIMEOUT:
                printf("Warning: Rx timeout on socket, trying again\n");
                errCount++;
                continue;
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
            case TYPE_DATA:
                // expected packet
                errCount = 0;
                break;
            case TYPE_FIN:
                printf("FIN packet received\n");
                return true;
            default:
                printf("Warning: Received an unexpected packet type, ignoring it\n");
                errCount++;
                continue;
        }

        // process the received data packet
        if (printRx(graphFile, &tvStart, &tvRecv, hdrIn->seq) == UINT_MAX) {
            printf("Warning: Graph data file write error\n");
        }
    }

    printf("Error: Received maximum number of subsequent bad packets\n");
    return false;
}

int main(int argc, char *argv[]) {
    // check the arguments
    if (argc != 3) {
        printf("Usage: %s <server ip> <requested file>\n", argv[0]);
        exit(1);
    }

    //int soc = udpInit(UDP_PORT, CLIENT_TIMEOUT);
    int soc = udpInit(UDP_PORT + 1, CLIENT_TIMEOUT);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, program stopped\n");
        exit(1);
    } else {
        printf("UDP socket initialized, IP=%s, SOCID=%d\n", argv[1], soc);
    }

    printf("Requesting file '%s' from the server\n", argv[2]);
    if (reqFile(soc, argv[1]) == false) {
        printf("Error: Request failed, program stopped\n");
        close(soc);
        exit(1);
    } else {
        printf("Request for '%s' successful, waiting for data\n", argv[2]);
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
        printf("File '%s' successfully transfered\n", argv[2]);
    }

    // "delete" resources
    fclose(graphFile);
    close(soc);

    // plot a graph
    if (plotGraph() == false) {
        printf("Warning: Graph could not be plotted\n");
    } else {
        printf("RX packet rate graph: '%s', raw data: '%s'\n", GRAPH_OUTPUT_FILE, GRAPH_DATA_FILE);
    }

    return 0;
}