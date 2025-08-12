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

int main(int argc, char *argv[])
{
    char *config_filename;
    uint16_t T4U_BASE_PORT = 10101;
    uint32_t client_chunks_allocated = 0;
    uint32_t num_clients = 0;
    struct Client_Addr *client_list = NULL;
    FILE *config_file;
    int32_t ret;

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
    
    ret = ST_ERR_OK;
    *client_list = NULL;
    
    curr_line = malloc(sizeof(char) * BASE_LEN);

    if (curr_line == NULL)
    {
        goto cleanup;
    }
    line_len = BASE_LEN;

    *client_list = malloc(sizeof(struct Client_Addr) * CLIENT_ADDR_BLOCKS);
    client_chunks_allocated++;
    if (*client_list == NULL)
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
        
        if (*num_clients >= (CLIENT_ADDR_BLOCKS-1))
        {
            printf("Error: no more than %i clients.\n", CLIENT_ADDR_BLOCKS);
            goto cleanup;
        }

        src_sock_addr.sin_addr.s_addr = htonl((src_addr[0] << 24)+(src_addr[1] << 16) + (src_addr[2] << 8) + src_addr[3]);
        dest_sock_addr.sin_port = dest_port;

        client_list[*num_clients]->t4u_addr = *((struct sockaddr *)&src_sock_addr);
        client_list[*num_clients]->dest_addr = *((struct sockaddr *)&dest_sock_addr);

        *num_clients += 1;
                
    }

    return ret;
    
cleanup:
    free(curr_line);
    curr_line = NULL;
    free(*client_list);
    *client_list = NULL;
    printf("Problem on line %u.\n", line_cnt);
    ret = ST_ERR_FAIL;
    return ret;
}
