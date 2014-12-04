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
#include <fcntl.h>


/*******************
 * Critical Variables
 *******************/
#define SPLICE_DELAY 500 //time between calculating splice ratios
#define SPLICE_FRAME 100  //controls resolution of splice ratio from servers
#define SPLICE_THRESH 15  //TODO threshold of change needed to send update
#define SPLICE_GAP 200 // + last received packet syncs splice changeover at servers
#define SPLICE_IGNORE_THRESH 20 //threshold for excluding node from splice ack
//above is 1/value for percentage of splice ratio to fall below to ignore

/*******************
 * General defines
 *******************/
#define dprintf(...) do { if (DEBUG) printf(__VA_ARGS__); } while (0) // debug print (use make debug)
#define UDP_PORT 55555 // does not matter for now
#define RECV_TIMEOUT 2 // timeout for recv call (secs)
#define MAX_FILENAME_LEN 50 // maximum lenght of filenames
#define TEST_FILE "/dev/urandom"
#define RATE_STEP 5 // tx rate is changed by this amount (kB/s) when needed
#define RATE_MAX 30 // maximum intended tx rate per server (kB/s)

/*******************
 * Send file defines
 *******************/
#define FILE_COUNT 2
#define FILE1 "pic.bmp" // approx 30 packets
#define FILE2 TEST_FILE // EMPTY_PKT_COUNT empty packets
#define EMPTY_PKT_COUNT 30000 //final frame number to end on

/*******************
 * Packet buffer defines
 *******************/
#define BUF_MAX_OCCUP 0.5   // maximum intended rx buffer occupancy (ratio <0,1>)
#define BUF_MIN_OCCUP 0.3   // minimum intended rx buffer occupancy (ratio <0,1>)
#define BUF_SIZE 1000       // size (pkts) of the packet buffer (in client)
#define BUF_LOST_THRSH 500  // missing packets older than seq=(newest seq)-LOST_THRSH are considered as lost 
#define BUF_CHECK_TIME 1000000 // time (usecs) between subsequent buffer flushes, rate adjustments, missing packet requests 

/*******************
 * Packet Headers
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

/*packet header of TYPE_SPLICE packet*/
typedef struct pkthdr_spl {
    uint8_t src; // source
    uint8_t dst; // destination
    uint8_t type; // packet type
    uint32_t sseq; //holds sync seq number for new splice ratios
    uint8_t ratios[4]; //holds new splice ratios
} pkthdr_spl;


/*******************
 * Packet Defines
 *******************/
/* Packet type codes */
#define TYPE_REQ 1      // request for a new file to stream
#define TYPE_REQACK 2   // request confirmation, followed by data packets
#define TYPE_REQNAK 3   // requested file does not exist / cannot be streamed
#define TYPE_DATA 4     // packet containing streamed data
#define TYPE_NAK 5      // negative acknowledgement, packet is missing
#define TYPE_FIN 6      // file streaming sucessfully finished
#define TYPE_FAIL 7     // client/server failed, stop the streaming (not used now)
#define TYPE_SPLICE 8   // splice ratio change msg
#define TYPE_SPLICE_ACK 9 // ack from server for new splice ratio
#define TYPE_RATE 10    // request to set a certain tx rate

/* Source/Destination codes */
/* Nodes 1-8: codes 1-8 */
#define ID_CLIENT 8
//below server defines are hardcoded MUST REMAIN to simplify code
//note - unused
#define ID_SERVER1 0
#define ID_SERVER2 1
#define ID_SERVER3 2
#define ID_SERVER4 3

/* Packet lengths */
#define PKTLEN_MSG 128 // fixed size for easy usage, can be changed in the future
#define HDRLEN (sizeof(pkthdr_common)) // header size
#define DATALEN 1024 // data size
#define PKTLEN_DATA (HDRLEN+DATALEN) // data packet size

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
#define RX_TERMINATED 6     // received TYPE_FAIL, communication will be terminated

#define MAX_ERR_COUNT 5     // maximum number of subsequent errors received


/*******************
 * Graph plotting defines
 *******************/
#define GRAPH_DATA_FILE "graph_datafile"
#define GNUPLOT_SCRIPT "plot.sh"

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

/* checkRxSrc
 *
 * check the source of the packet
 */
int checkRxSrc(int rxRes, unsigned char* pkt, uint8_t expDst);

/*
 * fillpktSplice
 *
 * Fills splice update packet with all necessary info
 */
bool fillpktSplice(unsigned char* buf, uint8_t dst,uint32_t sseq, uint8_t ratios[4]);

/* 
 * checkRxStatus
 * 
 * Check the return value of recv call and the received packet and return an rx status
 * 
 * rxRes: return value of recv call
 * pkt: pointer to the packet
 * expSize: expected size of received packet (bytes)
 * expDst: expected packet destination
 * 
 * Return value: one of the rx result codes (see above)
 */
int checkRxStatus(int rxRes, unsigned char* pkt, uint8_t expDst);

/*
 * timeDiff
 * 
 * Compute time difference between two timeval values.
 * 
 * beg: pointer to a timeval value, beggining of the measured time interval
 * end: pointer to a timeval value, end of the measured time interval
 * 
 * Return value: elapsed time in msecs, UINT_MAX if error occured (e.g. end < beg)
 */ unsigned int timeDiff(struct timeval* beg, struct timeval* end);

/*
 * fillpkt
 * 
 * Create a packet, fill it with a given data
 * 
 * buf: pointer to the packet (i.e. allocated memory where the packet will be created)
 * src,dst: source and destination of the packet
 * type: type of the packet
 * seq: SEQ number
 * payload: pointer to payload that will be copied in the packet. NULL if there is no payload
 * payloadLength: size (bytes) of the payload, 0 if there is no payload
 * 
 * Return value: true if the packet is successfully filled, false otherwise
 */
bool fillpkt(
        unsigned char* buf,
        uint8_t src, uint8_t dst, uint8_t type, uint32_t seq,
        unsigned char* payload, unsigned int payloadLen);

/*
 * dprintPkt
 * 
 * Print debug info about a given packet. Prints only in debug mode ("make debug" )
 * 
 * pkt: pointer to the packet
 * pktLen: length of the packet (bytes)
 * tx: true if the packet was/is transmitted, false if received
 */
void dprintPkt(unsigned char* pkt, unsigned int pktLen, bool isTx);


/*
 * rateToDelay
 * 
 * Convert tx rate to a corresponding delay between subsequent data packet transmissions
 * 
 * rate: rate in kB/s
 * 
 * Return value: delay in usecs
 */
unsigned int rateToDelay(unsigned int rate);

#endif	/* COMMON_H */

