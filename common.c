/* Definitions of common functions
 * See the header file for detailed description
 *
 * Jan Beran
 */

#include "common.h"

// TODO: optional logging into a file
// TODO: better debug prints
// TODO: deal with unknown/corrupted packets

bool initHostStruct(struct sockaddr_in* host, char* ipAddrStr, unsigned int port) {
    memset(host, 0, sizeof (*host));
    host->sin_family = AF_INET;

    if (ipAddrStr == NULL) {
        host->sin_addr.s_addr = INADDR_ANY;
    } else if (inet_pton(AF_INET, ipAddrStr, &(host->sin_addr)) != 1) { // import IP from C string
        printf("Error: Host IP address is not valid (expected format xxx.xxx.xxx.xxx)\n");
        return false;
    }

    if (port != 0) {
        if ((port < 1024) || (port > 65535)) {
            printf("Error: Invalid port number (expected 1023 < port < 65536)\n");
            return false;
        } else {
            host->sin_port = htons(port);
        }
    }
    return true;
}

int udpInit(unsigned int localPort, unsigned int timeoutSec) {
    // create a socket
    int soc = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (soc == -1) {
        printf("Error: Failed to create a socket\n");
        return -1;
    }

    // prepare a structure for the local host info
    struct sockaddr_in local;
    if (initHostStruct(&local, NULL, localPort) == false) {
        close(soc);
        return -1;
    }

    if (localPort != 0) {
        // bind the socket to the given local port port
        if (bind(soc, (struct sockaddr*) &local, sizeof (local)) == -1) {
            printf("Warning: Unable to bind the given port to the socket\n");
        }
    }

    // set timeout
    if (timeoutSec > 0) {
        struct timeval time;
        time.tv_sec = timeoutSec;
        time.tv_usec = 0;
        if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof (time)) != 0) {
            printf("Warning: Socket timeout could not be set\n");
        }
    }

    printf("UDP socket initialized, SOC=%d\n", soc);
    return soc;
}

int checkRxStatus(int rxRes, pkthdr_common* hdr, int expSize, uint8_t expDst) {
    if ((hdr == NULL) || (expSize < 1) || (expDst < 1)) {
        // wrong parameters, treat as err                   
        return RX_ERR;
    }

    if (rxRes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // timeout on socket
            return RX_TIMEOUT;
        } else {
            // major rx error
            return RX_ERR;
        }
    }

    if (rxRes != expSize) {        
        return RX_CORRUPTED_PKT;
    }

    if (hdr->dst != expDst) {
        return RX_UNKNOWN_PKT;
    }    
    return RX_OK;
}
