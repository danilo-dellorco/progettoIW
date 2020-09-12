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

int create_socket() {
	
	struct sockaddr_in new_addr;
	int sockfd;
	//creazione socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("SERVER: socket creation error\n");
		exit(-1);
	}

	//configurazione socket
	memset((void *)&new_addr, 0, sizeof(new_addr));
	new_addr.sin_family = AF_INET;
	new_addr.sin_port = htons(0);
	new_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//assegnazione indirizzo al socket
	if (bind(sockfd, (struct sockaddr *)&new_addr, sizeof(new_addr)) < 0) {
		printf("SERVER: socket bind error\n");
		exit(-1);
	}
	return sockfd;
}

void set_timeout(int sockfd, int timeout) {
	// Imposta il timeout della socket in microsecondi
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = timeout;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time)) < 0) {
		perror("setsockopt error");
		exit(-1);
	}
}

// Utilizzata per il debug e l'analisi dei pacchetti inviati
void inputs_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}

void clearScreen(){
	printf("\033[2J\033[H");
}

// Genera un numero casuale e ritorna true o false in base alla probabilita di perdita passata in input
bool is_packet_lost(int prob){
  int random = rand() %100;
 // printf ("Random Number: %d\n",random);
  if (random<prob){
	  return true;
  }
  return false;
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

// Imposta il timer di ritrasmissione
void set_timer(int micro){
    struct itimerval it_val;
	if (micro >= MAX_RTO){
		micro = MAX_RTO;
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

// Stampa una barra di avanzamento relativo all'invio del file
void print_percentage(int part, int total, int oldPart){
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
	printf (" %.2f%% complete\n",percentage);
}

