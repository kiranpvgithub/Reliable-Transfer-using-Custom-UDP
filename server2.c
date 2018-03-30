/* Creates a datagram server.  The port 
   number is passed as an argument.  This
   server runs forever */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>

#define TCPgetPacketCount 20000
#define BUFFERSIZE 1000 
#define nakReceiver 22000
#define data BUFFERSIZE

//Gobal variable
int packetCount=0, currCount=0, tcpFlag=0 , nakFlag=0;
unsigned char *dataPacket;
pthread_t getCount, receciveDataUDP, nakDataUDP;

unsigned long elap;
struct timeval tv0, tv1;
char *filename;

//Print Error meassgae
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

//First TCP connection is made to get packet count
void *getPacketCount() {
     int sockfd, newsockfd, portno, pid,n;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     char buf[100];
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(TCPgetPacketCount);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
     newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);
     if (newsockfd < 0) 
             error("ERROR on accept");
     n = read(newsockfd, buf, 4);
     if (n < 0) error("ERROR reading from socket");
     memmove(&packetCount,buf,4);
     n = read(newsockfd, buf, 100);
     if (n < 0) error("ERROR reading from socket");
     filename = (char*)malloc(100*sizeof(char));     
     strcpy(filename, buf);
     gettimeofday(&tv0,0);
     close(sockfd);
     tcpFlag=1;
}



void *receiveData() {
	
	int  currentSeq = 0, previousSeq = 0, fpos;
	unsigned char *testData;
	int sock, length, n, i;
	FILE *fp;
	void* tempBuffer = NULL;
	int receiveCount = 0;

	dataPacket = (unsigned char*)malloc(packetCount*sizeof(char));
	testData = (unsigned char*)malloc(packetCount*sizeof(char));
	memset(dataPacket, 0, packetCount);
	memset(testData, 1, packetCount);
	
        tempBuffer = (void*)malloc(BUFFERSIZE+4*sizeof(char));
	char buf[data];
	
	//Open the file to write to
	fp=fopen("/tmp/x2.txt","wb");

	socklen_t fromlen;
	struct sockaddr_in server;
	struct sockaddr_in from;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		error("Opening socket");
	}
	
	length = sizeof(server);
	bzero(&server, length);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	
	server.sin_port = htons(21000);

	if (bind(sock, (struct sockaddr *) &server, length) < 0) {
		error("binding");
	}

	fromlen = sizeof(struct sockaddr_in);
	while (1) {
		if((memcmp(dataPacket,testData,packetCount)) == 0) {
			break;
		}
		n = recvfrom(sock, tempBuffer, BUFFERSIZE+4, 0, (struct sockaddr *)&from, &fromlen);
		if (n < 0) {
			error("recvfrom");
		}
		memmove(&currentSeq,tempBuffer,4);
		if (dataPacket[currentSeq] == 1) {
			continue;
		} else {
			dataPacket[currentSeq] = 1;
		}
		if (nakFlag == 0) {
			if (currentSeq > packetCount - 10000) nakFlag = 1;
		}
		/* Need to write to the file*/
		fpos = currentSeq * data;
		if (fseek(fp, fpos, SEEK_SET) < 0) {
			error("fseek: \n");		
		}
		fwrite(tempBuffer+4,sizeof(char),n-4,fp);
	}
	close(sock);
	fclose(fp);
	free(tempBuffer);
}


unsigned char* bit_byte(unsigned char *dataPacket) {
	int dataPacketByteCount = packetCount%8 == 0 ? packetCount/8 : packetCount/8 + 1;
	int lastFlag = packetCount%8 == 0 ? 0: 1;
	int lastByteBitCount = packetCount%8;
	unsigned char *output = (unsigned char*)calloc(dataPacketByteCount,sizeof(char));
	int i, j = 0;
	int z[8] = {128,64,32,16,8,4,2,1};
	for (i = 0; i < packetCount; i+=8) {
			if (j == dataPacketByteCount-1 && lastFlag == 1) {
				int sum = 0;
				int k;
				for (k = 0; k < lastByteBitCount; k++) {
					sum += dataPacket[j+k]*z[k];
				}
				output[j++] = sum;
				sum = 0;
			}
			else {
				output[j++] = dataPacket[i]*128 + dataPacket[i+1]*64 + dataPacket[i+2]*32 + dataPacket[i+3]*16 + dataPacket[i+4]*8 + dataPacket[i+5]*4 + dataPacket[i+6]*2 + dataPacket[i+7]*1;			
			}
	}
	return output;		
}

void *sendNAK(void *ser) {

	unsigned char *testData;
	int sockfd, nr;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	while(nakFlag == 0)  usleep(50);;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		error("ERROR opening socket");
	}

	server = (struct hostent *) ser;
	if (server == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(nakReceiver);
	bcopy((char *) server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	
	testData = (unsigned char*)malloc(packetCount*sizeof(char));
	memset(testData, 1, packetCount);


	//get nack header size
	int nackHeader = ((packetCount/8) % 1000) == 0 ? (packetCount/8) / 1000 : (packetCount/8) / 1000 + 1;
	int lastNackSize = ((packetCount/8) % 1000) == 0 ? 1000 : (packetCount/8) % 1000;
	int dataPacketByteCount = packetCount%8 == 0 ? packetCount/8 : packetCount/8 + 1;
	unsigned char* dataPacketByte = NULL;
	unsigned char* currentNack = (char*)calloc(1001,sizeof(char));
	
	while(1) {
		if(memcmp(dataPacket, testData, packetCount)==0) {
			break;
		}
		dataPacketByte = bit_byte(dataPacket);

		int i;
		for (i = 0; i < nackHeader; i++){	
			if(i == nackHeader-1) {
				currentNack[0] = (unsigned int)i;
				int j;
				for(j=0; j<lastNackSize; j++){
					currentNack[j+1] = dataPacketByte[i*1000+j];			
				}
				nr = sendto(sockfd, currentNack, lastNackSize+1, 0,(const struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));		
			}
			else {
				currentNack[0] = (unsigned int)i;
				int j;
				for(j=0; j<1000; j++){
					currentNack[j+1] = dataPacketByte[i*1000+j];	
				}	
				nr = sendto(sockfd, currentNack, 1001, 0,(const struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));
			}	
		}
		if (nr < 0) {
			printf("ERROR writing to socket");
		}
		usleep(2000);
	}
	close(sockfd);
}


int main(int argc, char *argv[]) {
	pthread_create(&getCount,0,getPacketCount,NULL);
	pthread_join(getCount, NULL);
	pthread_create(&receciveDataUDP, 0, receiveData, NULL);
	struct hostent *server =  gethostbyname(argv[1]);
	pthread_create(&nakDataUDP, 0, sendNAK, server);
	pthread_join(receciveDataUDP, NULL);
	pthread_join(nakDataUDP,NULL);
	gettimeofday(&tv1,0);
	elap = (tv1.tv_sec - tv0.tv_sec) + (tv1.tv_usec-tv0.tv_usec)/1000000;
	printf("elapse=%lu\n",elap);
	return 0;
}

