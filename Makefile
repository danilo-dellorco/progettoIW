CC = gcc
LIB?= ./lib/
FILEC = $(LIB)utility.c $(LIB)tcp_receiver.c $(LIB)tcp_sender.c client.c -lm -lpthread -lrt
FILES = $(LIB)utility.c $(LIB)tcp_receiver.c $(LIB)tcp_sender.c server.c -lm -lpthread -lrt

do:
	$(CC) $(FILEC) -o client.out
	$(CC) $(FILES) -o server.out

	@echo " "
	@echo "Compilato"
clean:
	rm client
	rm server

	@echo " "
	@echo "File eliminati!"
