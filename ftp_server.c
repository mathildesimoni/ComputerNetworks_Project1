#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

int serve_client(int client_fd);
int get_command(char* message);
int main()
{
	//1. socket();
	int server_fd = socket(AF_INET,SOCK_STREAM,0);
	printf("server_fd = %d \n",server_fd);
	if(server_fd<0)
	{
		perror("socket");
		return -1;
	}
	if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int))<0)
	{
		perror("setsock");
		return -1;
	}
	//2. bind ();
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(21);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(server_fd,(struct sockaddr*)&server_address,sizeof(server_address))<0)
	{
		perror("bind");
		return -1;
	}


	//3. listen()
	if(listen(server_fd,5)<0)
	{
		perror("listen");
		return -1;
	}
	fd_set full_fdset,ready_fdset;
	FD_ZERO(&full_fdset);
	FD_SET(server_fd,&full_fdset);
	int max_fd = server_fd;

	int command;

	//4. accept()
	while(1)
	{	
		// printf("max_fd=%d\n",max_fd);
		ready_fdset = full_fdset;
		if(select(max_fd+1,&ready_fdset,NULL,NULL,NULL)<0)
		{
			perror("select");
			return -1;
		}

		for(int fd = 0; fd<=max_fd; fd++)
		{
			if(FD_ISSET(fd,&ready_fdset))
			{
				if(fd==server_fd) 
				{
					int new_fd = accept(server_fd,NULL,NULL);
					printf("client fd = %d \n",new_fd);
					FD_SET(new_fd,&full_fdset);
					if(new_fd>max_fd) max_fd=new_fd;    //Update the max_fd if new socket has higher FD
				}
				else if(serve_client(fd)==-1)
				{
					FD_CLR(fd,&full_fdset);
				}
			}
		}
	}

	//6. close());
	close(server_fd);
}
//=================================
int serve_client(int client_fd)
{
	char message[100];	
	bzero(&message,sizeof(message));
	if(recv(client_fd,message,sizeof(message),0)<0)
	{
		perror("recv");
		return 0;
	}

	int command = get_command(message);
	// printf("Command: %d \n", command);

	if(strcmp(message,"bye")==0)
	{
		printf("Client disconnected \n");
		close(client_fd);
		return -1;
	}

	if (strlen(message) == 0){
		printf("Client disconnected \n");
		close(client_fd);
		return -1;
	}

	return 0;
}

int get_command(char* message)
{
	printf("%s \n",message);
	// if (strcmp(message,"PORT")==0){
	// 	get_port()
	// }
	return 0;
}


