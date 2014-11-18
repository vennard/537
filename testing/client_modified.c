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

#include "common.h"
#include "common.c"


// buffers for in/out packets
static unsigned char pktIn[PKTLEN_DATA] = {0};
static unsigned char pktOut[PKTLEN_MSG] = {0};
static pkthdr_common* hdrIn = (pkthdr_common*) pktIn;
static unsigned char* payloadIn = pktIn + HDRLEN;
//static unsigned char* payloadOut = pktOut + HDRLEN;
//static pkthdr_common* hdrOut = (pkthdr_common*) pktOut;

// data rate storage
static int src1pkts, src2pkts, src3pkts, src4pkts = 0;
static struct timeval tvStart, tvRecv, tvCheck;
static char *s1, *s2, *s3, *s4; //server ip addresses

// calculate and send new ratios on to server
bool newSpliceRatio(int src1, int src2, int src3, int src4) {
	float total = src1 + src2 + src3 + src4;
	float src1ratio = (float) src1 / total; 
	float src2ratio = (float) src2 / total; 
	float src3ratio = (float) src3 / total; 
	float src4ratio = (float) src4 / total; 
	float checkRatio = src1ratio + src2ratio + src3ratio + src4ratio;
	if (checkRatio != 1) return false;
    //TODO round ratios after * SPLICE_FRAME
    //TODO check if update is necessary (standard deviation)

	//TODO finish send new ratios to servers
	/*
	static unsigned char pktMsg[PKTLEN_MSG] = {0};
    if (fillpkt(pkt, ID_CLIENT, 1, TYPE_SPLICE, 0, (unsigned char*) src1ratio, sizeof(src1ratio)) == false) {
		 printf("Failed to fill splice ratio msg packet correctly\n");
		 return false;
	 }
	 */
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
    struct sockaddr_in server1, server2, server3, server4, sender;
    unsigned char pkt1[PKTLEN_MSG], pkt2[PKTLEN_MSG], pkt3[PKTLEN_MSG], pkt4[PKTLEN_MSG]; 
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;
    unsigned int serverAcks[4] = {0, 0, 0, 0};

    //create targets
    if (initHostStruct(&server1, s1, UDP_PORT) == false) {
        return false;
    }
    if (initHostStruct(&server2, s2, UDP_PORT) == false) {
        return false;
    }
    if (initHostStruct(&server3, s3, UDP_PORT) == false) {
        return false;
    }
    if (initHostStruct(&server4, s4, UDP_PORT) == false) {
        return false;
    }

    // create the requests
    if (fillpkt(pkt1, ID_CLIENT, ID_SERVER1, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) {
        return false;
    }
    if (fillpkt(pkt2, ID_CLIENT, ID_SERVER2, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) {
        return false;
    }
    if (fillpkt(pkt3, ID_CLIENT, ID_SERVER3, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) {
        return false;
    }
    if (fillpkt(pkt4, ID_CLIENT, ID_SERVER4, TYPE_REQ, 0, (unsigned char*) filename, strlen(filename)) == false) {
        return false;
    }

    // send the request and receive a reply
    while (errCount++ < MAX_ERR_COUNT) {
        

sendto(soc, pktOut, PKTLEN_MSG, 0, (struct sockaddr*) &server, sizeof (server)); dprintPkt(pktOut, PKTLEN_MSG, true);
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_MSG, 0, (struct sockaddr*) &sender, &senderSize);
        rxRes = checkRxStatus(rxRes, pktIn, ID_CLIENT);

        if (rxRes == RX_TERMINATED) return false;
        if (rxRes != RX_OK) continue;
        if (hdrIn->type == TYPE_REQACK){
    		  gettimeofday(&tvStart, NULL); //start time from acknowledge of start request
			  return true;
		  }
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

        if (hdrIn->type == TYPE_FIN) {
            fclose(graphDataFile);
            fclose(streamedFile);
            return true;
        } else if (hdrIn->type != TYPE_DATA) {
            printf("Warning: Received an unexpected packet type, ignoring it\n");
            errCount++;
            continue;
        }

		  //TODO finish Collect data rate stats for splicing ratios
		  int src = checkRxSrc(rxLen, pktIn, ID_CLIENT);
		  switch (src) {
			  case 1: 
				  src1pkts++;
				  break;
			  case 2:
				  src2pkts++;
				  break;
			  case 3:
				  src3pkts++;
				  break;
			  case 4:
				  src4pkts++;
				  break;
			  default:
			  	  printf("Error: read invalid src address from incoming packet, exiting\n");
			     return false;
		  }
		  gettimeofday(&tvCheck, NULL);
		  unsigned int ratioCheck = timeDiff(&tvStart, &tvCheck);
		  unsigned int ratioUpdate = ratioCheck % SPLICE_UPDATE_TIME;
		  if ((ratioCheck > SPLICE_DELAY) && (ratioUpdate == 0)) {
			  if (DEBUG) printf("Launching Splice Updater: pkts received 1 - %i, 2 - %i, 3 - %i, 4 - %i\n",src1pkts,src2pkts,src3pkts,src4pkts);
			  if (newSpliceRatio(src1pkts, src2pkts, src3pkts, src4pkts)) return false;
		  }
		  //TODO end of splice ratio additions

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
	 printf("Usage: %s <server 1 ip> <server 2 ip> <server 3 ip> <server 4 ip> [<requested file>]\n",argv[0]);
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
    printf("Requesting file '%s' from servers with the following addresses\n", filename);
    printf("   server1: %s\n   server2: %s\n   server3: %s\n   server4: %s\n", s1, s2, s3, s4);

	exit(0); //DEBUG STOP
    if (reqFile(soc, argv[1], filename) == false) { //TODO remove middle arg
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
