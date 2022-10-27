#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include <dirent.h>

#include "ftp_server.h"

#define LOGINFILE "users.txt"

int main() {

	int auth = 0;
	char username[256];
	//1. socket();
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);

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
	printf("Server started and is listening...\n");

	fd_set full_fdset,ready_fdset;
	FD_ZERO(&full_fdset);
	FD_SET(server_fd,&full_fdset);
	int max_fd = server_fd;
	int result = 0;
	int to_stop = 0;
	char init_message[] = "220 Service ready for new user.";

	//4. accept()
	while(1) {	
		ready_fdset = full_fdset;
		if (to_stop == 1){
			break;
		}
		if (select(max_fd+1, &ready_fdset, NULL, NULL, NULL) < 0) {
			perror("select");
			return -1;
		}

		for (int fd = 0; fd<=max_fd; fd++) {
			if (FD_ISSET(fd, &ready_fdset)) {
				if (fd == server_fd) {
					int new_fd = accept(server_fd, NULL, NULL);
					printf("New client connected.\n");

					// let the client know the server is ready
					send(new_fd, init_message, strlen(init_message), 0);

					FD_SET(new_fd, &full_fdset);
					if (new_fd > max_fd) max_fd = new_fd;    //Update the max_fd if new socket has higher FD
				}
				else {
					printf("\n");
					result = serve_client(fd, &auth, &username);
					// need to do 0 option to serve client
					if (result == -1){ // if client disconnected
						FD_CLR(fd,&full_fdset);
					}
					// to remove later, debugging for now
					// if (result == -2){ // for fork
					// 	// FD_CLR(server_fd,&full_fdset);
					// 	// printf("Clearing client (-2)\n");
					// 	printf("received -2 \n");
					// 	// to_stop = 1;
					// }
				}
			}
		}
	}
	if (to_stop == 1){
		return 0;
	}
	//6. close());
	// printf("Closing server_fd\n");
	close(server_fd);
	return 0;
}

