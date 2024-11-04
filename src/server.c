#include "server.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 12345
#define BUFFER_SIZE 1024

// Signal handler for SIGINT (graceful shutdown)
void __attribute__((noreturn)) sigint_handler(int signum)
{
    const char *message = "Server shutting down.\n";

    (void)signum;
    write(STDOUT_FILENO, message, strlen(message));

    _exit(0);
}

// Uppercase transformation
char upper_filter(char c)
{
    if(isalpha((unsigned char)c))
    {
        return (char)toupper((unsigned char)c);
    }
    return c;
}

// Lowercase transformation
char lower_filter(char c)
{
    if(isalpha((unsigned char)c))
    {
        return (char)tolower((unsigned char)c);
    }
    return c;
}

// Null transformation (no changes)
char null_filter(char c)
{
    return c;
}

// Function pointer type for filter functions
typedef char (*filter_func)(char);

// Select the appropriate filter function
filter_func select_filter(const char *filter_name)
{
    // Uppercase transformation
    if(strcmp(filter_name, "upper") == 0)
    {
        return upper_filter;
    }
    // Lowercase transformation
    if(strcmp(filter_name, "lower") == 0)
    {
        return lower_filter;
    }
    // Null transformation
    return null_filter;
}

// Applying the filter to the entire string
void apply_filter(char *response, const char *client_string, filter_func filter)
{
    size_t i;
    size_t max_len = BUFFER_SIZE - 2;    // Leave space for '\n' and '\0'

    for(i = 0; i < max_len && client_string[i] != '\0'; i++)
    {
        response[i] = filter(client_string[i]);
    }

    // Append newline
    response[i++] = '\n';
    response[i]   = '\0';
}

// Function to handle client requests
void handle_client(int client_socket)
{
    char        buffer[BUFFER_SIZE];
    char        response[BUFFER_SIZE];
    const char *client_string;
    const char *filter_name;
    filter_func filter;
    char       *saveptr;
    ssize_t     bytes_read;
    size_t      total_bytes_read    = 0;
    size_t      total_bytes_written = 0;
    size_t      response_len;

    // Read data from the client socket
    while((bytes_read = read(client_socket, buffer + total_bytes_read, sizeof(buffer) - total_bytes_read - 1)) > 0)
    {
        total_bytes_read += (size_t)bytes_read;
        buffer[total_bytes_read] = '\0';
        // Check if we've received a newline character
        if(strchr(buffer, '\n') != NULL)
        {
            break;
        }
        if(total_bytes_read >= sizeof(buffer) - 1)
        {
            fprintf(stderr, "Input too long\n");
            // Do not close client_socket here
            return;
        }
    }

    if(bytes_read == -1)
    {
        perror("Error reading from socket");
        // Do not close client_socket here
        return;
    }

    buffer[total_bytes_read] = '\0';    // Null-terminate the string

    printf("\nServer received data.\n");

    // The input string taken as string and filter
    client_string = strtok_r(buffer, ":\n", &saveptr);
    filter_name   = strtok_r(NULL, ":\n", &saveptr);

    if(filter_name == NULL || client_string == NULL)
    {
        printf("Error: Invalid input format. Exiting process.\n");
        // Do not close client_socket here
        return;
    }

    printf("String: %s Filter: %s\n", client_string, filter_name);

    // Select the appropriate filter function
    filter = select_filter(filter_name);

    // Apply the filter to the client's string
    apply_filter(response, client_string, filter);

    printf("Processed response: %s\n", response);

    // Get the length of the response
    response_len = strlen(response);

    // Write the processed string back to the client
    while(total_bytes_written < response_len)
    {
        ssize_t bytes_written = write(client_socket, response + total_bytes_written, response_len - total_bytes_written);
        if(bytes_written == -1)
        {
            perror("Error writing to socket");
            // Do not close client_socket here
            return;
        }
        total_bytes_written += (size_t)bytes_written;
    }

    printf("\nResponse sent to client.\n");
    printf("Press Ctrl+C to stop or wait for another input.\n");

    // Do not close client_socket here
}

int main(void)
{
    int                server_fd;
    struct sockaddr_in address;
    int                opt     = 1;
    int                addrlen = sizeof(address);
    pid_t              pid;

    // Create socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    // Bind the socket to the port
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if(listen(server_fd, 3) < 0)
    {
        perror("Listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running. Press Ctrl+C to stop.\n");

    // Set up the signal handler for SIGINT
    signal(SIGINT, sigint_handler);

    // To avoid zombie processes
    signal(SIGCHLD, SIG_IGN);

    // Main server loop to handle client requests
    while(1)
    {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if(new_socket < 0)
        {
            perror("Accept");
            continue;
        }

        pid = fork();
        if(pid < 0)
        {
            perror("Fork");
            close(new_socket);
            continue;
        }

        if(pid == 0)
        {
            // Child process
            close(server_fd);    // Child doesn't need the listening socket
            handle_client(new_socket);
            close(new_socket);    // Close the socket here
            exit(0);
        }

        // Parent process
        close(new_socket);
    }
}
