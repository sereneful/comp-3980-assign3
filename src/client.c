#include "client.h"
#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ARG 5
#define PORT 12345
#define BUFFER_SIZE 1024

void print_usage(const char *prog_name)
{
    printf("Usage: %s -s <string> -f <filter>\n", prog_name);
    printf("Options:\n");
    printf("  -s <string>    The string to be transformed\n");
    printf("  -f <filter>    The filter type (upper, lower, null)\n");
}

int is_filter_valid(const char *filter)
{
    return (strcmp(filter, "upper") == 0 || strcmp(filter, "lower") == 0 || strcmp(filter, "null") == 0);
}

int main(int argc, char *argv[])
{
    int                sock = 0;
    struct sockaddr_in serv_addr;
    int                opt;

    char        buffer[BUFFER_SIZE];
    char        response[BUFFER_SIZE];
    const char *client_string    = NULL;
    const char *filter_type      = NULL;
    size_t      total_bytes_sent = 0;
    size_t      message_len      = 0;
    size_t      total_bytes_read = 0;

    if(argc > MAX_ARG)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Getopt to parse command line arguments
    while((opt = getopt(argc, argv, "s:f:")) != -1)
    {
        switch(opt)
        {
            case 's':
                client_string = optarg;
                break;
            case 'f':
                filter_type = optarg;
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(client_string == NULL || filter_type == NULL)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(strlen(client_string) == 0)
    {
        printf("Error: The string is empty.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(!is_filter_valid(filter_type))
    {
        printf("Error: Invalid filter type '%s'.\n", filter_type);
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("Socket creation error");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);

    // Convert IPv4 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        close(sock);
        return -1;
    }

    // Connect to the server
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection Failed");
        close(sock);
        return -1;
    }

    // Concatenate the input string
    snprintf(buffer, sizeof(buffer), "%s:%s\n", client_string, filter_type);

    // Initialize variables before use
    total_bytes_sent = 0;
    message_len      = strlen(buffer);

    // Send the data to the server
    while(total_bytes_sent < message_len)
    {
        ssize_t bytes_sent = write(sock, buffer + total_bytes_sent, message_len - total_bytes_sent);
        if(bytes_sent == -1)
        {
            perror("Error writing to socket");
            close(sock);
            exit(EXIT_FAILURE);
        }
        total_bytes_sent += (size_t)bytes_sent;
    }

    // Initialize total_bytes_read before use
    total_bytes_read = 0;

    // Read the processed response from the server
    while(1)
    {
        ssize_t bytes_read = read(sock, response + total_bytes_read, sizeof(response) - total_bytes_read - 1);
        if(bytes_read == -1)
        {
            perror("Error reading from socket");
            close(sock);
            exit(EXIT_FAILURE);
        }
        if(bytes_read == 0)
        {
            // Server closed the connection
            break;
        }
        total_bytes_read += (size_t)bytes_read;
        response[total_bytes_read] = '\0';
        // Check if we've received a newline character
        if(strchr(response, '\n') != NULL)
        {
            break;
        }
        if(total_bytes_read >= sizeof(response) - 1)
        {
            fprintf(stderr, "Response too long\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }

    if(total_bytes_read == 0)
    {
        fprintf(stderr, "No data received from server\n");
    }
    else
    {
        // Remove the newline character if present
        char *newline_pos = strchr(response, '\n');
        if(newline_pos != NULL)
        {
            *newline_pos = '\0';
        }
        printf("Processed string: %s\n", response);
    }

    close(sock);

    return 0;
}