//=================================
//  returns -1 if client wants to disconnect, 0 if invalid input/other problem, 1 of OK
int serve_client(int client_fd, int* auth, char* username) {
	char message[256];	
	bzero(&message, sizeof(message));
	
	char buffer[256];	
	bzero(&buffer, sizeof(buffer));

	if (recv(client_fd, message, sizeof(message), 0) < 0) {
		perror("recv");
		return 0;
	}

	// check if the client disconnected or wants to disconnect
	if (strlen(message) == 0) {
		printf("Client disconnected.\n");
		close(client_fd);
		return -1;
	}

	// check command
	if (strncmp(message, "PORT", 4) == 0){
		printf("PORT command received.\n");

		int pid = fork();
		
		if (pid == 0) {
			// close(server_fd);

			// get client address and port
			int client_ip_arr[4];
			int client_port_arr[2];
			char client_ip[30];
			int client_port;
			sscanf(message, "PORT %d,%d,%d,%d,%d,%d", &client_ip_arr[0], &client_ip_arr[1], &client_ip_arr[2], &client_ip_arr[3], &client_port_arr[0], &client_port_arr[1]);
			sprintf(client_ip, "%d.%d.%d.%d", client_ip_arr[0], client_ip_arr[1], client_ip_arr[2],client_ip_arr[3]);
			client_port = client_port_arr[0] * 256 + client_port_arr[1];
			// printf("client IP: %s\n", client_ip);
			// printf("client PORT: %d\n", client_port);

			int data_sd = create_data_socket(client_ip, client_port);
			if (data_sd == -1) { return 0; }
		   	printf("Connected to client on new port \n");

			// server sends acknowledgement to client that it received the port
			// bzero(&message, sizeof(message));
			strcpy(message, "200 PORT command successful.");
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
				printf("STOR command received.\n");
				data_transfer = handle_STOR(data_sd, message);
			}
			else if (strncmp(message, "RETR", 4) == 0) {
				printf("RETR command received.\n");

				char path[256];
				sscanf(message, "RETR %s", &path);

				char response[256];
				bzero(&response,sizeof(response));

				// check if file exists in current server directory
				if (check_file_exists(path) == -1) {
					strcpy(response, "no file");
					send(data_sd, response, strlen(response), 0);
					bzero(&response,sizeof(response));
					data_transfer = 0;
				}
				else {
					strcpy(response, "150 File status okay; about to open data connection. \n");
					send(client_fd, response, strlen(response), 0);
					bzero(&response,sizeof(response));
					data_transfer = handle_RETR(data_sd, message);
				}
				
			}
			else if (strncmp(message, "LIST", 4) == 0) {
				printf("LIST command received.\n");
				data_transfer = handle_LIST(data_sd, message);
			}

			bzero(&message, sizeof(message));
			if (data_transfer == 0) { 
				close(data_sd);
				strcpy(message, "550 No such file or directory.");
				send(client_fd, message, strlen(message), 0);
				bzero(&message, sizeof(message));
				return 0; 
			}
			else { // data_transfer = 1
				close(data_sd);
				strcpy(message, "226 Transfer completed.");
				send(client_fd, message, strlen(message), 0);
				bzero(&message, sizeof(message));
				printf("File transferred to client. \n");
				return 1;
			}
		    // return -2;

		}
		else {
			close(client_fd); 
			return -1;
		}
	}

	else if (strncmp(message, "USER", 4) == 0) {
		printf("USER command received.\n");
		printf("Checking for usernames...\n");
		handle_loginuser(client_fd, message, username, auth);
	}

	else if (strncmp(message, "PASS", 4) == 0) {
		printf("PASS command received.\n");
		if (*auth == 1){
			printf("Checking for passwords...\n");
			handle_loginpass(client_fd, message, username, auth);
		}else{
			printf("Bad sequence of commands.\n");
			bzero(buffer, sizeof(buffer));
			strcpy(buffer, "503 Bad sequence of commands.");
			send(client_fd, buffer, strlen(buffer), 0);
			bzero(&buffer,sizeof(buffer));
		}
	}

	else if (strncmp(message, "CWD", 3) == 0) {
		printf("CWD command received.\n");

		char new_dir[256];
		sscanf(message, "CWD %s", &new_dir);

		bzero(&message, sizeof(message));
		strcpy(message, "please send username");
		send(client_fd, message, strlen(message), 0);

		bzero(&message, sizeof(message));
		if (recv(client_fd, message, sizeof(message), 0) < 0) {
				perror("recv");
				return 0;
		}

		int result = change_directory(&message, new_dir, username);
		if (result == -1) {
			printf("Error: could not change current server directory.\n");
			bzero(&message, sizeof(message));
			strcpy(message, "550 No such file or directory.");
		}
		else {
			char tmp_response[256];
			printf("Server directory updated.\n");
			sprintf(tmp_response, "200 directory changed to %s", message);
			bzero(&message, sizeof(message));
			strcpy(message, tmp_response);
		}
		send(client_fd, message, strlen(message), 0);
	}

	else if (strncmp(message, "PWD", 3) == 0) {
		printf("PWD command received.\n");

		bzero(&message, sizeof(message));
		strcpy(message, "ready for PWD");
		send(client_fd, message, strlen(message), 0);

		bzero(&message, sizeof(message));
		if (recv(client_fd, message, sizeof(message), 0) < 0) {
			perror("recv");
			return 0;
		}
		printf("current server dir: %s \n", message);
		
		char pwd_response[256];
		sprintf(pwd_response, "257 %s", message );
		bzero(&message, sizeof(message));
		strcpy(message, pwd_response);
		send(client_fd, message, strlen(message), 0);
		bzero(&message, sizeof(message));
	}

	else if (strncmp(message, "QUIT", 4) == 0) {
		printf("QUIT command received.\n");

		bzero(&message, sizeof(message));
		strcpy(message, "221 Service closing control connection.");
		send(client_fd, message, strlen(message), 0);
		bzero(&message, sizeof(message));

		close(client_fd);
		printf("Client disconnected \n");
		return -1;
	}
	else { // invalid command
		bzero(&message, sizeof(message));
		strcpy(message, "202 Command not implemented.");
		send(client_fd, message, strlen(message), 0);
		bzero(&message, sizeof(message));
		printf("Invalid command. \n");
	}

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

	char path[256];
	sscanf(message, "STOR %s", &path);
	// printf("Path to file: %s \n", path);

	recv(data_sd, buffer, sizeof(buffer), 0);
	// printf("%s", buffer);

	if (strncmp(buffer, "no file", 7) == 0){
		return 0;
	}
	else {
		printf("Opening the file \n");
		FILE *fp;

	    if (!(fp = fopen (path, "wb"))) {    /* open/validate file open */
	        perror ("fopen-file");
	        return 0;
	    }


	    // write first line already received in buffer to file
	    fprintf(fp, "%s", buffer);

	    while (1) {
	    	bzero(buffer, sizeof(buffer));
	    	recv(data_sd, buffer, sizeof(buffer), 0);
	    	 if (strlen(buffer) > 0) {
	    	 	// printf("just received a line: %s \n", buffer);
				fprintf(fp, "%s", buffer);
				fflush(fp);  //Flushes buffer and prints to a file
	    	 }
	    	 else {
				printf("\nEnd of file now \n");
				// recv(data_sd, buffer, sizeof(buffer), 0);
	    	 	break;
	    	 }
	    }

	    fclose(fp);
	    return 1;
		// printf("Line of file received from server: %s \n", buffer);
	}
}

