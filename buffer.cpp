#include <string>
#include <list>
#include <algorithm>
#include "buffer.h"
#include "packet.h"

Buffer::Buffer(int size){
	buffer_size = size;
	expected_seq = last_expected_to_drop_seq = 0;
}

bool Buffer::isEmpty(){
	return pkt_buffer.empty();
}

void Buffer::setBaseSeq(short seqnum){
	expected_seq = last_expected_to_drop_seq = seqnum;
}


bool Buffer::push_in_pkt(short seqnum, std::string pkt){
	//printf("* * * * * * * * * Pushing into buffer --- seqnum %d   exp_seq %d   W+B %d\n", seqnum, expected_seq, WINDOWSIZE+ BUFF_SIZE);
	if ((seqnum < expected_seq && expected_seq - seqnum < WINDOWSIZE+ BUFF_SIZE)
		|| (seqnum > expected_seq && seqnum - expected_seq  > WINDOWSIZE + BUFF_SIZE)){//a retransmitted duplicate pkt, consider over flow
		//printf("* * * * * * * * * Return false, reason 1\n");
		return false;					//for client just drop the duplicate and return false to notice
	}
	else if (find_seq(seqnum) != pkt_buffer.end()){	//duplicate pkt in buffer
		//printf("* * * * * * * * * Return false, reason 1\n");
		return false;
	}
	else if (seqnum == expected_seq){		//a delivered in order pkt
		struct pkt temp;
		temp.seq = seqnum;
		temp.pkt = pkt;
		pkt_buffer.push_back(temp);
		expected_seq = (expected_seq + pkt.length()) % MAXSEQNUM;
	}
	else{									//an out-of-order pkt
		struct pkt temp;
		temp.seq = seqnum;
		temp.pkt = pkt;
		pkt_buffer.push_back(temp);
	}
	//printf("* * * * * * * * * Return true\n");
	return true;
}

std::string Buffer::drop_packet(){
	std::string result;
	std::list<struct pkt>::iterator it;
	//printf("1: %d ----- %d\n", last_expected_to_drop_seq, expected_seq);
	it = find_seq(last_expected_to_drop_seq);
	//printf("%d ----- %d\n", last_expected_to_drop_seq, expected_seq);

	if (it != pkt_buffer.end()){
		result = it->pkt;
		pkt_buffer.erase(it);

		
		it = find_seq(last_expected_to_drop_seq);
		int is_erased = (it == pkt_buffer.end());
		//printf(">>>>>>>>Is erased????? %d\n", is_erased);
		

		last_expected_to_drop_seq = (last_expected_to_drop_seq + result.length()) % MAXSEQNUM;
		//set expected_seq if the droped packet is out of order

		/*
		if ((expected_seq > last_expected_to_drop_seq && expected_seq - last_expected_to_drop_seq > WINDOWSIZE + BUFF_SIZE))
		 	|| (last_expected_to_drop_seq > expected_seq && last_expected_to_drop_seq - expected_seq > WINDOWSIZE + BUFF_SIZE))  //consider overflow
        	expected_seq = std::min(expected_seq, last_expected_to_drop_seq);
    	else
        	expected_seq = std::max(expected_seq, last_expected_to_drop_seq);
        */
        if (last_expected_to_drop_seq != expected_seq)
        	expected_seq = last_expected_to_drop_seq;
		return result;
	}
	return "";
}

std::list<struct Buffer::pkt>::iterator Buffer::find_seq(short A){
	std::list<struct pkt>::iterator it;
    it = pkt_buffer.begin();
	while (it != pkt_buffer.end()){
		if (A == it->seq)
			return it;
		it++;
	}
	return it;
}

