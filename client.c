/* Client code 
 * Sends a request to stream a file (movie) from server, waits for acknowledgment and
 * keeps receiving data packets until a FIN packet is received.
 * 
 * Creation: Jan Beran
 *
 * Edited 11/12: JV - Commenting and adding splicing calculations
 */

#include "common.h"

// buffers for in/out packets
static unsigned char pktIn[PKTLEN_DATA] = {0};
static unsigned char pktOut[PKTLEN_MSG] = {0};
static pkthdr_common* hdrIn = (pkthdr_common*) pktIn;
//static pkthdr_common* hdrOut = (pkthdr_common*) pktOut;
static unsigned char* payloadIn = pktIn + HDRLEN;
//static unsigned char* payloadOut = pktOut + HDRLEN;

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

bool reqFile(int soc, char* serverIpStr, char* filename) {
    // create structures for the server info
    struct sockaddr_in server, sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    if (initHostStruct(&server, serverIpStr, UDP_PORT) == false) {
        return false;
    }

    // create the request
    if (fillpkt(pktOut, ID_CLIENT, ID_SERVER, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) {
        return false;
    }

    // send the request and receive a reply
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
    struct sockaddr_in sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    struct timeval tvStart, tvRecv;

    if (strcmp(*filename, TEST_FILE) == 0) *filename = "random";
    char streamedFilename[strlen(*filename) + 10];
    snprintf(streamedFilename, strlen(*filename) + 10, "client_%s", *filename);
    FILE* graphDataFile = fopen(GRAPH_DATA_FILE, "w");
    FILE* streamedFile = fopen(streamedFilename, "wb");
    if ((graphDataFile == NULL) || (streamedFile == NULL)) {
        printf("Error: Local data file could not be opened, program stopped\n");
        return false;
    }

    gettimeofday(&tvStart, NULL);
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

        if (hdrIn->type == TYPE_FIN) {
            fclose(graphDataFile);
            fclose(streamedFile);
            return true;
        } else if (hdrIn->type != TYPE_DATA) {
            printf("Warning: Received an unexpected packet type, ignoring it\n");
            errCount++;
            continue;
        }

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
        filename = TEST_FILE;
    } else {
        printf("Usage: %s <server ip> [<requested file>]\n", argv[0]);
        exit(1);
    }

	 // initialize UDP socket
    int soc = udpInit(UDP_PORT + 1, RECV_TIMEOUT);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, program stopped\n");
        exit(1);
    } else {
        dprintf("UDP socket initialized, IP=%s, SOCID=%d\n", argv[1], soc);
    }

	 // start transmission of file
    printf("Requesting file '%s' from the server\n", filename);
    if (reqFile(soc, argv[1], filename) == false) {
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
