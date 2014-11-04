/* Client code 
 * 
 * Jan Beran
 */

#include "common.h"
#define CLIENT_LOG_NAME "client_log"

// buffers for in/out packets
static unsigned char pktIn[PKTLEN_DATA] = {0};
static unsigned char pktOut[PKTLEN_REQ] = {0};

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
                printf("REQACK received\n");
                return true;
            case TYPE_REQNAK:
                printf("Error: The requested file cannot be streamed\n");
                return false;
            default:
                printf("Warning: Received an unknown packet, ignoring it\n");
                continue;
        }
    }
    printf("Error: Maximum number of request attempts reached\n");
    return false;
}

bool receiveMovie(int soc, FILE* logFile) {
    struct sockaddr_in sender;
    unsigned int senderSize = sizeof (sender);
    unsigned int errCount = 0;

    // Packet headers
    pkthdr_common* hdrIn = (pkthdr_common*) & pktIn;

    while (errCount < MAX_TX_ATTEMPTS) {
        memset(pktIn, 0, PKTLEN_DATA);
        int rxRes = recvfrom(soc, pktIn, PKTLEN_DATA, 0, (struct sockaddr*) &sender, &senderSize);
        time_t rxTime = time(NULL); // get a timestamp
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
                printf("Streaming successfully finished\n");
                return true;
            default:
                printf("Warning: Received an unknown packet, ignoring it\n");
                errCount++;
                continue;
        }

        // process the received data packet
        char* mes = "Received data, TIME=%s, SEQ=%u\n";
        fprintf(logFile, mes, ctime(&rxTime), hdrIn->seq);
        printf(mes, ctime(&rxTime), hdrIn->seq);
    }
    
    printf("Error: Maximum number of errors reached\n");
    return false;
}

int main(int argc, char *argv[]) {
    // check the arguments
    if (argc != 3) {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    //int soc = udpInit(UDP_PORT, CLIENT_TIMEOUT);
    int soc = udpInit(UDP_PORT+1, CLIENT_TIMEOUT);
    if (soc == -1) {
        printf("Error: UDP socket could not be initialized, program stopped\n");
        exit(1);
    }

    printf("Requesting file <filename will be here> from the server\n");
    if (reqFile(soc, argv[1]) == false) {
        printf("Error: Request failed, program stopped\n");
        close(soc);
        exit(1);
    }

    FILE* logFile = fopen(CLIENT_LOG_NAME, "w");
    if (logFile == NULL) {
        printf("Error: Logfile could not be opened, program stopped\n");
        exit(1);
    }

    printf("File request successful\n");
    if (receiveMovie(soc, logFile) == false) {
        printf("Error: Error during the file streaming, program stopped\n");
        fclose(logFile);
        close(soc);
        exit(1);
    }

    fclose(logFile);
    close(soc);
    return 0;
}