#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */
#include <poll.h>
#include <errno.h>
#include <ctype.h>
#include <algorithm>
#include <time.h>
#include <unordered_map>

#include <iostream>
#include <string>
#include <sstream> 
#include <fstream>

#include "packet.h"
#include "buffer.h"

const short SYN_Seq = 0;

//global SeqNum buffer: Seq is received from server, ack back the same number
Buffer pkt_buffer(WINDOWSIZE/BUFF_SIZE);
std::string filename;
int sockfd = -1;
bool dup_flag = false;

void error(char *msg)
{
    perror(msg);
    exit(1);
}


//declaration
void printmessage(std::string action, std::string state, short num);
static void makeTimer(struct my_timer *timer_data, unsigned msec);

///////////////////////////////////////
//timer design for each packet
struct my_timer {
    timer_t id;
    short seqnum;
    Packet pkt;

    my_timer() {}
    my_timer(short seq, Packet mpkt): seqnum(seq), pkt(mpkt){}
};

std::unordered_map<short, struct my_timer> pkt_timer;

//handling the signal, retransmission
static void timer_handler(int sig, siginfo_t *si, void *uc) {
    struct my_timer *timer_data = (my_timer*)si->si_value.sival_ptr;

    //retransmission data
    short seqnum = timer_data->seqnum;
    Packet pkt = timer_data->pkt;
    std::string buffer = pkt.packet_to_string();

    //2*RTO timeout for FIN
    if (pkt.getFINbit())
    {
        close(sockfd);
        exit(1);
        //close connection
    }


    write(sockfd, buffer.c_str(), buffer.length());
    if (pkt.getSYNbit())
        printmessage("send", "Retransmission SYN", seqnum);
    else
        printmessage("send", "Retransmission", seqnum);

    //delete current timer    
    timer_delete(timer_data->id);

    //set the timer, and add it to the list
    pkt_timer[seqnum] = my_timer(seqnum, pkt);
    makeTimer(&pkt_timer[seqnum], TIMEOUT);
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


/* 
use to print message 
action: "send" or "receive"
state: use for sending, input can be "Retransmission" / "FIN" / "SYN" / ""
num: seq or ack
*/
void printmessage(std::string action, std::string state, short num){
    if (!action.compare("send")){
        if (state != "SYN" && state != "Retransmission SYN")    //ACK ("Retransmission") ("FIN")
            printf("Sending packet %d %s\n", num, state.c_str());
        else    // num = -1, SYN
            printf("Sending packet %s\n", state.c_str());
    }
    else if (!action.compare("receive"))
        printf("Receiving packet %d\n", num);
    else
        fprintf(stderr, "error input.");
}

/*
send back an ack pkt
*/
void process_regular_packet(Packet& pkt){
    Packet response;
    std::string buffer;
    int buff_size;

    response.setACK(pkt.getSEQ());
    buffer = response.packet_to_string();
    buff_size = buffer.length();
    write(sockfd, buffer.c_str(), buff_size);

    if (pkt_buffer.push_in_pkt(pkt.getSEQ(), pkt.packet_to_string())){
        //printf("%s\n", (pkt.get_content()).c_str());
        printmessage("send", "", response.getACK());
    }
    else
        printmessage("send", "Retransmission", response.getACK());

    //write to file
    std::string output = pkt_buffer.drop_packet();
    while (!output.empty()){
        Packet temp(output);
        std::ofstream myfile("received.data", std::ios::out | std::ios::app );
        myfile<<temp.get_content();
        myfile.close();
        output = pkt_buffer.drop_packet();
    }
}

void process_packet (Packet& pkt){
    printmessage("receive", "", pkt.getSEQ() );


    Packet response;
    std::string buffer;
    int buff_size;
    //already sent SYN_Seq, check if ACK from server = SYN_Seq
    //send REQbit + (ACK = server's SYN_Seq) + filename to server
    //buffer this SeqNum
    //// need to buffer this packet, if lost, need timeout to retransmit ////
    if (pkt.getSYNbit()){
        //delete the timer at receiving time
        std::unordered_map<short, struct my_timer>::iterator it = pkt_timer.find(pkt.getSEQ());
        if (it != pkt_timer.end()) {   //get the ACK, delete the timer
            timer_delete((it->second).id);
            pkt_timer.erase(it);
        }
        //ignore duplicate packets
        if (dup_flag) return;

        if (pkt.getACK() == SYN_Seq){
            //set base seq for upcoming file pkts
            pkt_buffer.setBaseSeq(pkt.getSEQ()+ HEADER_SIZE);

            response.setACK(pkt.getSEQ());
            response.setSEQ(HEADER_SIZE);
            response.setREQbit(true);
            response.fillin_content(filename);
            buffer = response.packet_to_string();
            buff_size = buffer.length();
            write(sockfd, buffer.c_str(), buff_size);
            dup_flag = true; //We have received SYN 

            /*
            //set the timer, and add it to the list
            short seqnum = response.getSEQ();
            pkt_timer[seqnum] = my_timer(seqnum, response);
            makeTimer(&pkt_timer[seqnum], TIMEOUT);
            */

            printmessage("send", "", response.getACK());
        }
    }

    //receive FIN from server, send FIN and ACK to server
    //// close connection for 2RTO ////
    else if (pkt.getFINbit()){
        //ignore duplicate packets
        //if (dup_flag) return;

        response.setFINbit(true);
        response.setACK(pkt.getSEQ());
        response.setSEQ(2*HEADER_SIZE);
        buffer = response.packet_to_string();
        buff_size = buffer.length();
        write(sockfd, buffer.c_str(), buff_size);
        dup_flag = true; //We have received FIN
        
        //set the timer, and add it to the list
        short seqnum = response.getSEQ();
        pkt_timer[seqnum] = my_timer(seqnum, response);
        makeTimer(&pkt_timer[seqnum], 2*TIMEOUT);


        //If it is a 404 error, we print additional message
        if (pkt.getERRbit())
            printf("404 Not Found\n");
        printmessage("send", "FIN", response.getACK());
        //close connection for 2RTO
    }

    //receive REQ from server, close connection
    else if (pkt.getREQbit()){
        close(sockfd);
        exit(1);
        //close connection
    }
    else{
        dup_flag = false;
        process_regular_packet(pkt);
    }

}


int main(int argc, char *argv[])
{
    int portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char* hostname;

    if (argc < 4) {
        fprintf(stderr,"ERROR, missing server hostname or portnumber of file\n");
        exit(1);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create socket

    if (sockfd < 0)
        error((char *)"ERROR opening socket");

    server = gethostbyname(hostname);

    if (server == NULL) 
        error((char *)"ERROR getting host server");


    memset((char *) &serv_addr, 0, sizeof(serv_addr));   // reset memory

    // fill in address info
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memmove((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        error((char *)"ERROR connecting to server");


//////////////////


    char buffer[BUFF_SIZE];

    memset(buffer, 0, BUFF_SIZE);  // reset memory

    struct pollfd pfd[1]; // create a poll to run the server constantly
    pfd[0].fd = sockfd;
    pfd[0].events = POLLIN;

    //send initial SYN pkt
    Packet syn;
    syn.setSYNbit(true);
    syn.setSEQ(SYN_Seq);
    std::string syn_string = syn.packet_to_string();
    //write(STDOUT_FILENO, syn_string.c_str(). syn_string.size());
    printmessage("send", "SYN", syn.getSEQ());
    write(sockfd, syn_string.c_str(), syn_string.length());

    //set the timer, and add it to the list
    short seqnum = syn.getSEQ();
    pkt_timer[seqnum] = my_timer(seqnum, syn);
    makeTimer(&pkt_timer[seqnum], TIMEOUT);

    while(1)
    {
        int ret = poll(pfd, 1, 0);
        if (ret > 0)
        {

            //printf("pfd  -----  events : %d,  revents : %d       POLLIN = %d, POLLERR = %d, POLLHUP = %d  \n", pfd[0].events, pfd[0].revents, POLLIN, POLLERR, POLLHUP);

            if (pfd[0].revents & POLLIN)
            {
                int read_bytes = read(sockfd, buffer, BUFF_SIZE);
                
                if (read_bytes < 0)
                {
                    fprintf(stderr, "Read from socket fails. Error: %s\n", strerror(errno));
                    exit(1);
                }
		
		        //printf("read bytes are %d\n", read_bytes);
		        if (read_bytes == 0)
		        {
		            close(sockfd);
		            break;
		        }

                //printf("Here is the message:\n");
                //write(STDOUT_FILENO, buffer, read_bytes);
                    
                
                //reply to server
                if (*buffer){
                    std::string conv="";
                    for (int i = 0; i < read_bytes; i++)
                        conv += buffer[i];
                    Packet temp(conv);
                    process_packet(temp);
                    memset(buffer, 0, BUFF_SIZE);  // reset memory after one request
                }

            }

            if (pfd[0].revents & (POLLHUP+POLLERR))
                break;
        }
    }

    close(sockfd);  // close connection

    return 0;
}