int handle_RETR(int data_sd, char* message) {

	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));

	char* path[256];
	sscanf(message, "RETR %s", &path);
	// printf("Path to file: %s \n", path);

	// check if file exists in current server directory
	// if (check_file_exists(path) == -1) {
	// 	strcpy(buffer, "no file");
	// 	send(data_sd, buffer, strlen(buffer), 0);
	// 	bzero(&buffer,sizeof(buffer));
	// 	return 0;
	// }

	// if file exists, starts transfer
	FILE *fp;

    if (!(fp = fopen (path, "rb"))) {    /* open/validate file open */
        perror ("fopen-file");
        strcpy(buffer, "no file");
		send(data_sd, buffer, strlen(buffer), 0);
		bzero(&buffer,sizeof(buffer));
        return 0; // failure
    }
	printf("sending data now \n");
    while(fgets(buffer, 256, fp)) {
	    send(data_sd, buffer, strlen(buffer), 0);
	    bzero(&buffer,sizeof(buffer));
	}
    fclose(fp);
    return 1; // success
}

int handle_LIST(int data_sd, char* message) {
	char buffer[256]; // 256 is a ramdom number for now
	bzero(buffer, sizeof(buffer));
	
	char* path[256];
	sscanf(message, "LIST %s", &path);
	// printf("Path to directory: %s \n", path);

	// check if file exists in current server directory
	if (check_dir_exists(path) == -1) {
		strcpy(buffer, "no file");
		send(data_sd, buffer, strlen(buffer), 0);
		bzero(&buffer,sizeof(buffer));
		return 0;
	}

	strcpy(buffer, "150 File status okay; about to open data connection. \n");
	send(data_sd, buffer, strlen(buffer), 0);
	bzero(&buffer,sizeof(buffer));

	// bzero(&buffer, sizeof(buffer));
	// strcpy(buffer, d_buffer);
	// send(data_sd, buffer, strlen(buffer), 0);

	DIR *d;
    struct dirent *dir;
	char* d_buffer[256];
    d = opendir(path);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            //printf("%s\n", dir->d_name);
			sprintf(d_buffer, "%s ", dir->d_name);
			bzero(&buffer, sizeof(buffer));
			strcpy(buffer, d_buffer);
			send(data_sd, buffer, strlen(buffer), 0);
        }
        closedir(d);
    }

    return 1;
}

