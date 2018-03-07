
#define BUFF_SIZE 1024		//bytes
#define HEADER_SIZE 8		//bytes
#define PAYLOADSIZE BUFF_SIZE-HEADER_SIZE	//bytes
#define MAXSEQNUM 30720		//bytes
#define WINDOWSIZE 5120		//bytes
#define TIMEOUT 500			//ms

class Packet{
public:
	Packet();
	/* decode a string into a packet */
	Packet(std::string pkt);

	/* set private variables */
	void setSYNbit(bool S);
	void setFINbit(bool F);
	void setERRbit(bool E);
	void setREQbit(bool E);
	bool setSEQ(short Seqnum);
	bool setACK(short ACKnum);
	/* fill in content, if file size > payload size, return false */
	bool fillin_content(std::string content);

	/* convert a packet struct into a string to allow transmit through network */
	std::string packet_to_string ();

	/* get private variables */
	bool getSYNbit() {return SYNbit-'0';}
	bool getFINbit() {return FINbit-'0';}
	bool getERRbit() {return ERRbit-'0';}
	bool getREQbit() {return REQbit-'0';}
	short getSEQ()	{return SEQ;}
	short getACK()	{return ACK;}
	std::string get_content() {return data;}

private:
	//header size: 4 + 2 + 2 bytes 
	char SYNbit;	//0 or 1
	char FINbit;	//0 or 1
	char ERRbit;	//file cannot open or doesn't exist: 0 or 1()
	char REQbit;    //true if server receives requesting file name; if client receives close connecting ack

    short SEQ;		//-1 or num
    short ACK;		//-1 or num

    //file content
    std::string data;
};