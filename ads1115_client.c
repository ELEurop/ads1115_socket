/* ads1115_client  v0.8
 * Client fuer ads1115_server
 *
 * Dank an
 * https://www.tobscore.com/socket-programmierung-in-c/
 * Gordon fuer i2c-Code von wiringPi
 *
 * @author: El Europ alias momefilo
 * Lizenz: GPL
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define PORT	"4711"

int connectto(char *server){
	struct addrinfo hints, *servinfo, *p;
	int sockfd, status;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if((status = getaddrinfo(server, PORT, &hints, &servinfo)) != 0){
		fprintf(stderr, "network-error: %s\n", gai_strerror(status));
		return -1;
	}

	for(p = servinfo; p != NULL; p=p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("client: socket");
			continue;
		}

		if(connect(sockfd,p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if(p == NULL){
		fprintf(stderr, "client: failed to connect\n");
		return -2;
	}
	freeaddrinfo(servinfo);
	return sockfd;
}

int sendall(int fd, u_int16_t *data, int len){
	int total = 0, bytesleft = len, n;
	while(total < len){
		n = send(fd, data+total, bytesleft, 0);
		if(n == -1) break;
		total += n;
		bytesleft -= n;
	}
	return n == -1 ? -1: 0;
}

int main(int argc, char *argv[]){

	u_int16_t sendbytes[21] = {0}, readbytes[20] = {0};
	int sockfd, datenanzahl, datencount;

	if(argc < 4){
		fprintf(stderr, "usage: %s server.ip [[i2c-adress [channelconfig ...] ...]\n", argv[0]);
		return -1;
	}

	datenanzahl = argc - 2;
	sendbytes[0] = htons(datenanzahl);
	for(datencount = 2; datencount < argc; datencount++){
		sendbytes[datencount - 1] = htons(atoi(argv[datencount]));
	}

	/* Verbindungsaufbau zum Server */
	if((sockfd = connectto(argv[1])) < 0){
		fprintf(stderr, "client: NOT-connected\n");
		return -2;
	}

	while(1){
		if(sendall(sockfd, sendbytes, sizeof(sendbytes)))perror("send");
		recv(sockfd, readbytes, sizeof(readbytes), 0);
		for(datencount = 0; datencount < datenanzahl; datencount++)
			printf("%f\n", ntohs(readbytes[datencount])*0.0001875);

		break;
	}
	return 0;
}
