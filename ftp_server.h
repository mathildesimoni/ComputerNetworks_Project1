#ifndef FTP_SERVER_H
#define FTP_SERVER_H

int serve_client(int client_fd);
int create_data_socket(char* client_ip, int client_port);
int handle_STOR(int data_sd, char* message);
int handle_RETR(int data_sd, char* message);
int handle_LIST(int data_sd, char* message);
int handle_loginuser(int client_fd, char* message);
int handle_loginpass(int client_fd, char* message);
int change_directory(char* cur_dir_server, char* new_dir);
int check_dir_exists(char* path);
int check_file_exists(char* path);

#endif