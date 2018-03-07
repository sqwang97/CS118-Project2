#include <string>
#include <iostream>
#include "packet.h"
#include "buffer.h"
#include <string>
#include <sstream> 
#include <fstream>

using namespace std;

int main(){
	//std::string testcase = "0 0 0 -1 -1 5 Test.";
	Packet A;
	Packet B;
	Packet C;
	short seq1 = 1024;
	short seq2 = seq1+HEADER_SIZE+1;
	short seq3 = seq2+HEADER_SIZE+5;
	A.fillin_content("A");				A.setSEQ(seq1);
	B.fillin_content("BBBBB");			B.setSEQ(seq2);
	C.fillin_content("CCCCCCCCC");		C.setSEQ(seq3);
	string oA = A.packet_to_string();
	string oB = B.packet_to_string();
	string oC = C.packet_to_string();
	
	Buffer X(5);
	X.setBaseSeq(seq1);
	X.push_in_pkt(seq1, oA);

	cout << X.drop_packet() <<endl;

	X.push_in_pkt(seq3, oC);

	cout << X.drop_packet() << endl;

	X.push_in_pkt(seq2, oB);

	cout << X.drop_packet() << endl << X.drop_packet() << endl;
	cout << X.isEmpty() << endl;
}