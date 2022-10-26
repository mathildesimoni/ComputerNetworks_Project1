all: server client

server: ftp_server.o
	gcc ftp_server.o -o server -w

client: ftp_client.o
	gcc ftp_client.o -o client -w

ftp_server.o: ftp_server.c ftp_server.h
	gcc -c ftp_server.c -w

ftp_client.o: ftp_client.c ftp_client.h
	gcc -c ftp_client.c -w

clean:
	rm *.o server client
