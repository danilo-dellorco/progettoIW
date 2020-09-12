#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>

void inputs_wait(char *s);
bool is_packet_lost(int prob);
char *time_stamp();
void set_timer(int micro);
void print_percentage(int part, int total, int oldPart);
void clearScreen();
int create_socket();
void set_timeout(int, int);