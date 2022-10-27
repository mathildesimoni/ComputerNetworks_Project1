#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include <dirent.h>

#include "ftp_client.h"

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

    // printf("Client connected to server at %s:%hu \n", my_ip, my_port);
	printf("To display available commands, enter \"commands\" \n");

	char buffer[256]; // will store user input
	bzero(buffer, sizeof(buffer));
	int request_number = 0;
	int response; // will store server responses
	int display_commands = 0;
	char cur_dir_client[256];
	strcpy(cur_dir_client, "no_dir");

	char user_name[256];
	bzero(user_name, sizeof(user_name));

	char cur_dir_server[256];
	strcpy(cur_dir_server, "no_dir");

	int logged_in = 0;

	recv(server_sd, buffer, sizeof(buffer), 0);
	printf("%s \n", buffer);
	bzero(buffer, sizeof(buffer));

	while(1) {

		if (display_commands == 1) {
			display_commands = display_user_commands();
		}
		else {
			printf("ftp> ");
		}		
		
		// get input from user
		fgets(buffer, sizeof(buffer), stdin);
	    buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it

        // check if the client wants to print the commands
	    if (strcmp(buffer, "commands")==0) {
			display_commands = 1;
        }

        // check input
        else if (check_input_port(buffer) == 0) {
        	printf("Error: You cannot explicitely send a PORT command \n");
        }
        else { // proceed with the request
        	response = serve_user(server_sd, buffer, my_ip, my_port, &request_number, cur_dir_client, &logged_in, cur_dir_server, &user_name);
        	// if (response == 0) {
        	// 	printf("Error: could not send command to server \n");
        	// }
        	if (response == -1) {
        		// printf("Closing the connection to server \n");
	        	close(server_sd);
	            break;
        	}	
        }
        bzero(buffer, sizeof(buffer));		
        // printf("Current client directory: %s\n", cur_dir_client);
        // printf("Current server directory: %s\n", cur_dir_server);
        // printf("User logged in: %d \n", logged_in);
	}
	return 0;
}

