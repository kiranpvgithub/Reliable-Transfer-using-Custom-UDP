#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>

#define SIZEOFPACKETS 1000
#define SENDDELAY 80

volatile int start = 0;
int packets=0;
volatile int resend=0;
pthread_t udpsend, udpresend, tcp_nak, tcp_nop; 
unsigned char *buffer=NULL;
void *tcp_noofpackets(void* hos);
void *tcp_nakclient(void* arg);
void *sendpak(void* hos);
void *resendpak(void * hos);
char *filename;


typedef struct udppacket {
	int seqno;
	char data[SIZEOFPACKETS];
}udp_packet;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {

	if (argc != 3) {
		printf("Usage: ./client host filename\n");
		exit(1);
	}

	FILE *fp;
	struct stat st;
	stat(argv[2], &st);
	int size = st.st_size;

	packets = (size % SIZEOFPACKETS != 0) ? (size/SIZEOFPACKETS) +1 : size/SIZEOFPACKETS;

	buffer=(unsigned char *) calloc(packets+1, sizeof(char));
	buffer[packets]='\0';

	struct hostent *hostname;	
	hostname = gethostbyname(argv[1]);

	filename = argv[2];
	if (hostname==0) error("Unknown host");

	pthread_create(&tcp_nop, 0, tcp_noofpackets, hostname);
	
	pthread_create(&tcp_nak, 0, tcp_nakclient, NULL);
	pthread_join(tcp_nop, NULL);

	pthread_create(&udpsend, 0, sendpak, hostname);
	pthread_create(&udpresend, 0, resendpak, hostname);
	pthread_join(udpsend, NULL);
	pthread_join(udpresend, NULL);
	pthread_join(tcp_nak, NULL);
	
	//free(buffer);
	pthread_exit(0);
	return 0;
}

void *tcp_noofpackets(void* hos) {

	struct hostent *hostname = (struct hostent *) hos;

	int sockfd, n;
	struct sockaddr_in serv_addr;

	unsigned int  *buff=&packets;


	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	if (hostname == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}
	
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

	bcopy((char *) hostname->h_addr,
	(char *)&serv_addr.sin_addr.s_addr,
	hostname->h_length);
	serv_addr.sin_port = htons(20000);
	
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");

	//send packet count
	n = write(sockfd, buff, 4);

	if (n < 0)
		error("ERROR writing to socket");

	//send file name to server
	n = write(sockfd, filename, 100);
	while (start == 0) {
		usleep(100);
	}
	close(sockfd);
	return NULL;
}

unsigned char* byte_bit(unsigned char *inputByte, int lastFlag) {
	unsigned char *output = NULL;
	int currentByte = 0;
	if (lastFlag == 0) {
		output = (unsigned char*)calloc(8000,sizeof(char));
		int i, j = 8;
		for(i = 0; i < 1000; i++) {
			output[i*j] = (inputByte[i] & 0x80) ? 1:0;
			output[i*j+1] = (inputByte[i] & 0x40) ? 1:0;
			output[i*j+2] = (inputByte[i] & 0x20) ? 1:0;
			output[i*j+3] = (inputByte[i] & 0x10) ? 1:0;
			output[i*j+4] = (inputByte[i] & 0x08) ? 1:0;
			output[i*j+5] = (inputByte[i] & 0x04) ? 1:0;
			output[i*j+6] = (inputByte[i] & 0x02) ? 1:0;
			output[i*j+7] = (inputByte[i] & 0x01) ? 1:0;
		}
		return output;
	}
	//last nack packet
	else {
		int lastNackSize = (packets % 1000) == 0 ? 1000 : packets % 1000;
		output = (unsigned char*)calloc(lastNackSize*8,sizeof(char));
		int i, j = 8;
		for(i = 0; i < lastNackSize; i++) {
			output[i*j] = (inputByte[i] & 0x80) ? 1:0;
			output[i*j+1] = (inputByte[i] & 0x40) ? 1:0;
			output[i*j+2] = (inputByte[i] & 0x20) ? 1:0; 
			output[i*j+3] = (inputByte[i] & 0x10) ? 1:0; 
			output[i*j+4] = (inputByte[i] & 0x08) ? 1:0; 
			output[i*j+5] = (inputByte[i] & 0x04) ? 1:0; 
			output[i*j+6] = (inputByte[i] & 0x02) ? 1:0; 
			output[i*j+7] = (inputByte[i] & 0x01) ? 1:0;
		}
		return output;
	}
}


