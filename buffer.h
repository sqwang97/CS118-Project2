#include <list>

#define BUFF_SIZE 1024		//bytes

//client buffer
class Buffer{
public:
	Buffer(int size);

	bool isEmpty();

	//call before push in the first pkt 
	void setBaseSeq(short seqnum);

	//called when everytime receive a pkt
	//will return false only when client receives a duplicate pkt
	bool push_in_pkt(short seqnum, std::string pkt);

	//used when drop the pkt to file receieve.data
	std::string drop_packet();

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