// returns 0 if error
int serve_user(int server_sd, char* input, char* my_ip, unsigned short int my_port, int* request_number, char* cur_dir_client, int* logged_in, char* cur_dir_server, char* user_name) {

	int my_ip_arr[4];
	char message[256];
	bzero(message, sizeof(message));

	// for USER, PASS, CWD, PWD, not preprocessing needed, directly send to server
	if ((strncmp(input, "USER", 4) == 0) || (strncmp(input, "PASS", 4) == 0)) {

		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}

		// wait for server to send response message
		recv(server_sd, message, sizeof(message), 0);
		printf("%s \n", message);

		if ((strncmp("USER", input, 4) == 0) && (strncmp("331", message, 3) == 0)){
			char username[256];
			char tmp_dir_client[256];
			sscanf(input, "USER %s", &username);
			sprintf(tmp_dir_client, "client_directories/%s/", username);
			strcpy(cur_dir_client, tmp_dir_client);
			strcpy(user_name, username);

			char tmp_dir_server[256];
			sprintf(tmp_dir_server, "server_directories/%s/", username);
			strcpy(cur_dir_server, tmp_dir_server);
		}

		if ((strncmp("PASS", input, 4) == 0) && (strncmp("230", message, 3) == 0)){
			*logged_in = 1;
		}
	}

	else if (strncmp(input, "CWD", 3) == 0){
		if(*logged_in == 0){
			printf("530 Not logged in.\n");
		}
		else{
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
			printf("%s \n", message);
			if (strncmp(message, "200", 3) == 0) {
				char* path[256];
				sscanf(message, "200 directory changed to %s", &path);
				// sprintf(path, "%s%s/", cur_dir_server, new_dir);
				strcpy(cur_dir_server, path);
			}
		}
	}

	else if (strncmp(input, "PWD", 3) == 0) {
		if(*logged_in == 0){
			printf("530 Not logged in.\n");
		}
		else{
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
			//receive 257 message from server
			bzero(message, sizeof(message));
			recv(server_sd, message, sizeof(message), 0);
			printf("%s\n", message);
		}
	}

	// for the 3 next if conditions, no server needed, commands implemented locally
	else if (strncmp(input, "!LIST", 5) == 0) {
		if(*logged_in == 0){
			printf("530 Not logged in.\n");
		}
		else{
			list_directory(cur_dir_client);
		}
	}

	else if (strncmp(input, "!CWD", 4) == 0) {
		if(*logged_in == 0){
			printf("530 Not logged in.\n");
		}
		else{
			char new_dir[256];
			sscanf(input, "!CWD %s", &new_dir);

			int result = change_directory(cur_dir_client, new_dir, &user_name);
			if (result == -1) {
				printf("Error: could not change current client directory \n");
			}
		}
	}

	else if (strncmp(input, "!PWD", 4) == 0) {
		if(*logged_in == 0){
			printf("User must successfully login first.\n");
		}
		else{
			printf("Current client directory: %s\n", cur_dir_client);
		}
	}

	// send to server
	else if (strncmp(input, "QUIT", 4) == 0) {
		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}

		// wait for server to send response message
		recv(server_sd, message, sizeof(message), 0);
		printf("%s \n", message);  

		if (strncmp(message, "221", 3) != 0) {
			printf("Error: could not quit connection \n");
			return 0;
		}
		return -1;
	}

	else if ((strncmp(input, "STOR", 4) == 0) || (strncmp(input, "RETR", 4) == 0) || (strncmp(input, "LIST", 4) == 0)) {
		if(*logged_in == 0){
			printf("530 Not logged in.\n");
		}
		else{
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
				send(server_sd, message, strlen(message), 0);				
				data_transfer = list_files(data_server_sd);
			}
			else {
				char file_name[256];
				if (strncmp(input, "STOR", 4) == 0) {
					sscanf(input, "STOR %s", &file_name);
					bzero(message, sizeof(message));
					sprintf(message, "STOR %s%s", cur_dir_server, file_name);
					
					send(server_sd, message, strlen(message), 0);
					data_transfer = upload_file(data_server_sd, file_name, cur_dir_client, cur_dir_server);
				}
				else { // RETR command
					sscanf(input, "RETR %s", &file_name);
					bzero(message, sizeof(message));
					sprintf(message, "RETR %s%s", cur_dir_server, file_name);
					
					send(server_sd, message, strlen(message), 0);
					data_transfer = download_file(data_server_sd, file_name, cur_dir_client, cur_dir_server);
				}
			}
			bzero(message, sizeof(message));
			recv(server_sd, message, sizeof(message), 0);
			printf("%s \n", message); 

			if (data_transfer == 0) {
				close(data_client_sd);
				*request_number += 1;
				return 0; 
			}

			// close the data connection
			close(data_client_sd);
			*request_number += 1;
		}
	}

	else {
		if (send(server_sd, input, strlen(input),0) < 0) {
		    perror("send");
		    return 0;
		}
		bzero(message, sizeof(message));
		recv(server_sd, message, sizeof(message), 0);
		printf("%s \n", message); 
	}
	return 1;
}


// need to check that the user doesn't input a PORT command explicitely
// indeed, in that case, the server would open a new connection on a random port (decided by the user)
// this could block the application
int check_input_port(char* input) {
	if (strncmp(input, "PORT", 4) == 0) { return 0;} // invalid
	return 1; // valid
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

	// printf("Client is listening...\n");
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
	// printf("SENT PORT COMMAND \n");
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
	// printf("Server connected on IP %s and port %hu \n", server_data_IP, ntohs(server_data_addr.sin_port));

	// wait for server to send 200 OK response
	recv(server_sd, message, sizeof(message), 0);
	printf("%s \n", message);

	if (strncmp(message, "200", 3) != 0) {
		printf("Error: could not establish a data connection \n");
		return -1;
	}
	bzero(message, sizeof(message));

	return data_server_sd;
}

int upload_file(int data_server_sd, char* file_name, char* cur_dir_client, char* cur_dir_server) {
	char buffer[256]; 
	bzero(buffer, sizeof(buffer));

	char client_path[256];
	bzero(client_path, sizeof(client_path));
	sprintf(client_path, "%s%s", cur_dir_client, file_name);
	printf("client path: %s \n", client_path);

	// if file exists, starts transfer
	FILE *fp;

    if (!(fp = fopen (client_path, "rb"))) {   
        perror ("fopen-file");
        strcpy(buffer, "no file");
		send(data_server_sd, buffer, sizeof(buffer), 0);
		bzero(&buffer,sizeof(buffer));
        return 0; // failure
    }
    while(fgets(buffer, 256, fp) != NULL) {
		send(data_server_sd, buffer, sizeof(buffer), 0);
		bzero(&buffer,sizeof(buffer));
	}

	close(data_server_sd);
    fclose(fp);
    return 1; // success
}

