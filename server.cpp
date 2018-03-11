
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
#include <vector>
#include <queue>
#include <unordered_set>
#include <climits>

#include <iostream>
#include <string>
#include <sstream> 
#include <fstream>

#include "packet.h"

const short SYN_Seq = 0;

std::string file_data = "";
unsigned long file_data_size = 0;
unsigned long data_offset = 0;

int sockfd = -1;

short Max_seq = 0;
short Pre_seq = 0;
short seq_window_head = 0;

//current window design, used for priority queue
struct window {
    window(short nseqnum, unsigned long noffset, short nlength)
    {
        seqnum = nseqnum;
        offset = noffset;
        length = nlength;
    }

    window() {}

    //return true, put w2 in front of w1, pkt with larger seq number < pkt with smaller seq number 
    bool operator<(const window& w2) const
    {
        // regular pkt
        if (seqnum > w2.seqnum && seqnum - w2.seqnum < WINDOWSIZE + BUFF_SIZE) return true;
        // wrap around pkt
        if (seqnum < w2.seqnum && w2.seqnum - seqnum > WINDOWSIZE + BUFF_SIZE) return true;
        return false;
    }

    short seqnum;
    unsigned long offset;
    short length;
};

std::priority_queue<window> current_window;
std::unordered_set<short> ackbuf;

void error(char *msg)
{
    perror(msg);
    exit(1);
}



std::string process_content(std::string filenm) {
    std::ifstream myfile(filenm.c_str(), std::ifstream::in | std::ifstream::binary);
    std::ostringstream content;
    if (myfile){
        content << myfile.rdbuf();
        myfile.close();
        return content.str(); 
    }
    return "";
}

bool read_in (std::string filenm){
   
    std::string file_buffer = "";

    //test if file exist
    std::ifstream file(filenm.c_str());
    if (file){
        file_data = process_content(filenm);
        if (file_data.compare("")) {  //we have the data      empty file???
            file_data_size = file_data.size(); 
            file.close();
            return true;
        }
    }
    
    file.close();
    return false;
}
////////////////////////

void updateMaxSeq(short seqnum){
    if (Max_seq - seqnum > WINDOWSIZE + BUFF_SIZE || seqnum - Max_seq > WINDOWSIZE + BUFF_SIZE)  //consider overflow
        Max_seq = std::min(Max_seq, seqnum);
    else
        Max_seq = std::max(Max_seq, seqnum); 
}

void printmessage(std::string action, std::string state, short num){
    if (!action.compare("send"))
        printf("--------->Sending packet %d %d %s\n", num, WINDOWSIZE, state.c_str());    
    else if (!action.compare("receive"))
        printf("-------------->Receiving packet %d\n", num);
    else
        fprintf(stderr, "error input.");
}

void send_regular_packet(short seqnum, unsigned long offset, int payload, struct sockaddr_in src_addr, socklen_t addrlen){
    Packet response;
    std::string buffer;
    int buff_size;
    window current(seqnum, offset, payload);
    current_window.push(current);
    response.setSEQ(seqnum);
    //Pre_seq = Max_seq;
    response.fillin_content(file_data.substr(offset, payload));
    buffer = response.packet_to_string();
    buff_size = buffer.length();
    //printf("%s\n", buffer.c_str());
    sendto(sockfd, buffer.c_str(), buff_size, 0, (struct sockaddr*)&src_addr, addrlen);
    //data_offset += PAYLOADSIZE;
    //printmessage("send", "", response.getSEQ());
    //updateMaxSeq( (pkt.getSEQ() + BUFF_SIZE) % BUFF_SIZE);
}


