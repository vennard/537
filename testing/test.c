#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

volatile sig_atomic_t keep_going = 1;

void catch_alarm(int sig) {
    keep_going = 0;
    signal(sig, catch_alarm);
}

int main(void) {
    signal (SIGALRM, catch_alarm);
    alarm(2);
    while(1) {
        if (!keep_going) {
            printf("wow\n");
            alarm(2);
            keep_going = 1;
        }
    }
    return 0;
} 