int handle_loginuser(int client_fd, char* message, char* username, int* auth){
	FILE *fp;
    char fname[256],fpass[256],user[256],buffer[256];   
	sscanf(message, "USER %s\n", &user);
	user[strcspn (user, "\n")] = 0;  
	strcpy(username, user);

    if (!(fp = fopen (LOGINFILE, "r"))) {    
        perror ("fopen-file");
        exit (EXIT_FAILURE);
    }

	while(fgets(buffer, 256, fp) != NULL) {
		sscanf(buffer, "%s %s", fname, fpass);
		if ((strcmp(username, fname) == 0)) {  
			bzero(buffer, sizeof(buffer));
			strcpy(buffer, "331 Username OK, need password.");
			send(client_fd, buffer, strlen(buffer), 0);
			bzero(&buffer,sizeof(buffer));
			printf("Valid username.\n");
			*auth = 1;
			return 1; 
    	}
	}

    fclose (fp);

	printf("Failed login. Try again.\n");
	bzero(buffer, sizeof(buffer));
	strcpy(buffer, "530 Not logged in.");
	send(client_fd, buffer, strlen(buffer), 0);
	bzero(&buffer,sizeof(buffer));

    return 0;     
}

int handle_loginpass(int client_fd, char* message, char* username, int* auth){
	FILE *fp;
    char fname[256],fpass[256],pass[256],buffer[256];   
	sscanf(message, "PASS %s\n", &pass);

	pass[strcspn (pass, "\n")] = 0;  

    if (!(fp = fopen (LOGINFILE, "r"))) {   
        perror ("fopen-file");
        exit (EXIT_FAILURE);
    }

	while(fgets(buffer, 256, fp) != NULL) {
		sscanf(buffer, "%s %s", fname, fpass);

		if ((strcmp(username, fname) == 0) && (strcmp(pass, fpass) == 0)) {  /* validate login */
			printf ("User has successfully logged in.\n");
			bzero(buffer, sizeof(buffer));
			strcpy(buffer, "230 User logged in, proceed.");
			send(client_fd, buffer, strlen(buffer), 0);
			bzero(&buffer,sizeof(buffer));
			*auth = 0;

			return 1;   
    		}
		}	

    fclose (fp);

	printf("Failed login. Try again.\n");
	bzero(buffer, sizeof(buffer));
	strcpy(buffer, "530 Not logged in.");
	send(client_fd, buffer, strlen(buffer), 0);
	bzero(&buffer,sizeof(buffer));

    return 0;    
}

int change_directory(char* cur_dir_server, char* new_dir, char* username){
	// check new dir exists
	char* path[256];

	if (strcmp(new_dir, "..") == 0) {
		char base_dir[256];
		sprintf(base_dir, "%s%s/", "server_directories/", username);
		if (strcmp(cur_dir_server, base_dir) == 0) {
			return -1; // if the cliesnt is at the base directory, cannot go to previous directory (NOT ALLOWED)
		}

		// process to go back to previous directory
		char tmp[256];
		strcpy(tmp, cur_dir_server);

		// get the index of the last / character to remove the end substring
		int i;
		int count = 2;
		int length = strlen(cur_dir_server);

		for (i = 2; i < length; i++) {
			if (cur_dir_server[length - i] == '/') {
				break;
			}
			count++;
		}
		tmp[length - i] = '\0';
		sprintf(path, "%s/", tmp);
	}
	else {
		sprintf(path, "%s%s/", cur_dir_server, new_dir);
	}
	if (check_dir_exists(path) == 0) {
		strcpy(cur_dir_server, path);
		return 0;
	}
	return -1;
}

int check_dir_exists(char* path){
	struct stat path_stat;
	if (stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {return 0; }
	return -1;
}

int check_file_exists(char* path){
	struct stat path_stat;
	if (stat(path, &path_stat) == 0) {return 0; }
	return -1;
}


