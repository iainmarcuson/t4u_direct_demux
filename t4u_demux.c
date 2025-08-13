// t4u_demux.c
// Implements the Sydor Technologies T4U Demultiplexor
// Copyright (C) 2025 Sydor Technologies

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <errno.h>

#include "t4u_demux.h"

int32_t parse_config_file(FILE *config_file, struct Client_Addr **client_list, uint32_t *num_clients, uint16_t src_port);

int32_t main_loop(struct Client_Addr *client_list, uint32_t num_clients, int listen_socket);

int main(int argc, char *argv[])
{
    char *config_filename;
    uint16_t T4U_BASE_PORT = 10101;
    uint32_t client_chunks_allocated = 0;
    uint32_t num_clients = 0;
    struct Client_Addr *client_list = NULL;
    FILE *config_file;
    int32_t ret;
    int t4u_socket;
    struct sockaddr_in listen_socket_addr;

    if ( (argc != 2) && (argc != 3))
    {
        printf("Usage: t4u_demux <config file> [T4U base port]\n");
        return 1;
    }

    config_filename = argv[1];

    if (argc == 3)
    {
        int read_cnt;
        read_cnt = sscanf(argv[2], "%hu", &T4U_BASE_PORT);
        if (read_cnt != 1)
        {
            printf("Invalid T4U base port number specified.\n");
            return 1;
        }
    }

    config_file = fopen(config_filename, "r");

    if (config_file == NULL)
    {
        printf("Failed to open configuration file %s\n", config_filename);
        return 1;
    }

    ret = parse_config_file(config_file, &client_list, &num_clients, T4U_BASE_PORT);

    if (ret != ST_ERR_OK)
    {
        printf("Error parsing config file.\n");
        return 1;
    }

    printf("Read %u clients.\n", num_clients);

    // Create the base socket
    t4u_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (t4u_socket == -1)
    {
        perror("Error establishing listening socket");
        return 1;
    }
    
    listen_socket_addr.sin_family = AF_INET;
    listen_socket_addr.sin_port = htons(T4U_BASE_PORT);
    listen_socket_addr.sin_addr.s_addr = htonl(0); // All addresses
    ret = bind(t4u_socket, (struct sockaddr *)&listen_socket_addr, sizeof(listen_socket_addr));
    if (ret == -1)
    {
        perror("Error binding listening socket");
        return 1;
    }
    

    for (uint32_t client_idx=0; client_idx < num_clients; client_idx++)
    {
        client_list[client_idx].socket = socket(AF_INET,SOCK_DGRAM, 0);
    }

    ret = main_loop(client_list, num_clients, t4u_socket);
    
    return 0;
}

