#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include<unistd.h>
#include<stdlib.h>

// function declarations
int serve_user(int server_sd, char* input, char* my_ip, unsigned short int my_port, int* request_number);
int check_input(char* input);
int create_data_socket(int new_port, char* my_ip);
int establish_data_connection(int server_sd, int* my_ip_arr, int new_port, int data_client_sd);
int upload_file(int data_server_sd, char* file_name);
int download_file(int data_server_sd, char* file_name);
int list_files(int data_server_sd);
int display_user_commands();

// function definitions
int main() {
	//socket
	int server_sd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sd < 0) {
		perror("socket:");
		exit(-1);
	}

	//setsock
	int value  = 1;
	setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)
	struct sockaddr_in server_addr, my_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(21);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //INADDR_ANY, INADDR_LOOP

	//connect
    if (connect(server_sd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(-1);
    }

    // get client's own IP address and port randomly assigned with the connect() function
    bzero(&my_addr, sizeof(my_addr));
    socklen_t addr_len = sizeof(my_addr);
    getsockname(server_sd, (struct sockaddr *) &my_addr, &addr_len);
    char my_ip[INET_ADDRSTRLEN];
    unsigned short my_port = ntohs(my_addr.sin_port);
    inet_ntop(AF_INET, &(my_addr.sin_addr), my_ip, INET_ADDRSTRLEN);

    printf("Client connected to server at %s:%hu \n", my_ip, my_port);

	char buffer[256]; // will store user input
	bzero(buffer, sizeof(buffer));
	int request_number = 1;
	int response; // will store server responses
	int display_commands = 1;

	while(1) {

		if (display_commands == 1) {
			display_commands = display_user_commands();
		}
		else {
			printf("\nTo display available commands, enter \"commands\" \n");
			printf("Please enter a command: ");
		}		
		
		// get input from user
		fgets(buffer, sizeof(buffer), stdin);
	    buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it

        // check if the client wants to print the commands
	    if (strcmp(buffer, "commands")==0) {
			display_commands = 1;
        }

        // check input
        else if (check_input(buffer) == 0) {
        	printf("Invalid input: please enter the command again\n");
        }
        else { // input is valid, proceed with the request
        	response = serve_user(server_sd, buffer, my_ip, my_port, &request_number);
        	if (response == 0) {
        		printf("Error: could not send command to server \n");
        	}
        	else if (response == -1) {
        		printf("Closing the connection to server \n");
	        	close(server_sd);
	            break;
        	}	
        }
        bzero(buffer, sizeof(buffer));			
	}
	return 0;
}

// returns 0 if error
int serve_user(int server_sd, char* input, char* my_ip, unsigned short int my_port, int* request_number) {

	int my_ip_arr[4];
	char message[256];
	bzero(message, sizeof(message));

	printf("\n");

	// for USER, PASS, CWD, PWD, not preprocessing needed, directly send to server
	if ((strncmp(input, "USER", 4) == 0) || (strncmp(input, "PASS", 4) == 0) || (strncmp(input, "CWD", 3) == 0) || (strncmp(input, "PWD", 3) == 0)) {
		printf("USER, PASS, PWD or PWD command typed \n");

		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}

		// wait for server to send response message
		recv(server_sd, message, sizeof(message), 0);
		printf("Response from server: %s \n", message);  

	}

	// for the 3 next if conditions, no server needed, commands implemented locally
	else if (strncmp(input, "!LIST", 5) == 0) {
		printf("!LIST command typed \n");
	}

	else if (strncmp(input, "!CWD", 4) == 0) {
		printf("!CWD command typed \n");
	}

	else if (strncmp(input, "!PWD", 4) == 0) {
		printf("!PWD command typed \n");
	}

	// send to server
	else if (strncmp(input, "QUIT", 4) == 0) {
		printf("QUIT command typed \n");
		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}

		// wait for server to send response message
		recv(server_sd, message, sizeof(message), 0);
		printf("Response from server: %s \n", message);  

		if (strncmp(message, "221", 3) != 0) {
			printf("Error: could not login \n");
			return -1;
		}
	}

	else if ((strncmp(input, "STOR", 4) == 0) || (strncmp(input, "RETR", 4) == 0) || (strncmp(input, "LIST", 4) == 0)) {
		printf("STOR, RETR or LIST command typed: sending PORT command first\n");

		// establish data connection with a PORT command
		int new_port = my_port + *request_number;
		sscanf(my_ip, "%d.%d.%d.%d", &my_ip_arr[0], &my_ip_arr[1], &my_ip_arr[2], &my_ip_arr[3]);
		
		int data_client_sd = create_data_socket(new_port, my_ip);
		if (data_client_sd == -1) { return 0; }

		int data_server_sd = establish_data_connection(server_sd, my_ip_arr, new_port, data_client_sd);
		if (data_server_sd == -1) { 
			close(data_client_sd);
			return 0; 
		}

		int data_transfer;
		// start exchange of data
		if (strncmp(input, "LIST", 4) == 0) {
			send(server_sd, input, strlen(input), 0);
			data_transfer = list_files(data_server_sd);
		}
		else {
			char file_name[256] = "test.txt";
			if (strncmp(input, "STOR", 4) == 0) {
				send(server_sd, input, strlen(input), 0);
				data_transfer = upload_file(data_server_sd, file_name);
			}
			else { // RETR command
				send(server_sd, input, strlen(input), 0);
				data_transfer = download_file(data_server_sd, file_name);
			}
		}
		if (data_transfer == 0) {
			close(data_client_sd);
			return 0; 
		}

		// close the data connection
		close(data_client_sd);
		*request_number += 1;
	}

	// to remove later, for now it is for debugging purposes
	else {
		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}
	}

	return 1;
}