void process_regular_ack(Packet& pkt, struct sockaddr_in src_addr, socklen_t addrlen){
    Packet response;
    std::string buffer;
    int buff_size;

    if (pkt.getACK() == current_window.top().seqnum){  //We receive an in order ACK
        current_window.pop();
        if (current_window.empty() && ackbuf.empty()){  //This is the last ACK for file data, and we have received all ACKs before
            response.setFINbit(true);
            response.setSEQ(Max_seq);
            Pre_seq = Max_seq;
            buffer = response.packet_to_string();
            buff_size = buffer.length();
            sendto(sockfd, buffer.c_str(), buff_size, 0, (struct sockaddr*)&src_addr, addrlen);
            printmessage("send", "FIN", response.getSEQ());
            Max_seq = (Max_seq + HEADER_SIZE) % MAXSEQNUM;
        }
        else if (data_offset + PAYLOADSIZE <= file_data_size) {  //We can send PAYLOADSIZE data
            seq_window_head = current_window.top().seqnum;  //Update window head Seq
            send_regular_packet(Max_seq, data_offset, PAYLOADSIZE, src_addr, addrlen);
            Pre_seq = Max_seq;
            data_offset += PAYLOADSIZE;
            printmessage("send", "", Max_seq);
            Max_seq = (Max_seq + BUFF_SIZE) % MAXSEQNUM;
        }
        else if (data_offset < file_data_size) {  //We send the rest of data, and try to catch ACK to send FIN
            seq_window_head = current_window.top().seqnum;  //Update window head Seq
            int rest_length = file_data_size - data_offset;
            send_regular_packet(Max_seq, data_offset, rest_length, src_addr, addrlen);
            Pre_seq = Max_seq;
            data_offset += rest_length;
            printmessage("send", "", Max_seq);
            Max_seq = (Max_seq + rest_length + HEADER_SIZE) % MAXSEQNUM;
        } else 
	    seq_window_head = current_window.top().seqnum;

        if(!ackbuf.empty())  //check out of order buffer, one at a time
        {
            std::unordered_set<short>::iterator it;
	    seq_window_head = current_window.top().seqnum;
            it = ackbuf.find(seq_window_head);
            if (it != ackbuf.end())  //We have buffered this ACK before, so we want to go for next
            {
                ackbuf.erase(it);  //delete this buffer
                Packet simulator;  //Simulate we received this ACK packet
                simulator.setACK(seq_window_head);
                process_regular_ack(simulator, src_addr, addrlen);
            }
        }

    }   
    else{  //We receive an out of order ACK
        ackbuf.insert(pkt.getACK());
    }


}


