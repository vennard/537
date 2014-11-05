/* Common defines, includes and functions
 * Should be included by client, server and repeater code
 *
 * Jan Beran
 */

#ifndef COMMON_H
#define	COMMON_H


/*******************
 * Includes 
 *******************/
/* General includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h> // fixed length datatypes (e.g. u_int32_t)
#include <stdbool.h> // define bool datatype

/* Network includes*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/*******************
 * General defines
 *******************/
#define UDP_PORT 55555 // does not matter for now
#define CLIENT_TIMEOUT 2 // timeout for recv call (secs)


/*******************
 * Packet defines
 *******************/

/* Common packet header
 * Beginning of the received packet should be casted to this struct to easily read the header */
typedef struct pkthdr_common {
    uint8_t src; // source
    uint8_t dst; // destination
    uint8_t type; // packet type
    uint32_t seq; // sequence number
    /* followed by payload */
} pkthdr_common;

/* Packet type codes */
#define TYPE_REQ 1      // request for a new file to stream
#define TYPE_REQACK 2   // request confirmation, followed by data packets
#define TYPE_REQNAK 3   // requested file does not exist / cannot be streamed
#define TYPE_DATA 4     // packet containing streamed data
#define TYPE_NAK 5      // negative acknowledgement, packet is missing
#define TYPE_FIN 6      // file streaming sucessfully finished
#define TYPE_FAIL 7     // client/server failed, stop the streaming (not used now)

/* Source/Destination codes */
/* Nodes 1-8: codes 1-8 */
#define ID_CLIENT 8
#define ID_SERVER 7 // the only server from client's perspective (i.e. node 7 in our setup)
/* Change: Client has to communicate with all the servers, ID_SERVER define does not make sense */

/* Packet lengths (without headers) */
#define PKTLEN_REQ 128 // fixed size for easy usage, can be changed in the future
#define PKTLEN_DATA 1024

/* Packet offsets (bytes)*/
#define OFF_HEADER 0
#define OFF_PAYLOAD (sizeof(pkthdr_common))


/*******************
 * Rx/Tx defines
 *******************/
/* Rx result codes */
#define RX_OK 0             // Packet OK
#define RX_TIMEOUT 1        // Rx timeout on socket
#define RX_ERR 2            // Rx failed
#define RX_CORRUPTED_PKT 3  // received corrupted packet
#define RX_NOTEXPECTED 4    // received packet type not expected (not used now)
#define RX_UNKNOWN_PKT 5    // received unknown packet, ignore it

#define MAX_TX_ATTEMPTS 5   // maximum number of repeated packet tx


/*******************
 * Functions
 *******************/
/* 
 * initHostStruct
 * 
 * Fill the given sockaddr_in structure
 * 
 * host: pointer to sockaddr_in structure to fill
 * ipAddrStr: C-string, IP address in dot decimal form, NULL for INADDR_ANY
 * port: udp port number (1024-65535), 0 for not setting a port
 * 
 * Return value: true if init successful, false otherwise
 */
bool initHostStruct(struct sockaddr_in* host, char* ipAddrStr, unsigned int port);


/* 
 * udpInit
 * 
 * Initialize local udp socket. Can be used for both client/server nodes.
 * 
 * localPort: local udp port to bind to the socket, 0 to use an arbitrary port (i.e. to not perform bind)
 * timeoutSec: timeout in seconds for recv call. 0 for no timeout (blocking recv calls)
 * 
 * Return value: -1 if init fails, socket id otherwise
 */
int udpInit(unsigned int localPort, unsigned int timeoutSec);


/* 
 * checkRxStatus
 * 
 * Check the return value of recv call and the received packet and return an rx status
 * 
 * rxRes: return value of recv call
 * hdr: pointer to the packet header
 * expSize: expected size of received packet (bytes)
 * expDst: expected packet destination
 * 
 * Return value: one of the rx result codes (see above)
 */
int checkRxStatus(int rxRes, pkthdr_common* hdr, int expSize, uint8_t expDst);

/*
 * timeDiff
 * 
 * Compute time difference between two timeval values.
 * 
 * beg: pointer to a timeval value, beggining of the measured time interval
 * end: pointer to a timeval value, end of the measured time interval
 * 
 * Return value: elapsed time in msecs, UINT_MAX if error occured (e.g. end < beg)
 */
unsigned int timeDiff(struct timeval* beg, struct timeval* end);

#endif	/* COMMON_H */

