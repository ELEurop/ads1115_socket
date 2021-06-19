/* ads1115_server  v1.0
 * Liest daten des ads1115 ADC und sendet sie an den Client
 *
 * Dank an
 * https://www.tobscore.com/socket-programmierung-in-c/
 * Gordon fuer i2c-Code von wiringPi
 *
 * @author: El Europ alias momefilo
 * Lizenz: GPL
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <momefilo_tools.h>

#define PORT	"4711"
#define I2CDEV	"/dev/i2c-%d"
#define BUSNBR	1

int i2cdevicefile;

int i2csetdevicefile(void){
	char	mdevice[20];
	/* String fuer pfad zusammensetzten */
	snprintf(mdevice, 19, I2CDEV, BUSNBR);
	/* i2c-Geraetedatei oeffnen */
	if((i2cdevicefile = open(mdevice, O_RDWR)) < 0){
		printf("Fehler: Keine Berichtung -> %s\n", strerror(errno));
		return -1;
	}
	return 0;
}
/* Adresse des i2c-Busteilnehmers setzten */
int i2csetaddr(int address){
	if(ioctl(i2cdevicefile, I2C_SLAVE, address) < 0){
		fprintf(stderr,"Fehler: i2c-adresse -> %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/* Fuer i2cread und i2cwrite */
int i2c_smbus_access(char rw,u_int8_t command,int size,union i2c_smbus_data *data){

	struct i2c_smbus_ioctl_data args ;
	args.read_write = rw ;
	args.command    = command ;
	args.size       = size ;
	args.data       = data ;
	return ioctl (i2cdevicefile, I2C_SMBUS, &args) ;
}

int i2cwrite (int reg, int value){

	union i2c_smbus_data data;
	data.word = value;
	return i2c_smbus_access (I2C_SMBUS_WRITE, reg, I2C_SMBUS_WORD_DATA, &data);
}

int i2cread(int reg){

	union i2c_smbus_data data;
	if (i2c_smbus_access (I2C_SMBUS_READ, reg, I2C_SMBUS_WORD_DATA, &data))
		return -1 ;
	else
	return data.word & 0xFFFF ;
}

int bindserver(void){
	struct addrinfo hints, *ai, *p;
	int listener, status, yes=1;

	/* IP-Adresse und Port des Serverhosts auslesen */
	memset(&hints, 0, sizeof hints);
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= AI_PASSIVE;
	if((status = getaddrinfo(NULL, PORT, &hints, &ai)) != 0){
		fprintf(stderr,"getaddrinfo error: %s\n", gai_strerror(status));
		return -1;
	}
	/* Server an IP-Adresse und Port binden */
	for(p = ai; p != NULL; p = p->ai_next){

		/* Dateideskriptor erstellen */
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(listener < 0) continue;

		/* Port sofort für Server freigeben */
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		/* Server an Port binden */
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0){
			close(listener);
			continue;
		}
		break;
	}
	if(p == NULL){
		fprintf(stderr, "kann server nicht an Port binden\n");
		return -2;
	}
	/* Adressinfo Struckturspeicher wieder freigeben */
	freeaddrinfo(ai);
	/* Am Port lauschen mit bis zu zehn Clientanforderungen in der Wareteschlange */
	if(listen(listener, 10) < 0){
		perror("listen");
		return -3;
	}
	return listener;
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

	/* select für Multiplex lesen und schreiben */
	fd_set master;
	fd_set read_fds;
	int fdmax;

	/* Dateideskriptoren, Struckturen, und Hilfsvariablen */
	int listener, newfd;
	socklen_t addrlen;
	struct sockaddr_storage remoteaddr;
	int i, nbytes;

	/* Empfangs- und Sendebytes */
	u_int16_t readbytes[21], sendbytes[20];
	int datencount;

	/* Pools für select leeren */
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	/* i2c-Device oeffnen */
	if(i2csetdevicefile()) fprintf(stderr, "i2c-device error");

	/* server an Port binden und lauschen*/
	if((listener = bindserver()) < 0) fprintf(stderr, "network-error");

	/* lauschender Dateideskriptor in Pool für select zufügen */
	FD_SET(listener, &master);

	/* Den hoechsten Deskriptor für die select-schlefe bestimmen */
	fdmax = listener;

	while(1){
		/* Deskriptorenpool kopieren und an select uebergeben */
		read_fds = master;
		if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			return -5;
		}

		/* select-schleife alle Deskriptoren ueberpruefen */
		for(i = 0; i <= fdmax; i++){

			/* Gibt es eine Client/lese/schreib- Anfrage */
			if(FD_ISSET(i, &read_fds)){

				/* ist die Anfrage am lauschenden Port ?
				 * dann neuen Deskriptor zum Masterpool hinzufuegen */
				if(i == listener){

					/* Neuen Deskriptor erstellen */
					addrlen	= sizeof(remoteaddr);
					newfd	= accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);
					if(newfd == -1) perror("accept");

					/* und zum Masterpool zufuegen */
					else{
						FD_SET(newfd, &master);

						/* Hoechste deskriptorennr. bestimmen */
						if(newfd > fdmax) fdmax = newfd;
					}

				/* oder Daten  von Client lesen und verarbeiten */
				}else{
					if((nbytes = recv(i,readbytes,sizeof(readbytes), 0)) <= 0){

						/* Client hat die Verbindung beendet */
						if(nbytes == 0){}

						/* Die Verbindung hat einen Fehler */
						if(nbytes < 0) perror("receive");

						/* Deskriptor schließen und
						 * aus Pool entfernen */
						close(i);
						FD_CLR(i, &master);
						break;

					/* Daten konnten gelesen werden */
					}else{
						/* Daten aus i2c-device lesen
						 * und in sendbytes speichern*/
						for(datencount = 1; datencount < ntohs(readbytes[0]) + 1; datencount++){

							/* Es ist eine i2c-Adresse
							* der nachfolgenden kanaele */
							if(ntohs(readbytes[datencount]) < 0x0100){
								i2csetaddr(ntohs(readbytes[datencount]));
								sendbytes[datencount - 1] = readbytes[datencount];

							/* Es ist eine kanalkonfiguration */
							}else{
								/* Config an i2c-Adresse schreiben */
								i2cwrite(1, __bswap_16(ntohs(readbytes[datencount])));
								mmillisleep(5);

								/* Daten aus i2c-Device lesen nd in sendbytes schreiben */
								sendbytes[datencount - 1] = htons(__bswap_16(i2cread(0)));
							}
						}
						/* sendbytes an Netzwerk senden */
						if(sendall(i, sendbytes, sizeof(sendbytes)))perror("send");
					} // Ende lesen
				} // Ende Nutzerdaten
			} // Ende Anfrage
		} // Ende select-schleife
	} // Ende Endlosloop
	return 0;
}
