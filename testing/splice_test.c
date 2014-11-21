//test program for splice ratios
//JLV - 11/18/14
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define SF 10

int server = 2;
static float ratio[4] = {.25,.25,.25,.25};
static int r[4];
static int b[4][4];
static bool empty[4];
static int loop = 8; //# of time test loops
static int seq[4] = {0,0,0,0};
static int ts = 0;
static int sseq = 13;

void checkSplice(int ss) {
        
}

int getSplice(int ss) {
    int i;
    empty[ss] = true;
    for (i = 0;i < 4;i++) if (b[ss][i] > 0) empty[ss] = false; 
    //if bucket empty, reset
    if (empty[ss]) {
        for (i = 0;i < 4;i++) b[ss][i] = r[i];
        empty[ss] = false;
    }
    //start calc
    int out = -1;
    for (i = 0;i < 4;i++) {
        if (b[ss][i] > 0) {
            b[ss][i]--;
            seq[ss]++;
            if (i == (ss)) out = seq[ss]-1;
        } 
    }
    return out;
}

int main(int argc, char *argv[]) {
    int i,frame,k;
    int j = 0;
    if ((argc != 1) && (argc != 5)) {
        printf("Usage: %s [<ratio1> <ratio2> <ratio3> <ratio4>]\n",argv[0]);
        exit(1);
    } else if (argc == 1){
        printf("Using default ratios of .25 with server 0\n");
        for (i = 0;i < 4;i++) r[i] = (int) (ratio[i] * SF);
    } else {
        //set ratios
        char *ptr;
        float nr[4];
        printf("Using fractions: ");
        for (i = 0;i < 4;i++) {
            nr[i] = strtof(argv[i+1], &ptr);
            printf(" %.6f ",nr[i]);
            r[i] = (int) (nr[i] * SF);
        }
        printf("\n");
    }

    printf("Ratios: ");
    for (i = 0;i < 4;i++) printf(" %i ",r[i]);
    printf("\n");

    printf("  0    1    2    3\n");
    printf("---------------\n");
    int a[4];
    while (j < loop) {
        for (i = 0;i < 4;i++) a[i] = getSplice(i);
        printf("  %i   %i   %i   %i\n",a[0],a[1],a[2],a[3]);
        j++;
    }

    exit(0);
}
