#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>

#include<unistd.h>
#include<stdlib.h>

int main()
{
	//socket
	int server_sd = socket(AF_INET,SOCK_STREAM,0);
	if(server_sd<0)
	{
		perror("socket:");
		exit(-1);
	}
	//setsock
	int value  = 1;
	setsockopt(server_sd,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(21);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //INADDR_ANY, INADDR_LOOP

	//connect
    if(connect(server_sd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("connect");
        exit(-1);
    }
	
	//accept
	char buffer[256];

	while(1)
	{
		printf("Available commands:\n");
		printf("(commands)\n");
		printf("Please enter a command: ");
	    fgets(buffer,sizeof(buffer),stdin);
	    buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it
	    if(strcmp(buffer,"bye")==0)
        {
			send(server_sd,buffer,strlen(buffer),0);
        	printf("closing the connection to server \n");
        	close(server_sd);
            break;
        }
        if(send(server_sd,buffer,strlen(buffer),0)<0)
        {
            perror("send");
            exit(-1);
        }
        bzero(buffer,sizeof(buffer));			
	}

	return 0;
}