int download_file(int data_server_sd, char* file_name, char* cur_dir_client, char* cur_dir_server){
	
	char client_path[256];
	bzero(client_path, sizeof(client_path));
	sprintf(client_path, "%s%s", cur_dir_client, file_name);

	char buffer[256]; 
	bzero(buffer, sizeof(buffer));

	recv(data_server_sd, buffer, sizeof(buffer), 0);

	if (strncmp(buffer, "no file", 7) == 0){
		return 0;
	}
	else {

		char file_ok[256];
		
		bzero(file_ok, sizeof(file_ok));
	    recv(data_server_sd, file_ok, sizeof(file_ok), 0);
		printf("%s", file_ok);


		FILE *fp;

	    if (!(fp = fopen (client_path, "wb"))) {    /* open/validate file open */
	        perror ("fopen-file");
	        return 0;
	    }

	    // write first line already received in buffer to file
	    fprintf(fp, "%s", buffer);

	    while (1) {
	    	bzero(buffer, sizeof(buffer));
			// printf("Ready to receive \n");
	    	recv(data_server_sd, buffer, sizeof(buffer), 0);
	    	// printf("%s\n", buffer);
	    	if (strlen(buffer) > 0) {
	    		fprintf(fp, "%s \n", buffer);
	    	}
	    	else {
				// printf("End of file now \n");
	    		break;
	    	}
	    }
	    fclose(fp);
	    return 1;
	}	
}

int list_files(int data_server_sd) {
	char buffer[256]; 
	bzero(buffer, sizeof(buffer));

	// 150 message
	recv(data_server_sd, buffer, sizeof(buffer), 0);
	printf("%s", buffer);
	bzero(buffer, sizeof(buffer));

	recv(data_server_sd, buffer, sizeof(buffer), 0);
	printf("%s \n", buffer);

	return 1;
}

int display_user_commands() {
	printf("Available commands:\n");
	printf("- USER username: to start authentification \n- PASS password: to finish authentification (after USER command) \n- STOR filename: upload a local file from current client directory to current server directory \n- RETR filename: download a file from current server directory to current client directory \n- LIST: list all the files under current server directory \n- !LIST: list all the files under current client directory \n- CWD foldername: change current server directory \n- !CWD foldername: change current client directory \n- PWD: display current server directory \n- !PWD: display current client directory \n- QUIT: quit the FTP session and closes the control TCP connection \n");
	printf("ftp> ");
	return 0;
}

int change_directory(char* cur_dir_client, char* new_dir, char* user_name){
	// check new dir exists
	char* path[256];

	if (strcmp(new_dir, "..") == 0) {
		char base_dir[256];
		sprintf(base_dir, "%s%s/", "client_directories/", user_name);
		if (strcmp(cur_dir_client, base_dir) == 0) {
			return -1; // if the cliesnt is at the base directory, cannot go to previous directory (NOT ALLOWED)
		}

		// process to go back to previous directory
		char tmp[256];
		strcpy(tmp, cur_dir_client);

		// get the index of the last / character to remove the end substring
		int i;
		int count = 2;
		int length = strlen(cur_dir_client);

		for (i = 2; i < length; i++) {
			if (cur_dir_client[length - i] == '/') {
				break;
			}
			count++;
		}
		tmp[length - i] = '\0';
		sprintf(path, "%s/", tmp);
	}
	else {
		sprintf(path, "%s%s/", cur_dir_client, new_dir);
	}

	struct stat path_stat;
	int is_dir = stat(path, &path_stat);
	if (S_ISDIR(path_stat.st_mode)) {
    	strcpy(cur_dir_client, path);
    	printf("Client directory updated: %s \n", path);
		return 0;
    }
	return -1;
}

//list files within directory for LIST command
int list_directory(char* cur_dir_client){
	DIR *d;
    struct dirent *dir;
	char* d_buffer[256];
    d = opendir(cur_dir_client);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
			sprintf(d_buffer, "%s ", dir->d_name);
			printf("%s\n", d_buffer);
        }
        closedir(d);
    }
	return 0;
}




