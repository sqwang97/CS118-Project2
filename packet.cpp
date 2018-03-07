#include <string>
#include "packet.h"

Packet::Packet(){
	SYNbit = '0';
	FINbit = '0';
	ERRbit = '0';
    REQbit = '0';

    SEQ = 0;
    ACK = 0;
    
    data = "";
}

Packet::Packet (std::string pkt){
	SYNbit = pkt[0];
	FINbit = pkt[1];
	ERRbit = pkt[2];
	REQbit = pkt[3];

	SEQ = (pkt[4] << 8) + pkt[5];
	ACK = (pkt[6] << 8) + pkt[7];
	if (pkt.size() > HEADER_SIZE)
		fillin_content(pkt.substr(HEADER_SIZE));
}

void Packet::setSYNbit(bool S){
	if(S) SYNbit = '1';
	else SYNbit = '0';
}

void Packet::setFINbit(bool F){
	if(F) FINbit = '1';
	else FINbit = '0';
}

void Packet::setERRbit(bool E){
	if(E) ERRbit = '1';
	else ERRbit = '0';
}

void Packet::setREQbit(bool E){
    if(E) REQbit = '1';
    else REQbit = '0';
}

bool Packet::setSEQ(short Seqnum){
	if (Seqnum < 0 || Seqnum >= MAXSEQNUM)
		return false;
	else
		SEQ = Seqnum;
	return true;
}

bool Packet::setACK(short ACKnum){
	if (ACKnum < 0 || ACKnum >= MAXSEQNUM)
		return false;
	else
		ACK = ACKnum;
	return true;
}

bool Packet::fillin_content(std::string content){
	int len = content.length();
	if (len > PAYLOADSIZE)
		return false;
	else
		data = content;
	return true;
}

std::string Packet::packet_to_string (){
	std::string result(1, SYNbit);
	result += FINbit;
	result += ERRbit;
	result += REQbit;

	//[seq_high | seq_low]
	char seq_high = ((unsigned short) SEQ) >> 8;
	char seq_low = SEQ - (seq_high << 8);

	char ack_high = ((unsigned short) ACK) >> 8;
	char ack_low = ACK - (ack_high << 8);

	result += seq_high;		result += seq_low;
	result += ack_high;		result += ack_low;

	result += data;
	return result;
}

