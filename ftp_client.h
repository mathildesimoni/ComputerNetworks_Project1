#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

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

#endif