// return 0 if invalid command or 1 if valid command
int check_input(char* input) {
	//  need to implement this function
	if (strncmp(input, "PORT", 4) == 0) {
		printf("You cannot explicitely send a PORT command \n");
		return 0;
	}
	return 1;
}


int create_data_socket(int new_port, char* my_ip) {

	int data_client_sd = socket(AF_INET,SOCK_STREAM,0);
	if (data_client_sd < 0) {
		perror("socket:");
		return 0;
	}
	if (setsockopt(data_client_sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
		perror("setsock: ");
		return 0;
	}

	//2. bind ();
	struct sockaddr_in client_address;
	bzero(&client_address, sizeof(client_address));
	client_address.sin_family = AF_INET;
	client_address.sin_port = htons(new_port);
	client_address.sin_addr.s_addr = inet_addr(my_ip); // htonl(INADDR_ANY); //
	if (bind(data_client_sd, (struct sockaddr*)&client_address, sizeof(client_address)) < 0) {
		perror("bind: ");
		return 0;
	}

	//3. listen()
	if (listen(data_client_sd, 5) < 0) {
		perror("listen");
		return 0;
	}

	printf("Client is listening...\n");
	return data_client_sd;
}


int establish_data_connection(int server_sd, int* my_ip_arr, int new_port, int data_client_sd) {

	char message[256];
	bzero(message, sizeof(message));

	// send PORT command to server
	sprintf(message, "PORT %d,%d,%d,%d,%d,%d", my_ip_arr[0], my_ip_arr[1], my_ip_arr[2], my_ip_arr[3], new_port/256, new_port%256);	
	if (send(server_sd, message, strlen(message),0) < 0) {
	    perror("send");
	    return -1;
	}
	printf("SENT PORT COMMAND \n");
	bzero(message, sizeof(message));

	// will store the address server is sending data from
	// this is for testing purpose, to make sure the server sends from port 20 (and not 21)
	struct sockaddr_in server_data_addr;
	socklen_t server_data_addr_len;
	server_data_addr_len = sizeof(server_data_addr);
	char server_data_IP[INET_ADDRSTRLEN];

	//accept 
	int data_server_sd = accept(data_client_sd, (struct sockaddr *)&server_data_addr, &server_data_addr_len); // blocking
	//stores server new IP address as a string and prints it
	inet_ntop(AF_INET, &(server_data_addr.sin_addr), server_data_IP, INET_ADDRSTRLEN);
	printf("Server connected on IP %s and port %hu \n", server_data_IP, ntohs(server_data_addr.sin_port));

	// wait for server to send 200 OK response
	recv(server_sd, message, sizeof(message), 0);
	printf("Response from server: %s \n", message);  

	if (strncmp(message, "200", 3) != 0) {
		printf("Error: could not establish a data connection \n");
		return -1;
	}
	bzero(message, sizeof(message));

	return data_server_sd;
}

int upload_file(int data_server_sd, char* file_name) {
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	strcpy(buffer, "This is the first line of the file from client");
	send(data_server_sd, buffer, strlen(buffer), 0);
	bzero(buffer, sizeof(buffer));

	return 1;
}

int download_file(int data_server_sd, char* file_name){
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));
	
	recv(data_server_sd, buffer, sizeof(buffer), 0);
	printf("Line of file received from server: %s \n", buffer);

	return 1;
}