void process_packet (Packet& pkt, struct sockaddr_in src_addr, socklen_t addrlen){
    printmessage("receive", "", pkt.getACK() );
    Packet response;
    std::string buffer;
    int buff_size;
    //receive SYN from client, need to send back SYN and ACK to client
    //// need to buffer this packet, if lost, need timeout to retransmit ////
    if (pkt.getSYNbit()){
        response.setSYNbit(true);
        response.setSEQ(SYN_Seq);
        Pre_seq = SYN_Seq;        
        response.setACK(pkt.getSEQ());
        buffer = response.packet_to_string();
        buff_size = buffer.length();
        sendto(sockfd, buffer.c_str(), buff_size, 0, (struct sockaddr*)&src_addr, addrlen);
        printmessage("send", "SYN", response.getSEQ());
        Max_seq = (Max_seq + HEADER_SIZE) % MAXSEQNUM;    //// do we need this? ////       
    }

    //receive FIN from client, send ACK to client, and close connection
    else if (pkt.getFINbit()){
        if (pkt.getACK() == Pre_seq) {
            //printf("<---------- Get Here -------------->\n");
            response.setREQbit(true);
            response.setSEQ(Max_seq);
            Pre_seq = Max_seq;
            buffer = response.packet_to_string();
            buff_size = buffer.length();
            sendto(sockfd, buffer.c_str(), buff_size, 0, (struct sockaddr*)&src_addr, addrlen);
            printmessage("send", "", response.getSEQ());
            Max_seq = (Max_seq + HEADER_SIZE) % MAXSEQNUM;    //// do we need this? ////
            close(sockfd);
            exit(1);
        }
    }

    //receive REQ from client, load data and send first N packets with data to client
    else if (pkt.getREQbit()){
        if (pkt.getACK() == Pre_seq)
        {
            if (read_in(pkt.get_content())) //we have the file, so we initial sending N packets
            {
		        int pkt_nums = 0;
                int modbit1 = 0, modbit2 = 0;
                if (file_data_size % PAYLOADSIZE > 0)
                    modbit1 = 1;
                if (WINDOWSIZE % BUFF_SIZE > 0)
                    modbit2 = 1;
		        if (file_data_size > (PAYLOADSIZE * (WINDOWSIZE / BUFF_SIZE))){ 
		            pkt_nums = ((int) WINDOWSIZE) / ((int) BUFF_SIZE) + modbit2;
		            //printf("pkt_num is %d\n", pkt_nums);
		        }           
		        else {
		            pkt_nums = ((int) file_data_size) / ((int) PAYLOADSIZE) + modbit1;
		            //printf("pkt_num is %d\n", pkt_nums);
                }
                seq_window_head = Max_seq;
                for (int i = 0; i < pkt_nums; i++) {
                    if (data_offset + PAYLOADSIZE <= file_data_size) {  //We can send PAYLOADSIZE data
                        send_regular_packet(Max_seq, data_offset, PAYLOADSIZE, src_addr, addrlen);
                        Pre_seq = Max_seq;
                        data_offset += PAYLOADSIZE;
                        printmessage("send", "", Max_seq);
                        Max_seq = (Max_seq + BUFF_SIZE) % MAXSEQNUM;
                    }
                    else {  //We send the rest of data, and try to catch ACK to send FIN
                        int rest_length = file_data_size - data_offset;
                        send_regular_packet(Max_seq, data_offset, rest_length, src_addr, addrlen);
                        Pre_seq = Max_seq;
                        data_offset += rest_length;
                        printmessage("send", "", Max_seq);
                        Max_seq = (Max_seq + rest_length + HEADER_SIZE) % MAXSEQNUM;
                    }
                }

            }
            else {  //we do not have that file
                response.setERRbit(true);
                response.setFINbit(true);
                response.setSEQ(Max_seq);
                Pre_seq=Max_seq;
                buffer = response.packet_to_string();
                buff_size = buffer.length();
                sendto(sockfd, buffer.c_str(), buff_size, 0, (struct sockaddr*)&src_addr, addrlen);
                printmessage("send", "FIN", response.getSEQ());
                Max_seq = (Max_seq + HEADER_SIZE) % MAXSEQNUM;
            }
        }        
    }

    /*
    else if (pkt.getEndflag()){
        // 
        Examine whether the buffer is empty,
        if empty, send FIN;
        else
        return; (wait retrans from server)
        //
        process_regular_packet(pkt, sockfd);
        if (pkt_buffer.isEmpty()){
            response.setFINbit(true);
            response.setACK(pkt.getSEQ());
            response.setSEQ(Max_seq+BUFF_SIZE);
            buffer = response.packet_to_string();
            buff_size = buffer.length();
            write(sockfd, buffer.c_str(), buff_size);
            printmessage("send", "FIN", response.getACK());
        }
    } */
    else
        process_regular_ack(pkt, src_addr, addrlen);

}



int main(int argc, char *argv[])
{
    int portno;
    struct sockaddr_in serv_addr;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create UDPsocket
    if (sockfd < 0)
        error((char *)"ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));   // reset memory

    // fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error((char *)"ERROR on binding");


////////////////////


    char buffer[BUFF_SIZE];

    memset(buffer, 0, BUFF_SIZE);  // reset memory

    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    //int read_request = recvfrom(sockfd, buffer,BUFF_SIZE, 0, (struct sockaddr*)&src_addr, &addrlen);
    ////find file, if ERR, return 404////
    ////if file exists, continue to sending packets of data////

    //sending packets of data
    struct pollfd pfd[1]; // create a poll to set timeout for packets
    pfd[0].fd = sockfd;
    pfd[0].events = POLLIN;
    while(1)
    {
        int ret = poll(pfd, 1, 0); //timeout is 500ms
        if (ret > 0)
        {

            if (pfd[0].revents & POLLIN)
            {
                int read_bytes = recvfrom(sockfd, buffer, BUFF_SIZE, 0, (struct sockaddr*)&src_addr, &addrlen);
                
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

                
                //write(STDOUT_FILENO, buffer, read_bytes);  //write information
                    
                
                //reply to client
                if (*buffer){
                    std::string conv="";
                    for (int i = 0; i < read_bytes; i++)
                        conv += buffer[i];
                    Packet temp(conv);
                    process_packet(temp, src_addr, addrlen);
                    memset(buffer, 0, BUFF_SIZE);  // reset memory after one request
                }

            }

            /*
            if (pfd[0].revents & (POLLHUP+POLLERR))
                break;
            */
        }

    }

    close(sockfd);  // close connection

    return 0;
}