void *tcp_nakclient(void* arg) {
	unsigned char rnak[packets];
	memset(rnak, 1, packets);
	int sockfd;//, newsockfd;
	struct sockaddr_in from;
	socklen_t clilen;

	struct sockaddr_in serv_addr, cli_addr;

	int n;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd < 0) {
		error("ERROR opening socket");
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;

	serv_addr.sin_addr.s_addr = INADDR_ANY;

	serv_addr.sin_port = htons(22000);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
		error("ERROR on binding");
	}
	socklen_t fromlen = sizeof(struct sockaddr_in);
	clilen = sizeof(cli_addr);
	start = 1;

	//get nack header size
	int nackHeader = ((packets/8) % 1000) == 0 ? (packets/8) / 1000 : (packets/8) / 1000 + 1;
	int lastNackSize = (packets % 1000) == 0 ? 1000 : packets % 1000;
	char* currentNack = (char*)calloc(1001,sizeof(char));
	unsigned int currentHeader = 0;
	char* dataPacketBit = (char*)calloc(8000,sizeof(char));
	unsigned char* tempNack = (char*)calloc(1001,sizeof(char));
	int nakCount = 0;
	unsigned char *temptemp = NULL;

	unsigned char* checkHeaderBit = (unsigned char*)calloc(nackHeader,sizeof(char));
	memset(checkHeaderBit,0,nackHeader);
	while (memcmp(buffer, rnak, packets) != 0) {
		n = recvfrom(sockfd, tempNack, 1001,0,(struct sockaddr *)&from,&fromlen);
		int i;
		currentHeader = tempNack[0];

		//not last Nack header
		if (currentHeader != nackHeader-1) {
			temptemp = byte_bit(tempNack+1,0);
			int i;
			for (i = 0; i < 8000; i++) {
				buffer[currentHeader*8000+i] = temptemp[i];
			}		
		}
		else {
			temptemp = byte_bit(tempNack+1,1);
			int i;
			for (i = 0; i < lastNackSize*8; i++) {
				buffer[currentHeader*lastNackSize+i] = temptemp[i];
			}
		}
		//reactivate resend thr
		
		if (checkHeaderBit[currentHeader] == 0) {
			checkHeaderBit[currentHeader] = 1;
			nakCount++;
		} 
		if (nakCount == nackHeader) {
			memset(checkHeaderBit,0,nackHeader);
			nakCount = 0;
			resend = 1;
			usleep(10000);
		}
		usleep(80);
		if (n < 0)
			error("ERROR reading from socket");
	}
	close(sockfd);
	//reactivate resend thr
	pthread_cancel(udpresend);
	exit(0);
	return NULL;
}





void *sendpak(void * hos) {
	int sock, nopak,nr=0;
	unsigned int length;
	struct sockaddr_in server;
	struct hostent *hp = (struct hostent *) hos;
	char buf[SIZEOFPACKETS+4];

	udp_packet *udp_sendpak;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)  error("socket");
	
	server.sin_family = AF_INET;
	if (hp == 0) error("Unknown host");

	bcopy((char *) hp->h_addr,(char *)&server.sin_addr,hp->h_length);
	server.sin_port = htons(21000);

	length = sizeof(struct sockaddr_in);
	FILE *fp = fopen("/tmp/x.txt","rb");
	udp_sendpak = (udp_packet*) calloc(SIZEOFPACKETS+4,sizeof(char));
	usleep(1000);
	//send file sequentially
	for (nopak = 0; nopak < packets; nopak++) {

		if(udp_sendpak!=NULL)
		{	
			
			if((nr = fread(buf+4,sizeof(char),SIZEOFPACKETS,fp))>0)
			{
		 		memmove(&buf[0],&nopak,4);
				sendto(sock, buf, nr+4, 0,(const struct sockaddr *)&server, length);
				usleep(SENDDELAY);
				
			}		
		}
	}
	close(sock);
	pthread_exit(0);
}





void *resendpak(void * hos) {
	while (resend == 0) {
		usleep(100);
	}

	unsigned char rnak[packets];
	memset(rnak, 1, packets);
	int sock, nopak;
	unsigned int length;
	struct sockaddr_in server;
	struct hostent *hp=(struct hostent *) hos;
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0) error("socket");

	server.sin_family = AF_INET;
	if (hp == 0) error("Unknown host");

	bcopy((char *) hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	server.sin_port = htons(21000);
	length = sizeof(struct sockaddr_in);
	FILE *fp=fopen("/tmp/x.txt","rb");
	int nr=0;
	char buf[SIZEOFPACKETS+4];
	int pos = 0;
	udp_packet* udp_sendpak = (udp_packet*) calloc(1,sizeof(udp_sendpak));	
	int i = 0;
	while (memcmp(buffer, rnak, packets) != 0) {
		for (nopak = 0; nopak < packets; nopak++) {
			if (((int)buffer[nopak]) == 0) {
				if (udp_sendpak!=NULL){
					pos = nopak * SIZEOFPACKETS;	
					if (fseek(fp, pos, SEEK_SET)<0)
					{
						error("wrong cursor position\n");
					}
					
					if((nr = fread(buf+4,sizeof(char),SIZEOFPACKETS,fp))>0)
					{				
						memmove(&buf[0],&nopak,4);
						sendto(sock, &buf[0], nr+4, 0, (const struct sockaddr *) &server, length);
						usleep(SENDDELAY);
					}
				}
			}
		}
		resend = 0;
		while (resend == 0) {
			usleep(100);
		}
	}
	close(sock);
	return NULL;
}



