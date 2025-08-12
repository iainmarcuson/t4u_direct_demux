// t4u_demux.h
// Provides definitions for the T4U Demultiplexor
// Copyright (C) 2025 Sydor Technologies

#ifndef T4U_DEMUX_H // Inclusion guard
#define T4U_DEMUX_H

#include <sys/socket.h>

#define CLIENT_ADDR_BLOCKS 16

// Error codes
#define ST_ERR_OK 0
#define ST_ERR_BASE -10000;
#define ST_ERR_FAIL -10001;

struct Client_Addr
{
    struct sockaddr_in t4u_addr; // T4U Address; only IP address in significant
    struct sockaddr_in dest_addr; // Destination address
    int socket;
};

#endif
