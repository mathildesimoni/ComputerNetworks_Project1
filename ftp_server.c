#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

int serve_client(int client_fd);
int check_input(char* input);
int create_data_socket(char* client_ip, int client_port);
int handle_STOR(int data_sd, char* message);
int handle_RETR(int data_sd, char* message);
int handle_LIST(int data_sd, char* message);

int main()
{
	//1. socket();
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	printf("server_fd = %d \n", server_fd);
	if (server_fd < 0) {
		perror("socket");
		return -1;
	}
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
		perror("setsock");
		return -1;
	}
	//2. bind ();
	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(21);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr("127.0.0.1"); does not work :/
	if (bind(server_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
		perror("bind");
		return -1;
	}

	//3. listen()
	if (listen(server_fd, 5) < 0) {
		perror("listen");
		return -1;
	}

	fd_set full_fdset,ready_fdset;
	FD_ZERO(&full_fdset);
	FD_SET(server_fd,&full_fdset);
	int max_fd = server_fd;

	//4. accept()
	while(1) {	
		// printf("max_fd=%d\n",max_fd);
		ready_fdset = full_fdset;
		if (select(max_fd+1, &ready_fdset, NULL, NULL, NULL) < 0) {
			perror("select");
			return -1;
		}

		for (int fd = 0; fd<=max_fd; fd++) {
			if (FD_ISSET(fd, &ready_fdset)) {
				if (fd == server_fd) {
					int new_fd = accept(server_fd, NULL, NULL);
					printf("client fd = %d \n", new_fd);
					FD_SET(new_fd, &full_fdset);
					if (new_fd > max_fd) max_fd = new_fd;    //Update the max_fd if new socket has higher FD
				}
				else if (serve_client(fd) == -1) { // if client disconnected
					FD_CLR(fd,&full_fdset);
				}
			}
		}
	}

	//6. close());
	close(server_fd);
}
//=================================
//  returns -1 if client wants to disconnect, 0 if invalid input/other problem, 1 of OK
int serve_client(int client_fd) {
	char message[256];	
	bzero(&message, sizeof(message));
	
	char buffer[256];	
	bzero(&buffer, sizeof(buffer));

	if (recv(client_fd, message, sizeof(message), 0) < 0) {
		perror("recv");
		return 0;
	}

	// check if the client disconnected or wants to disconnect
	if (strcmp(message, "bye") == 0 || strlen(message) == 0) {
		printf("Client disconnected \n");
		close(client_fd);
		return -1;
	}

	// check validity of input
	if (check_input(message) == 0){
		printf("Invalid input: please enter the command again\n");
		return 0;
	}

	printf("Message from client: %s \n", message);

	// check command
	if (strncmp(message, "PORT", 4) == 0){
		printf("PORT command received\n");

		// first, get client address and port
		int client_ip_arr[4];
		int client_port_arr[2];
		char client_ip[30];
		int client_port;
		sscanf(message, "PORT %d,%d,%d,%d,%d,%d", &client_ip_arr[0], &client_ip_arr[1], &client_ip_arr[2], &client_ip_arr[3], &client_port_arr[0], &client_port_arr[1]);
		sprintf(client_ip, "%d.%d.%d.%d", client_ip_arr[0], client_ip_arr[1], client_ip_arr[2],client_ip_arr[3]);
		client_port = client_port_arr[0] * 256 + client_port_arr[1];
		printf("client IP: %s\n", client_ip);
		printf("client PORT: %d\n", client_port);

		int data_sd = create_data_socket(client_ip, client_port);
		if (data_sd == -1) { return 0; }
	   	printf("Connected to client on new port \n");

	   	// server sends acknowledgement to client that it received the port
		bzero(&message, sizeof(message));
		strcpy(message, "200 PORT command successful");
		send(client_fd, message, strlen(message), 0);
		bzero(&message, sizeof(message));

		// start exchange of data
		// waits for RETR, LIST or STOR command
		if (recv(client_fd, message, sizeof(message), 0) < 0) {
			perror("recv");
			return 0;
		}

		int data_transfer;

		if (strncmp(message, "STOR", 4) == 0) {
			printf("STOR command received\n");
			data_transfer = handle_STOR(data_sd, message);
		}

		else if (strncmp(message, "RETR", 4) == 0) {
			printf("RETR command received\n");
			data_transfer = handle_RETR(data_sd, message);
		}

		else if (strncmp(message, "LIST", 4) == 0) {
			printf("LIST command received\n");
			data_transfer = handle_LIST(data_sd, message);
		}

		if (data_transfer == 0) { return 0; }





	    // send data to client
	 //    strcpy(buffer, "This is the first line of the file from server");
	 //    send(data_sd, buffer, strlen(buffer), 0);
	 //    bzero(&buffer,sizeof(buffer));

	 //    // receive data from client
	 //    recv(data_sd, buffer, sizeof(buffer), 0);
		// printf("Line of file received from client: %s \n", buffer);
		// bzero(&buffer,sizeof(buffer));

	    close(data_sd);
	}

	else if (strncmp(message, "USER", 4) == 0) {
		printf("USER command received\n");
	}

	else if (strncmp(message, "PASS", 4) == 0) {
		printf("PASS command received\n");
	}

	else if (strncmp(message, "STOR", 4) == 0) {
		printf("STOR command received\n");
	}

	else if (strncmp(message, "RETR", 4) == 0) {
		printf("RETR command received\n");
	}

	else if (strncmp(message, "LIST", 4) == 0) {
		printf("LIST command received\n");
	}

	else if (strncmp(message, "CWD", 3) == 0) {
		printf("CWD command received\n");
	}

	else if (strncmp(message, "PWD", 3) == 0) {
		printf("PWD command received\n");
	}

	else if (strncmp(message, "QUIT", 4) == 0) {
		printf("QUIT command received\n");
	}

	return 1;
}

