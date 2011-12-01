#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include "checksum.c"
#include "packetstruct.h"

#define MAXBUFLEN 100
#define MAXACKPACK 1000
#define MAXSENTPACK 1000
volatile sig_atomic_t timed_out = 0;

void catch_alarm( int sig)
{
	timed_out = 1;
	signal(sig,catch_alarm);

}

void error(char *msg)
{
  perror(msg);
  exit(1);
}

int main( int argc, char *argv[] )	  
{
	int sockfd;
	FILE *fp;
	struct sockaddr_in send_addr;
	struct sockaddr their_addr;
	int numbytes;
	int portno;
	int seqNum;
	char buf[ MAXBUFLEN ];
	char packetBuf[ sizeof( struct packet ) ];
	socklen_t addr_len;
	int cwnd = atoi(argv[2]);
	int ackPack [MAXACKPACK];
	int timedPacket;
	int count;
	struct timeval tv;
	fd_set readfds;
	struct packet* ack;
	struct packet packetArray [MAXSENTPACK];
	struct packet p;
	int prob = 0;
	int noSeq = 0;
	int timeSig = 0;

	//initialize the packet buffer and the ack packet bitmap, this is currently static and should be either dynamic or set to a very high level.
	memset(packetArray,0, sizeof(packetArray));
	int i;
	for (i=0;i<MAXSENTPACK; i++)
	{
		packetArray[i].dPacket.type = -1;
		
	}
	memset(ackPack,0,sizeof(ackPack));
  // arguments:
  // portnumber, CWnd, Pl, PC
  if ( argc != 5 ) {
    fprintf( stderr,"Invalid Arguments\n" );
    exit( 1 );
  }

  // create a socket
  sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
  if ( sockfd < 0 )
    error( "ERROR opening socket" );
  bzero( (char*) &send_addr, sizeof( send_addr ) );
  portno = atoi( argv[ 1 ] );
  send_addr.sin_family = AF_INET; // IPv4
  send_addr.sin_addr.s_addr = INADDR_ANY; //bind to own IP
  send_addr.sin_port = htons( portno );

  if ( bind( sockfd, (struct sockaddr*) &send_addr,
	   sizeof(send_addr)) < 0) 
    error( "ERROR on binding" );

  printf( "sender waiting for file name...\n" );

  addr_len = sizeof their_addr;
  if ( ( numbytes = recvfrom( sockfd, buf, MAXBUFLEN, 0,
			      &their_addr, &addr_len ) ) == -1 ) {
    perror( "recvfrom" );
    exit( 1 );
  }

  // Open the file requested
  fp = fopen( buf, "r" );
  if ( fp  == NULL ) {
    // send back an error message if file was not found
    p.dPacket.type = -1;
    strcpy( p.dPacket.data, "File Not Found\n" );
  } else {
    // otherwise, let the client know that file will be sent
    p.dPacket.type = 3;
    strcpy( p.dPacket.data, "File Will Be Sent\n" );
  }
  if ( sendto( sockfd, (char*) &p, sizeof( p ), 0,
	       &their_addr, sizeof( their_addr ) ) == -1 ) {
    perror("sendto");
    exit(1);
  }
  if ( fp == NULL ) {
    close( sockfd );
    exit(1);
  }

  printf("Serving file: %s\n", buf);

	seqNum = 0;
	count = 0;
	timedPacket = 0;
	tv.tv_sec = 1;
	tv.tv_usec = 000000;
	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	ack = (struct packet*) packetBuf;
	ack->dPacket.seqNum = 0;

	while(ack->dPacket.type!=2 )
	{
	//this is the while loop that deals with the window
		  while ( count < cwnd ) {
		    // If we haven't sent the packet yet, build a packet for this seqNum
			    if(packetArray[seqNum].dPacket.type == -1)
				{
				    p.dPacket.dataLength = fread( p.dPacket.data, sizeof( p.dPacket.data[0] ), MAXDATALENGTH/sizeof( p.dPacket.data[0] ), fp );
				    p.dPacket.seqNum =  seqNum;
				    //if this is the last packet, build it, send it, and break.		
				    if ( feof( fp ) ) 
					{
						p.dPacket.type = 2;
						printf( "last seqnum is = %i\n", seqNum );
						count = cwnd;
						p.checksum = checksum( (byte*) &(p.dPacket), sizeof( p.dPacket ) );
						packetArray[seqNum] = p;
						if ( sendto( sockfd, (char*) &p, sizeof(p), 0, &their_addr, sizeof( their_addr ) ) == -1 ) 
						{
						      perror("sendto");
						      exit(1);
						}
						break;
					} 
					else 
						p.dPacket.type = 0;
				}
				//If the packet with seqNum requested has already been sent, use the buffer to send the correct packet. 
				else
				{
					//if the last packet sent was the EOF packet, break. We do not need to send another packet from buffer.
					if(p.dPacket.type == 2 )
						break;
					printf("we are pulling from the buffer\n");
					p = packetArray[seqNum];
				}

				p.checksum = checksum( (byte*) &(p.dPacket), sizeof( p.dPacket ) );
				// store the packet in the packetArray buffer
				packetArray[seqNum] = p;
				// send packet
				if(seqNum == 2 && prob == 0){
					noSeq = 1;
					prob = 1;}
				sleep(1);
				if(noSeq == 0){
				if ( sendto( sockfd, (char*) &p, sizeof(p), 0, &their_addr, sizeof( their_addr ) ) == -1 ) 
				{
					perror("sendto");
					exit(1);
				}
				printf("just sent packet %i\n",seqNum);
				}
				noSeq = 0;
				
				//this is the timer, it is currently set to 5 seconds. 'count' signals the beginning of a new window.
				signal(SIGALRM, catch_alarm);
				if(timeSig == 0){
					alarm (10);
					timeSig = 1;
				}
				count++;
				seqNum++;
			}



			// don't care about writefds and exceptfds, currently the times out after 1 second, check tv structure
			select(sockfd+1, &readfds, NULL, NULL, &tv);

			// this is for getting the acks
			if(FD_ISSET(sockfd,&readfds))
			{	
				 if ( ( numbytes = recvfrom( sockfd, packetBuf, sizeof( struct packet ), 0, &their_addr, &addr_len ) ) == -1 ) 
				{
				      perror( "recvfrom" );
				      exit( 1 );
				}
				else
				{
					count--;
					if (ackPack[ack->dPacket.seqNum] == 1 ) 
					{
						//reset the window
						count = 0;
						//reset the seqNum
						seqNum = ack->dPacket.seqNum+1;
						//resend the entire window because we received a duplicate ACK and thus a
						printf( "DUPLICATE ACK:\nseqNum = %i\ntype = %i\ndataLength = %i\n", ack->dPacket.seqNum, ack->dPacket.type, ack->dPacket.dataLength );
					}
					ackPack[ack->dPacket.seqNum] = 1;
					printf( "received:\nseqNum = %i\ntype = %i\ndataLength = %i\n", ack->dPacket.seqNum, ack->dPacket.type, ack->dPacket.dataLength );
				}
			}
			if (timed_out == 1 && ackPack[timedPacket] == 0)
			{
				timeSig = 0;
				seqNum = timedPacket;
				printf( "LOST ACK\n");
				timed_out = 0;
			}
			else if (timed_out == 1 && ackPack[timedPacket] == 1)
			{
				timeSig = 0;
				timedPacket = seqNum;
				timed_out= 0;			
				printf( "TIMEOUT, RECEIVED ACK timePacket is %i\n",timedPacket);
			}

	}
	fclose( fp );
	close( sockfd );

	return 0;
}
