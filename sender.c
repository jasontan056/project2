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
#include "checksum.c"
#include "packetstruct.h"

#define MAXBUFLEN 100
#define MAXACKPACK 1000
#define MAXSENTPACK 1000

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
 struct packet packetArray [MAXSENTPACK];
  struct packet p;
int i;
	for (i=0;i<MAXSENTPACK; i++)
{
	packetArray[i].dPacket.type = -1;

}
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
  // sends file in packets
while( !feof( fp ))
{
   int count = 0;
//this is the window count for packets sent
  while ( count < cwnd ) {
    // build packet
    if(packetArray[seqNum].dPacket.type == -1)
	{
	    p.dPacket.dataLength = fread( p.dPacket.data, sizeof( p.dPacket.data[0] ),
					  MAXDATALENGTH/sizeof( p.dPacket.data[0] ), fp );
	    p.dPacket.seqNum =  seqNum;
	    if ( feof( fp ) ) {
	      p.dPacket.type = 2;
	      printf( "last seqnum is = %i\n", seqNum );
		break;
	    } else {
	      p.dPacket.type = 0;
	    }
	}
	else
	{
		//pull info from the buffer in the event that seqNum is some already sent packet.
		printf("we are pulling from the buffer\n");
		p = packetArray[seqNum];
	}

    p.checksum = checksum( (byte*) &(p.dPacket), sizeof( p.dPacket ) );
// this is the packet buffer.
    packetArray[seqNum] = p;
    // send packet
	sleep(5);
    if ( sendto( sockfd, (char*) &p, sizeof(p), 0, &their_addr, sizeof( their_addr ) ) == -1 ) {
      perror("sendto");
      exit(1);
    }
    count++;
    seqNum++;
	// this is for getting the acks, not sure if Select is needed/how select is used properly, subtract the count as you receive ACKS.
	 if ( ( numbytes = recvfrom( sockfd, packetBuf, sizeof( struct packet ), 0,
					&their_addr, &addr_len ) ) == -1 ) {
	      perror( "recvfrom" );
	      exit( 1 );
	    }
	else
	{
		count--;
		struct packet* ack;
		ack = (struct packet*) packetBuf;
		//check for duplicate ACK
		if (ackPack[ack->dPacket.seqNum] == 1 ) 
		{
			count = 0; //reset the window
			seqNum = ack->dPacket.seqNum; //reset the seqNum
			//resend the entire window because we received a duplicate ACK and thus a
		printf( "DUPLICATE ACK:\nseqNum = %i\ntype = %i\ndataLength = %i\n", ack->dPacket.seqNum, ack->dPacket.type, ack->dPacket.dataLength );
		}
		else
		{
   			ackPack[seqNum] = 1;
			printf( "received:\nseqNum = %i\ntype = %i\ndataLength = %i\n", ack->dPacket.seqNum, ack->dPacket.type, ack->dPacket.dataLength );
		}
	}
  }

}
 if ( sendto( sockfd, (char*) &p, sizeof(p), 0, &their_addr, sizeof( their_addr ) ) == -1 ) {
      perror("sendto");
      exit(1);
    }
  fclose( fp );
  close( sockfd );

  return 0;
}