// return 0 if invalid command or 1 if valid command
int check_input(char* input) {
	//  need to implement this function
	return 1;
}

int create_data_socket(char* client_ip, int client_port){
	// server creates a new socket to connect to new client port
	int data_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (data_sd < 0) {
		perror("socket:");
		return -1;
	}

	// add address of new client port (for data connection)
	struct sockaddr_in client_addr;
	bzero(&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(client_port);
	client_addr.sin_addr.s_addr = inet_addr(client_ip);

	// add address of new server port (port 20 for data connection)
	struct sockaddr_in new_server_addr;
	bzero(&new_server_addr, sizeof(new_server_addr));
	new_server_addr.sin_family = AF_INET;
	new_server_addr.sin_port = htons(20); // port 20 for data connection
	new_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (setsockopt(data_sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
		perror("setsock");
		return -1;
	}

	if (bind(data_sd, (struct sockaddr*)&new_server_addr, sizeof(new_server_addr)) < 0) {
		perror("bind failed");
		return -1;
	}

	//connect
    if (connect(data_sd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("connect");
        return -1;
    }
    return data_sd;
}

int handle_STOR(int data_sd, char* message) {
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	// receive data from client
	recv(data_sd, buffer, sizeof(buffer), 0);
	printf("Line of file received from client: %s \n", buffer);
	bzero(&buffer,sizeof(buffer));

	return 1;
}

int handle_RETR(int data_sd, char* message) {
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	// send data to client
    strcpy(buffer, "This is the first line of the file");
    send(data_sd, buffer, strlen(buffer), 0);
    bzero(&buffer,sizeof(buffer));

    return 1;
}

int handle_LIST(int data_sd, char* message) {
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	// send data to client
    strcpy(buffer, "file 1, file 2, file 3");
    send(data_sd, buffer, strlen(buffer), 0);
    bzero(&buffer,sizeof(buffer));

    return 1;
}







