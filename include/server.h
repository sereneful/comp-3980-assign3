#ifndef SERVER_H
#define SERVER_H

#define BUFFER_SIZE 1024

void __attribute__((noreturn)) sigint_handler(int signum);
char                           upper_filter(char c);
char                           lower_filter(char c);
char                           null_filter(char c);

typedef char (*filter_func)(char);

filter_func select_filter(const char *filter_name);
void        apply_filter(char *response, const char *client_string, filter_func filter);
void        handle_client(int client_socket);

#endif    // SERVER_H
