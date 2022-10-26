#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>

// function declarations
int serve_user(int server_sd, char* input, char* my_ip, unsigned short int my_port, int* request_number, char* cur_dir_client, int* logged_in, char* cur_dir_server);
int check_input(char* input);
int create_data_socket(int new_port, char* my_ip);
int establish_data_connection(int server_sd, int* my_ip_arr, int new_port, int data_client_sd);
int upload_file(int data_server_sd, char* file_name, char* cur_dir_client, char* cur_dir_server);
int download_file(int data_server_sd, char* file_name, char* cur_dir_client, char* cur_dir_server);
int list_files(int data_server_sd);
int display_user_commands();
int change_directory(char* cur_dir_client, char* new_dir);
int list_directory(char* cur_dir_client);

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
	char cur_dir_client[256];
	strcpy(cur_dir_client, "no_dir");

	char cur_dir_server[256];
	strcpy(cur_dir_server, "no_dir");

	int logged_in = 0;

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
        	response = serve_user(server_sd, buffer, my_ip, my_port, &request_number, cur_dir_client, &logged_in, cur_dir_server);
        	// if (response == 0) {
        	// 	printf("Error: could not send command to server \n");
        	// }
        	if (response == -1) {
        		printf("Closing the connection to server \n");
	        	close(server_sd);
	            break;
        	}	
        }
        bzero(buffer, sizeof(buffer));		
        printf("Current client directory: %s\n", cur_dir_client);
        printf("Current server directory: %s\n", cur_dir_server);
        printf("User logged in: %d \n", logged_in);
	}
	return 0;
}

