//test program for splice ratios
//JLV - 11/18/14
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define SF 10
#define SERVER 3

static float ratio[4] = {.25,.25,.25,.25};
static int r[4];
static int b[4];
static bool empty = false;
static int loop = 2; //# of time test loops

int main(int argc, char *argv[]) {
    int i,frame,k;
    int j = 0;
    if ((argc != 1) && (argc != 5)) {
        printf("Usage: %s [<ratio1> <ratio2> <ratio3> <ratio4>]\n",argv[0]);
        exit(1);
    } else if (argc == 1){
        printf("Using default ratios of .25\n");
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

    printf("server %i\n",SERVER);
    printf("---------------------\n");

    frame = 0;
    while (j < loop) {
        empty = false;
        for (i = 0;i < 4;i++) b[i] = r[i]; //reset bucket
        k = 0;
        while (!empty) {
            int sw = k % 4;
            //printf("entering switch with value %i\n",sw);
            //for (i = 0;i < 4;i++) printf("b%i = %i\n",i,b[i]);
            switch (sw) {
                case 0:
                    if (b[sw] > 0) {
                        b[sw]--;
                        if (sw == SERVER-1) printf("sent frame %i\n",frame);
                        frame++;
                    }
                    break;
                case 1:
                    if (b[sw] > 0) {
                        b[sw]--;
                        if (sw == SERVER-1) printf("sent frame %i\n",frame);
                        frame++;
                    }
                        break;
                case 2:
                    if (b[sw] > 0) {
                        b[sw]--;
                        if (sw == SERVER-1) printf("sent frame %i\n",frame);
                        frame++;
                    }
                        break;
                case 3:
                    if (b[sw] > 0) {
                        b[sw]--;
                        if (sw == SERVER-1) printf("sent frame %i\n",frame);
                        frame++;
                    }
                        break;
            }
            k++;
            empty = true;
            for (i = 0;i < 4;i++) if (b[i] > 0) empty = false; 
            //for (i = 0;i < 4;i++) printf("b%i = %i\n",i,b[i]);
        }
        j++;
    }

}
