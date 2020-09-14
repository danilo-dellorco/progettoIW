#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include "utility.h"
#include "comm.h"

// Genera un numero casuale e ritorna true o false in base alla probabilita di perdita passata in input
bool is_packet_lost(int prob){
  int random = rand() %100;
  if (random<prob){
	  return true;
  }
  return false;
}

// Crea una nuova socket ed effettua il binding
int create_socket() {
	struct sockaddr_in new_addr;
	int sockfd;

	// Creazione socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("SERVER: socket creation error\n");
		exit(-1);
	}

	// Configurazione socket
	memset((void *)&new_addr, 0, sizeof(new_addr));
	new_addr.sin_family = AF_INET;
	new_addr.sin_port = htons(0);
	new_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Assegnazione indirizzo al socket
	if (bind(sockfd, (struct sockaddr *)&new_addr, sizeof(new_addr)) < 0) {
		printf("SERVER: socket bind error\n");
		exit(-1);
	}
	return sockfd;
}

// Imposta il timeout della socket in microsecondi
void set_socket_timeout(int sockfd, int timeout) {
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = timeout;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time)) < 0) {
		perror("setsockopt error");
		exit(-1);
	}
}

// Imposta il timer di ritrasmissione
void set_retransmission_timer(int micro){
    struct itimerval it_val;

	if (ADAPTIVE_RTO == 0){
		micro = STATIC_RTO;
	}
	else if (micro >= MAX_RTO){
		micro = MAX_RTO;
	}
	else if (micro <= MIN_RTO && micro != 0){
		micro = MIN_RTO;
	}

	it_val.it_value.tv_sec = 0;
	it_val.it_value.tv_usec = micro;
	it_val.it_interval.tv_sec = 0;
	it_val.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		perror("Set Timer Error:");
		exit(1);
	}
}

// Stampa il timestamp con precisione ai microsecondi
char *time_stamp(){
	struct timeval tv;
	struct tm* ptm;
	char time_string[40];
 	long microseconds;
	char *timestamp = (char *)malloc(sizeof(char) * 16);

	gettimeofday(&tv,0);
	ptm = localtime (&tv.tv_sec);
	strftime (time_string, sizeof (time_string), "%H:%M:%S", ptm);
	microseconds = tv.tv_usec;
	sprintf (timestamp,"[%s.%03ld]", time_string, microseconds);
	return timestamp;
}

// Stampa una barra di avanzamento relativa all'invio/ricezione del file
void print_percentage(int part, int total, int oldPart, char* subject){
	if (strcmp(subject,CLIENT) != 0){
		return;
	}

	float percentage = (float) part/total*100;
	float oldPercentage = (float) oldPart/total*100;

	if ((int) oldPercentage == (int) percentage){
		return;
	}

	printf("|");
	for (int i = 0; i<=percentage/2; i++){
		printf("█");
	}
	for (int j = percentage/2; j<50; j++){
		printf("-");
	}
	printf ("|");
	printf (" %.2f%%\n",percentage);
}

// Pulisce la schermata del terminale
void clearScreen(){
	printf("\033[2J\033[H");
}