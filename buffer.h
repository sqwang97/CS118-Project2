/*
need:
1. update a Seqnum, hash function: index = ((SeqNum-Base_SeqNum)/BUFF_SIZE)/(WINDOWSIZE/BUFF_SIZE)
	if SeqNum[index] = input, denote a retransmission, return false
	else if SeqNum[index] != "" && SeqNum[index] != input, drop current pkt, return true
	else store Seqnum in SeqNum[index], return true
3. upon out of order packet is delivered, should be able to push buffered pkts 

*/
#include <list>

#define BUFF_SIZE 1024		//bytes

class Buffer{
public:
	Buffer(int size);

	bool isEmpty();

	//call before push in the first pkt 
	void setBaseSeq(short seqnum);

	//client: everytime receive a pkt
	//server: everytime send a pkt
	//will return false only when client receives a duplicate pkt
	bool push_in_pkt(short seqnum, std::string pkt);

	//use when server want to retransmit a pkt
	std::string get_pkt(short seqnum);

	//for client only: used when drop the pkt to file receieve.data
	std::string drop_packet();

	//for server only: used when receive an ACK from client, discard this pkt
	void drop_packet(short seqnum);

private:
	short last_expected_to_drop_seq;
	//the next pkt that need to be received
	short expected_seq;
	int buffer_size;

	struct pkt{
		short seq = -1;
		std::string pkt;
	};
	std::list<struct pkt> pkt_buffer;
	std::list<struct pkt>::iterator find_seq(short A);
};

