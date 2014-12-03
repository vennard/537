/* Definitions of packet buffer functions
 * See the header file for detailed description
 *
 * Jan Beran
 */

#include "packet_buffer.h"

/*******************
 * Local variables
 *******************/

// packet structure in the buffer

typedef struct pkt_buffer {
    bool isFree;
    uint32_t seq; // sequence number
    unsigned char data[DATALEN];
} pkt_buffer;

static pkt_buffer buf[BUF_SIZE]; // buffer itself
static unsigned int headInd = 0; // buffer starts at
static uint32_t headSeq = 1; // seq at the buffer start (present or expected)
static uint32_t lastSeq = 0; // last seq in buffer
static uint32_t lastReqSeq = 0; // last requested lost seq
static bool initialized = false;
static FILE* out = NULL;

/*******************
 * Private functions
 *******************/

static unsigned int getCount(void) {
    if (lastSeq == 0) return 0;
    return (lastSeq - headSeq + 1);
}

static int checkScope(uint32_t seq) {
    if (seq < headSeq) return BUF_SEQ_OLD;
    if (seq >= headSeq + BUF_SIZE) return BUF_SEQ_HIGH;

    // get index in the buffer corresponding to seq
    int index = (int) ((headInd + (seq - headSeq)) % BUF_SIZE);

    // is packet with seq already in the buffer?
    if (buf[index].isFree == false) return BUF_SEQ_EXIST;

    // no, it is empty, return the index
    return index;
}

/*******************
 * Public functions
 *******************/

bool bufInit(char* filename) {

    if (filename != NULL) {
        // open the output file
        out = fopen(filename, "wb");
        if (out == NULL) {
            printf("Error: Local data file could not be opened, program stopped\n");
            return false;
        }
    }

    // set init values
    for (int i = 0; i < BUF_SIZE; i++) {
        buf[i].isFree = true;
        buf[i].seq = 0;
    }

    initialized = true;
    return true;
}

bool bufFinish(void) {
    if (!initialized) return false;

    if (out != NULL) {
        fclose(out);
    }
    initialized = false;
    return true;
}

bool bufAdd(uint32_t seq, unsigned char* data) {
    if ((!initialized) || (data == NULL) || (seq == 0)) return false;

    int index = checkScope(seq);
    switch (index) {
        case BUF_SEQ_OLD:
            dprintf("Warning: attempt to insert already flushed packet in the buffer, seq=%u\n", seq);
            return true;
        case BUF_SEQ_HIGH:
            printf("Warning: attempt to insert too high seq in the buffer, packet dropped, seq=%u\n", seq);
            return false;
        case BUF_SEQ_EXIST:
            dprintf("Warning: attempt to insert pkt already present in the buffer, seq=%u\n", seq);
            return true;
        default:
            dprintf("Packet Inserted in the buffer, seq=%u index=%d\n", seq, index);
            break;
    }

    // set the packet data
    buf[index].isFree = false;
    buf[index].seq = seq;
    memcpy(buf[index].data, data, DATALEN);

    // update the last seq in the buffer
    if (seq > lastSeq) {
        lastSeq = seq;
    }

    return true;
}

unsigned int bufGetSubseqCount(void) {
    if (!initialized) return 0;

    unsigned int count = 0; // packet counter
    unsigned int ind = headInd; // index in the buffer
    while (count < BUF_SIZE) {
        if (buf[ind].isFree == true) break; // stop at the first free cell
        count++;
        ind = (ind + 1) % BUF_SIZE;
    }
    return count;
}

bool bufFlushFrame(void) {
    if (!initialized) return false;

    while (buf[headInd].isFree == false) {
        // flush the packet
        if (out != NULL) {
            if (fwrite(buf[headInd].data, 1, DATALEN, out) != (DATALEN)) {
                printf("Warning: local data file write error\n");
            }
        }
        dprintf("Packet flushed from the buffer, seq=%u index=%d\n", buf[headInd].seq, headInd);

        // update the buffer head
        buf[headInd].isFree = true;
        headInd = (headInd + 1) % BUF_SIZE;
        headSeq++;
        if (lastSeq < headSeq) {
            // buffer is empty
            lastSeq = 0;
        }
    }
    //dprintf("Reached free cell at index=%d\n, flushing stopped\n", headInd);
    return true;
}

double bufGetOccupancy(void) {
    if (!initialized) return 0;
    return (((double) getCount()) / BUF_SIZE);
}

uint32_t bufGetFirstLost(void) {
    lastReqSeq = 0; // reset the requested packets memory
    return bufGetNextLost();
}

uint32_t bufGetNextLost(void) {
    if (!initialized) return 0;
    unsigned int pktCount = getCount();
    if (pktCount <= BUF_LOST_THRSH) return 0; // not enough packets to have a lost one
    if (lastReqSeq >= lastSeq - BUF_LOST_THRSH) return 0; // already requested all possible losses
    if (lastReqSeq < headSeq) lastReqSeq = 0; // last requested seq too old

    // index in the buffer 
    unsigned int ind;
    if (lastReqSeq == 0) {
        // start from the beginning
        ind = headInd;
    } else {
        // start from the last request
        ind = (headInd + (lastReqSeq - headSeq + 1)) % BUF_SIZE;
    }

    unsigned int count = 0; // packet counter
    while (count < (pktCount - BUF_LOST_THRSH)) {
        if (buf[ind].isFree == true) {
            // lost packet detected
            lastReqSeq = headSeq + count; // remember its seq
            return (headSeq + count); // request it
        }
        count++;
        ind = (ind + 1) % BUF_SIZE;
    }
    return 0;
}