int list_files(int data_server_sd) {
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	recv(data_server_sd, buffer, sizeof(buffer), 0);
	printf("Files in the directory: %s \n", buffer);

	return 1;
}

int display_user_commands() {
	printf("\nAvailable commands:\n");
	printf("- USER username: to start authentification \n- PASS password: to finish authentification (after USER command) \n- STOR filename: upload a local file from current client directory to current server directory \n- RETR filename: download a file from current server directory to current client directory \n- LIST: list all the files under current server directory \n- !LIST: list all the files under current client directory \n- CWD foldername: change current server directory \n- !CWD foldername: change current client directory \n- PWD: display current server directory \n- !PWD: display current client directory \n- QUIT: quit the FTP session and closes the control TCP connection \n");
	printf("Please enter a command: ");
	return 0;
}


// draft
// TO KEEP just in case 

// create socket on new port
// int new_port = my_port + *request_number;
// sscanf(my_ip, "%d.%d.%d.%d", &my_ip_arr[0], &my_ip_arr[1], &my_ip_arr[2], &my_ip_arr[3]);
// int data_client_sd = socket(AF_INET,SOCK_STREAM,0);
// if (data_client_sd < 0) {
// 	perror("socket:");
// 	return 0;
// }
// if (setsockopt(data_client_sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
// 	perror("setsock: ");
// 	return 0;
// }

// //2. bind ();
// struct sockaddr_in client_address;
// bzero(&client_address, sizeof(client_address));
// client_address.sin_family = AF_INET;
// client_address.sin_port = htons(new_port);
// client_address.sin_addr.s_addr = inet_addr(my_ip); // htonl(INADDR_ANY); //
// if (bind(data_client_sd, (struct sockaddr*)&client_address, sizeof(client_address)) < 0) {
// 	perror("bind: ");
// 	return 0;
// }

// //3. listen()
// if (listen(data_client_sd, 5) < 0) {
// 	perror("listen");
// 	return 0;
// }

// printf("Client is listening...\n");

// // send PORT command to server
// sprintf(message, "PORT %d,%d,%d,%d,%d,%d", my_ip_arr[0], my_ip_arr[1], my_ip_arr[2], my_ip_arr[3], new_port/256, new_port%256);	
// if (send(server_sd, message, strlen(message),0) < 0) {
//     perror("send");
//     return 0;
// }
// bzero(message, sizeof(message));

// // wait for server to send 200 OK response
// recv(server_sd, message, sizeof(message), 0);
// printf("Response from server: %s \n", message);  

// if (strncmp(message, "200", 3) != 0) {
// 	printf("Error: could not establish a data connection \n");
// 	return 0;
// }
// bzero(message, sizeof(message));

// // will store the address server is sending data from
// // this is for testing purpose, to make sure the server sends from port 20 (and not 21)
// struct sockaddr_in server_data_addr;
// socklen_t server_data_addr_len;
// server_data_addr_len = sizeof(server_data_addr);
// char server_data_IP[INET_ADDRSTRLEN];

// //accept 
// int data_server_sd = accept(data_client_sd, (struct sockaddr *)&server_data_addr, &server_data_addr_len); // blocking
// //stores server new IP address as a string and prints it
// inet_ntop(AF_INET, &(server_data_addr.sin_addr), server_data_IP, INET_ADDRSTRLEN);
// printf("Server connected on IP %s and port %hu \n", server_data_IP, ntohs(server_data_addr.sin_port));






