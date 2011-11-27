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
  socklen_t addr_len;
  struct packet p;

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
  while ( !feof( fp ) ) {
    // build packet
    p.dPacket.dataLength = fread( p.dPacket.data, sizeof( p.dPacket.data[0] ),
				  MAXDATALENGTH/sizeof( p.dPacket.data[0] ), fp );
    p.dPacket.seqNum =  seqNum;
    if ( feof( fp ) ) {
      p.dPacket.type = 2;
      printf( "last seqnum is = %i", seqNum );
    } else {
      p.dPacket.type = 0;
    }

    p.checksum = checksum( (byte*) &(p.dPacket), sizeof( p.dPacket ) );

    // send packet
    if ( sendto( sockfd, (char*) &p, sizeof(p), 0, &their_addr, sizeof( their_addr ) ) == -1 ) {
      perror("sendto");
      exit(1);
    }
    
    seqNum++;
  }

  fclose( fp );
  close( sockfd );

  return 0;
}
