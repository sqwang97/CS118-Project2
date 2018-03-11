
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */
#include <ctype.h>
#include <algorithm>
#include <unordered_set>
#include <climits>
#include <time.h>
#include <unordered_map>
#include <unistd.h>

#include <iostream>
#include <string>
#include <sstream> 
#include <fstream>




///////////////////////////////////////
//timer design for each packet
struct my_timer {
    timer_t id;
    short seqnum;

    
    my_timer() {}
    my_timer(short seq): seqnum(seq) {}
};

std::unordered_map<short, struct my_timer> pkt_timer;

//handling the signal, retransmission
static void timer_handler(int sig, siginfo_t *si, void *uc) {
    struct my_timer *timer_data = (my_timer*)si->si_value.sival_ptr;


    //retransmission data
    short seqnum = timer_data->seqnum;
    printf("timeout %d\n", seqnum);

    //delete current timer    
    timer_delete(timer_data->id);    
}

//create our own timer
static void makeTimer(struct my_timer *timer_data, unsigned msec) {
    struct sigevent         te;
    struct itimerspec       its;
    struct sigaction        sa;
    int                     sigNo = SIGRTMIN;

    /* Set up signal handler. */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, NULL) == -1)
    {
        fprintf(stderr, "Failed to setup signal handling.\n");
        return;
    }

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = timer_data;
    timer_create(CLOCK_REALTIME, &te, &timer_data->id);

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = msec * 1000000;
    timer_settime(timer_data->id, 0, &its, NULL);
}



///////////////////////////////////////




int main(int argc, char *argv[])
{
    short index = 0;
    while (index < 11){
        my_timer mytimer(index);
        makeTimer(&mytimer, 500);
        pkt_timer[index] = mytimer;
        index++;
        usleep(100);
    }
}