// returns 0 if error
int serve_user(int server_sd, char* input, char* my_ip, unsigned short int my_port, int* request_number, char* cur_dir_client, int* logged_in, char* cur_dir_server) {

	int my_ip_arr[4];
	char message[256];
	bzero(message, sizeof(message));

	printf("\n");

	// for USER, PASS, CWD, PWD, not preprocessing needed, directly send to server
	if ((strncmp(input, "USER", 4) == 0) || (strncmp(input, "PASS", 4) == 0)) {
		printf("USER or PASS command typed \n");

		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}

		// wait for server to send response message
		recv(server_sd, message, sizeof(message), 0);
		printf("Response from server: %s \n", message);

		if ((strncmp("USER", input, 4) == 0) && (strncmp("331", message, 3) == 0)){
			char username[256];
			char tmp_dir_client[256];
			sscanf(input, "USER %s", &username);
			printf("Username: %s \n", username);
			sprintf(tmp_dir_client, "client_directories/%s/", username);
			strcpy(cur_dir_client, tmp_dir_client);

			char tmp_dir_server[256];
			sprintf(tmp_dir_server, "server_directories/%s/", username);
			strcpy(cur_dir_server, tmp_dir_server);
		}

		if ((strncmp("PASS", input, 4) == 0) && (strncmp("230", message, 3) == 0)){
			*logged_in = 1;
		}
	}

	else if (strncmp(input, "CWD", 3) == 0){
		printf("CWD command typed \n");

		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}

		// waits the server is ready to receive current server directory
		bzero(message, sizeof(message));
		recv(server_sd, message, sizeof(message), 0);

		// send current server directory to server (needed to check correct directory)
		if (send(server_sd, cur_dir_server, strlen(cur_dir_server),0) < 0) {
		    perror("send");
		    return 0;
		}

		char new_dir[256];
		sscanf(input, "CWD %s", &new_dir);

		// wait for answer from server
		bzero(message, sizeof(message));
		recv(server_sd, message, sizeof(message), 0);
		if (strncmp(message, "200", 3) == 0) {
			printf("Current server directory updated \n");
			char* path[256];
			sprintf(path, "%s%s/", cur_dir_server, new_dir);
			strcpy(cur_dir_server, path);
		}
		else {
			printf("Error: could not change current server directory \n");
		}
	}

	else if (strncmp(input, "PWD", 3) == 0) {
		printf("PWD command typed \n");
		
		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}

		bzero(message, sizeof(message));
		recv(server_sd, message, sizeof(message), 0);
		// send current server directory to server (needed to check correct directory)
		if (send(server_sd, cur_dir_server, strlen(cur_dir_server),0) < 0) {
		    perror("send");
		    return 0;
		}

		bzero(message, sizeof(message));
		recv(server_sd, message, sizeof(message), 0);
		printf("Response from server: %s\n", message);
	}


	// for the 3 next if conditions, no server needed, commands implemented locally
	else if (strncmp(input, "!LIST", 5) == 0) {
		printf("!LIST command typed \n");
		list_directory(cur_dir_client);
	}

	else if (strncmp(input, "!CWD", 4) == 0) {
		printf("!CWD command typed \n");
		char new_dir[256];
		sscanf(input, "!CWD %s", &new_dir);

		int result = change_directory(cur_dir_client, new_dir);
		if (result == -1) {
			printf("Error: could not change current client directory \n");
		}
	}

	else if (strncmp(input, "!PWD", 4) == 0) {
		printf("!PWD command typed \n");
		printf("Current client directory: %s\n", cur_dir_client);
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
			printf("Error: could not quit connection \n");
			return 0;
		}
		return -1;
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
			bzero(message, sizeof(message));
			sprintf(message, "LIST %s", cur_dir_server);
			printf("%s\n", message);
			send(server_sd, message, strlen(message), 0);
			data_transfer = list_files(data_server_sd);
		}
		else {
			char file_name[256];
			if (strncmp(input, "STOR", 4) == 0) {
				sscanf(input, "STOR %s", &file_name);
				printf("file name: %s \n", file_name);

				bzero(message, sizeof(message));
				sprintf(message, "STOR %s%s", cur_dir_server, file_name);
				
				send(server_sd, message, strlen(message), 0);
				data_transfer = upload_file(data_server_sd, file_name, cur_dir_client, cur_dir_server);
			}
			else { // RETR command
				sscanf(input, "RETR %s", &file_name);
				printf("file name: %s \n", file_name);

				bzero(message, sizeof(message));
				sprintf(message, "RETR %s%s", cur_dir_server, file_name);
				
				send(server_sd, message, strlen(message), 0);
				data_transfer = download_file(data_server_sd, file_name, cur_dir_client, cur_dir_server);
			}
		}
		bzero(message, sizeof(message));
		recv(server_sd, message, sizeof(message), 0);
		printf("Response from server: %s \n", message); 

		if (data_transfer == 0) {
			close(data_client_sd);
			*request_number += 1;
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

int upload_file(int data_server_sd, char* file_name, char* cur_dir_client, char* cur_dir_server) {
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	char client_path[256];
	bzero(client_path, sizeof(client_path));
	sprintf(client_path, "%s%s", cur_dir_client, file_name);
	printf("client path: %s \n", client_path);

	// if file exists, starts transfer
	FILE *fp;

    if (!(fp = fopen (client_path, "r"))) {    /* open/validate file open */
        perror ("fopen-file");
        strcpy(buffer, "no file");
		send(data_server_sd, buffer, strlen(buffer), 0);
		bzero(&buffer,sizeof(buffer));
        return 0; // failure
    }
    while(fgets(buffer, 256, fp)) {
		printf("%s", buffer);
		send(data_server_sd, buffer, strlen(buffer), 0);
	    bzero(&buffer,sizeof(buffer));
	}
    fclose(fp);
    return 1; // success
}

int download_file(int data_server_sd, char* file_name, char* cur_dir_client, char* cur_dir_server){
	
	char client_path[256];
	bzero(client_path, sizeof(client_path));
	sprintf(client_path, "%s%s", cur_dir_client, file_name);
	printf("client path: %s \n", client_path);

	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	recv(data_server_sd, buffer, sizeof(buffer), 0);

	if (strncmp(buffer, "no file", 7) == 0){
		return 0;
	}
	else {
		FILE *fp;

	    if (!(fp = fopen (client_path, "w"))) {    /* open/validate file open */
	        perror ("fopen-file");
	        return 0;
	    }

	    // write first line already received in buffer to file
	    fprintf(fp, "%s", buffer);
	    // fprintf(fp, "%s \n", buffer);

	    while (1) {
	    	bzero(buffer, sizeof(buffer));
	    	recv(data_server_sd, buffer, sizeof(buffer), 0);
	    	printf("%s\n", buffer);
	    	if (strlen(buffer) > 0) {
	    		printf("End of file now \n");
	    		fprintf(fp, "%s \n", buffer);
	    	}
	    	else {
	    		printf("break\n");
	    		break;
	    	}
	    }

	    fclose(fp);
	    return 1;
		// printf("Line of file received from server: %s \n", buffer);
	}
	
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

int change_directory(char* cur_dir_client, char* new_dir){
	// check new dir exists
	printf("New dir requested: %s \n", new_dir);
	char* path[256];
	sprintf(path, "%s%s/", cur_dir_client, new_dir);
	printf("new requested dir %s \n", path);

	struct stat path_stat;
	int is_dir = stat(path, &path_stat);

    if (S_ISDIR(path_stat.st_mode)) {
    	strcpy(cur_dir_client, path);
    	return 0;
    }
    return -1;
}

int list_directory(char* cur_dir_client){
	char cmd[40];
	sprintf(cmd, "ls %s", cur_dir_client);
	system(cmd);
	return 0;
}




