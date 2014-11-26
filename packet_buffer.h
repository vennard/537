/* Interface of the packet buffer component
 * Implemented as a fixed-length ring buffer
 *
 * Jan Beran
 */

#ifndef PACKET_BUFFER_H
#define	PACKET_BUFFER_H

#include "common.h"

/*******************
 * Buffer defines
 *******************/

// possible states of the received packet - internal use only
#define BUF_SEQ_OLD -3  // seq < beginning of the buffer
#define BUF_SEQ_HIGH -2 // seq > end of the buffer
#define BUF_SEQ_EXIST -1// seq already in the buffer


/*******************
 * Public functions
 *******************/

/* 
 * bufInit
 * 
 * Initialize the buffer, must be called prior any other buffer function
 * 
 * filename: name of the output file, NULL if no file used 
 * 
 * Return value: true if initialized, fail otherwise
 */
bool bufInit(char* filename);

/* 
 * bufFinish
 * 
 * Clean up allocated resources, should be called when the streaming is finished
 * 
 * Return value: true if successfully finished, fail otherwise
 */
bool bufFinish(void);

/* 
 * bufAdd
 * 
 * Add a new (received) packet in the buffer
 * 
 * seq: seq number of the packet
 * data: pointer to the actual packet data (DATALEN size)
 * 
 * Return value:    false if the packet has to be dropped (seq too high to get in the buffer)
 *                  true if the packet is successfully inserted or if it is an already processed packet
 */
bool bufAdd(uint32_t seq, unsigned char* data);

/* 
 * bufGetSubseqCount
 * 
 * Get number of subsequent seq numbers at beginning of the buffer (number of packets until a "hole")
 * 
 * Return value: 0 if error, number of subsequent packets otherwise (can be 0 as well)
 */
unsigned int bufGetSubseqCount(void);

/* 
 * bufFlushFrame
 * 
 * Flush a frame from the beginning of the buffer. Frame is written to the output file or
 * discarded if there is no file. Expected to be called every 10ms. 
 * 
 * Return value:    true if frame successfully flushed, 
 *                  false if error or if there is missing packet at the beginning of the buffer
 */
bool bufFlushFrame(void);

/* 
 * bufGetOccupancy
 * 
 * Get ratio (percentage) of buffer occupancy. 
 * Can be used to check if we are running out of the space in the buffer,
 * 
 * Return value: floating point value within <0,1>, ratio of occupancy (0 empty, 1 full)
 */
double bufGetOccupancy(void);

/* 
 * bufGetFirstLost
 * 
 * Get sequential number of the first (oldest) lost packet. 
 * Reset the requested packets memory.
 * 
 * Return value: 0 if there no lost packet in the buffer, seq of the first lost packet otherwise
 */
uint32_t bufGetFirstLost(void);

/* 
 * bufGetNextLost
 * 
 * Get sequential number of the next lost packet according to the the requested packets memory.
 * Should be called in a cycle until there are no more missing packets. 
 * Then the next call should be bufGetFirstLost() again.
 * 
 * Return value: 0 if there no next lost packet, seq of the next lost packet otherwise
 */
uint32_t bufGetNextLost(void);

#endif	/* PACKET_BUFFER_H */

