/* Definitions of common functions
 * See the header file for detailed description
 *
 * Jan Beran
 */

#include "common.h"

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
    return soc;
}

//TODO Change ERROR codes to be negative numbers - otherwise conflicts with node names
int checkRxSrc(int rxRes, unsigned char* pkt, uint8_t expDst) {
 	 if ((pkt == NULL) || (expDst < 1)) {
        printf("Warning: Unknown Rx error occurred\n");
        return RX_ERR;
    }
    if (rxRes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // timeout on socket
            printf("Warning: Rx timeout on socket, trying again\n");
            return RX_TIMEOUT;
        } else {
            printf("Warning: Rx error occurred\n");
            return RX_ERR;
        }
    }
    pkthdr_common* hdr = (pkthdr_common*) pkt;

	 if ((hdr->src < 1) || (hdr->src > 8)) return RX_ERR;
	 return hdr->src;
}

int checkRxStatus(int rxRes, unsigned char* pkt, uint8_t expDst) {
    if ((pkt == NULL) || (expDst < 1)) {
        printf("Warning: Unknown Rx error occurred\n");
        return RX_ERR;
    }

    if (rxRes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // timeout on socket
            printf("Warning: Rx timeout on socket, trying again\n");
            return RX_TIMEOUT;
        } else {
            printf("Warning: Rx error occurred\n");
            return RX_ERR;
        }
    }

    // print the debug info
    dprintPkt(pkt, rxRes, false);

    pkthdr_common* hdr = (pkthdr_common*) pkt;
    if (((rxRes != PKTLEN_DATA) && (rxRes != PKTLEN_MSG)) ||
            ((rxRes == PKTLEN_MSG) && (hdr->type == TYPE_DATA)) ||
            ((rxRes == PKTLEN_DATA) && (hdr->type != TYPE_DATA))) {
        printf("Warning: Received a packet of invalid size, ignoring it\n");
        return RX_CORRUPTED_PKT;
    }

    if (hdr->dst != expDst) {
        printf("Warning: Received a packet for different node, DST=%u\n", hdr->dst);
        return RX_UNKNOWN_PKT;
    }

    if (hdr->type == TYPE_FAIL) {
        printf("Error: FAIL message received (peer failed), communication will be terminated\n");
        return RX_TERMINATED;
    }

    return RX_OK;
}

unsigned int timeDiff(struct timeval* beg, struct timeval* end) {
    // check the pointers
    if (beg == NULL || end == NULL) return UINT_MAX;

    // check if beg <= end
    if (beg->tv_sec > end->tv_sec) return UINT_MAX;
    if ((beg->tv_sec == end->tv_sec) && (beg->tv_usec > end->tv_usec)) return UINT_MAX;

    // compute the difference
    unsigned int secDiff = (unsigned int) ((end->tv_sec - beg->tv_sec) * 1000);
    unsigned int usecDiff;
    if (end->tv_usec < beg->tv_usec) {
        usecDiff = (unsigned int) ((beg->tv_usec - end->tv_usec) / 1000);
        return (secDiff - usecDiff);
    } else {
        usecDiff = (unsigned int) ((end->tv_usec - beg->tv_usec) / 1000);
        return (secDiff + usecDiff);
    }
}

bool fillpkt(
        unsigned char* buf,
        uint8_t src, uint8_t dst, uint8_t type, uint32_t seq,
        unsigned char* payload, unsigned int payloadLen) {

    if (buf == NULL) {
        dprintf("Error: Packet could not be created\n");
        return false;
    }

    unsigned int pktLen;
    if (type == TYPE_DATA) {
        pktLen = PKTLEN_DATA;
    } else {
        pktLen = PKTLEN_MSG;
    }

    memset(buf, 0, pktLen);

    pkthdr_common* hdr = (pkthdr_common*) buf;
    hdr->src = src;
    hdr->dst = dst;
    hdr->type = type;
    hdr->seq = seq;

    if ((payload != NULL) && (payloadLen > 0)) {
        if ((payloadLen + HDRLEN) > pktLen) {
            dprintf("Error: Packet could not be created\n");
            return false;
        }
        memcpy((buf + HDRLEN), payload, payloadLen);
    }

    return true;
}

void dprintPkt(unsigned char* pkt, unsigned int pktLen, bool isTx) {
    if (!DEBUG) {
        return;
    }

    pkthdr_common* hdr = (pkthdr_common*) pkt;
    char* typeStr;
    char* dirStr;

    switch (hdr->type) {
        case TYPE_REQ:
            typeStr = "REQ";
            break;
        case TYPE_REQACK:
            typeStr = "REQACK";
            break;
        case TYPE_REQNAK:
            typeStr = "REQNAK";
            break;
        case TYPE_DATA:
            typeStr = "DATA";
            break;
        case TYPE_NAK:
            typeStr = "NAK";
            break;
        case TYPE_FIN:
            typeStr = "FIN";
            break;
        case TYPE_FAIL:
            typeStr = "FAIL";
            break;
    }
    if (isTx) {
        dirStr = "Tx";
    } else {
        dirStr = "Rx";
    }

    dprintf("%s packet: TYPE=%s, SRC=%u, DST=%u, SEQ=%u, SIZE=%u\n",
            dirStr, typeStr, hdr->src, hdr->dst, hdr->seq, pktLen);
}