int32_t parse_config_file(FILE *config_file, struct Client_Addr **client_list, uint32_t *num_clients, uint16_t src_port)
{
    char *curr_line = NULL;
    ssize_t bytes_read;
    const size_t BASE_LEN = 128;
    size_t line_len;
    int32_t ret;
    uint8_t src_addr[4];
    uint8_t first_char;
    uint16_t dest_port;
    int args_read;
    uint32_t b_is_comment;
    uint32_t b_is_blank;
    uint32_t client_chunks_allocated = 0;
    struct sockaddr_in src_sock_addr;
    struct sockaddr_in dest_sock_addr;
    uint32_t line_cnt = 0;
    uint32_t client_cnt = 0;
    struct Client_Addr *active_list;
    
    ret = ST_ERR_OK;
    *client_list = NULL;
    
    curr_line = malloc(sizeof(char) * BASE_LEN);

    if (curr_line == NULL)
    {
        goto cleanup;
    }
    line_len = BASE_LEN;

    active_list = malloc(sizeof(struct Client_Addr) * CLIENT_ADDR_BLOCKS);
    client_chunks_allocated++;
    if (active_list == NULL)
    {
        goto cleanup;
    }

    src_sock_addr.sin_family = AF_INET;
    src_sock_addr.sin_port = htons(src_port);

    dest_sock_addr.sin_family = AF_INET;
    dest_sock_addr.sin_addr.s_addr = htonl((127 << 24)+1); // Destination address is localhost
    
    while(1)
    {
        bytes_read = getline(&curr_line, &line_len, config_file);
        
        if (bytes_read == -1) //EOF or error
        {
            int last_err = errno;
            if (feof(config_file))
            {
                break;
            }
            perror("System Failure reading config file");
            free(curr_line);
            return ST_ERR_FAIL;
        }
        line_cnt++;

        // Check for blank line or comment line
        first_char = '\0'; // Initialize
        args_read = sscanf(curr_line, " %c", &first_char);
        if ((bytes_read == 1) || (args_read == 0) || (first_char == '#')) // Blank or comment
        {
            continue; // Read the next line;
        }
        
        args_read = sscanf(curr_line, " %hhu.%hhu.%hhu.%hhu %hu ", src_addr, src_addr+1, src_addr+2, src_addr+3, &dest_port);

        if (args_read != 5)
        {
            printf("Error reading config line: \"%s\"\n", curr_line);
            goto cleanup;
        }

        client_cnt++;
        
        if (client_cnt >= (CLIENT_ADDR_BLOCKS))
        {
            printf("Error: no more than %i clients.\n", CLIENT_ADDR_BLOCKS);
            goto cleanup;
        }

        src_sock_addr.sin_addr.s_addr = htonl((src_addr[0] << 24)+(src_addr[1] << 16) + (src_addr[2] << 8) + src_addr[3]);
        printf("Client %2u: %hhu.%hhu.%hhu.%hhu %hu\n", client_cnt, src_addr[0], src_addr[1], src_addr[2], src_addr[3], dest_port);
        dest_sock_addr.sin_port = htons(dest_port);
        dest_sock_addr.sin_addr.s_addr = htonl((127 << 24)+1);

        active_list[client_cnt-1].t4u_addr = *((struct sockaddr_in *)&src_sock_addr);
        active_list[client_cnt-1].dest_addr = *((struct sockaddr_in *)&dest_sock_addr);

        //printf("Loaded Address: %x\n", ntohl(active_list[client_cnt-1].t4u_addr.sin_addr.s_addr));
      
                
    }

    *client_list = active_list;
    *num_clients = client_cnt;
    
    return ret;
    
cleanup:
    free(curr_line);
    curr_line = NULL;
    free(active_list);
    *client_list = NULL;
    printf("Problem on line %u.\n", line_cnt);
    ret = ST_ERR_FAIL;
    return ret;
}

int32_t main_loop(struct Client_Addr *client_list, uint32_t num_clients, int listen_socket)
{
    uint8_t *dgram_data;
    ssize_t dgram_size;
    struct sockaddr_in recv_addr;
    const uint32_t DGRAM_MAX_SIZE = 65536;
    socklen_t addr_size;
    ssize_t bytes_sent;

    dgram_data = malloc(sizeof(uint8_t) * DGRAM_MAX_SIZE); // Max packet size
    if (dgram_data == NULL)
    {
        printf("Error allocating message memory.\n");
        return 1;
    }

    while (1)
    {
        dgram_size = recvfrom(listen_socket, dgram_data, DGRAM_MAX_SIZE, 0,
                              (struct sockaddr *)&recv_addr, &addr_size); // Receive the data


        //printf("Received a packet of length %llu.\n", (long long unsigned)dgram_size);
        //printf("Address is %x\n", ntohl(recv_addr.sin_addr.s_addr));
        //printf("Dest address is %x\n", ntohl(client_list[1].t4u_addr.sin_addr.s_addr));
        
        // Search for the address among the clients
        for (uint32_t client_idx = 0; client_idx<num_clients; client_idx++)
        {
            if (recv_addr.sin_addr.s_addr == client_list[client_idx].t4u_addr.sin_addr.s_addr) // IP Address matches
            {
                //printf("Found a match.\n");
                //printf("Socket ID is %i\n", client_list[client_idx].socket);
                bytes_sent = sendto(client_list[client_idx].socket, dgram_data, dgram_size,
                                    0, (struct sockaddr *)&(client_list[client_idx].dest_addr), sizeof(client_list[client_idx].dest_addr));
                if (bytes_sent == -1)
                {
                    //perror("Send error");
                }
                //printf("Sent %lli bytes.\n", (long long int) bytes_sent);
                break;
            }
        }
    }

    return ST_ERR_OK;
}

