#include <cassert>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include "network.hpp"
#include "game.hpp"

// Network code stuff for windows
global_var network_state_t *g_network_state;

void add_network_socket(network_socket_t *socket)
{
    socket->socket = g_network_state->sockets.socket_count++;
}

SOCKET *get_network_socket(network_socket_t *socket)
{
    return(&g_network_state->sockets.sockets[socket->socket]);
}

void initialize_network_socket(network_socket_t *socket_p, int32_t family, int32_t type, int32_t protocol)
{
    SOCKET new_socket = socket(family, type, protocol);

    if (new_socket == INVALID_SOCKET)
    {
        OutputDebugString("Failed to initialize socket\n");
        assert(0);
    }
    g_network_state->sockets.sockets[socket_p->socket] = new_socket;
}

void bind_network_socket_to_port(network_socket_t *socket, network_address_t address)
{
    SOCKET *sock = get_network_socket(socket);
    
    // Convert network_address_t to SOCKADDR_IN
    SOCKADDR_IN address_struct {};
    address_struct.sin_family = AF_INET;
    // Needs to be in network byte order
    address_struct.sin_port = address.port;
    address_struct.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(*sock, (SOCKADDR *)&address_struct, sizeof(address_struct)) == SOCKET_ERROR)
    {
        OutputDebugString("Failed to bind socket to local port");
        assert(0);
    }
}

void set_socket_to_non_blocking_mode(network_socket_t *socket)
{
    SOCKET *sock = get_network_socket(socket);
    u_long enabled = 1;
    ioctlsocket(*sock, FIONBIO, &enabled);
}

bool receive_from(network_socket_t *socket, char *buffer, uint32_t buffer_size, network_address_t *address_dst)
{
    SOCKET *sock = get_network_socket(socket);

    SOCKADDR_IN from_address = {};
    int32_t from_size = sizeof(from_address);
    
    int32_t bytes_received = recvfrom(*sock, buffer, buffer_size, 0, (SOCKADDR *)&from_address, &from_size);

    if (bytes_received == SOCKET_ERROR)
    {
        OutputDebugString("recvfrom failed\n");
    }
    else
    {
        buffer[bytes_received] = 0;
    }

    network_address_t received_address = {};
    received_address.port = from_address.sin_port;
    received_address.ipv4_address = from_address.sin_addr.S_un.S_addr;

    *address_dst = received_address;
    
    return(bytes_received > 0);
}

bool send_to(network_socket_t *socket, network_address_t address, char *buffer, uint32_t buffer_size)
{
    SOCKET *sock = get_network_socket(socket);
    
    SOCKADDR_IN address_struct = {};
    address_struct.sin_family = AF_INET;
    // Needs to be in network byte order
    address_struct.sin_port = address.port;
    address_struct.sin_addr.S_un.S_addr = address.ipv4_address;
    
    int32_t address_size = sizeof(address_struct);

    int32_t sendto_ret = sendto(*sock, buffer, buffer_size, 0, (SOCKADDR *)&address_struct, sizeof(address_struct));

    if (sendto_ret == SOCKET_ERROR)
    {
        char error_n[32];
        sprintf(error_n, "sendto failed: %d\n", WSAGetLastError());
        OutputDebugString(error_n);
        assert(0);
    }

    return(sendto_ret != SOCKET_ERROR);
}

uint32_t str_to_ipv4_int32(const char *address)
{
    return(inet_addr(address));
}

uint32_t host_to_network_byte_order(uint32_t bytes)
{
    return(htons(bytes));
}

uint32_t network_to_host_byte_order(uint32_t bytes)
{
    return(ntohl(bytes));
}

void initialize_socket_api(void)
{
    // This is only for Windows, make sure to change if using Linux / Mac
    WSADATA winsock_data;
    if (WSAStartup(0x202, &winsock_data))
    {
        OutputDebugString("Failed to initialize Winsock\n");
        assert(0);
    }
}

void initialize_as_client(void)
{
    add_network_socket(&g_network_state->main_network_socket);
    initialize_network_socket(&g_network_state->main_network_socket,
                              AF_INET,
                              SOCK_DGRAM,
                              IPPROTO_UDP);
     
    network_address_t address = {};
    address.port = host_to_network_byte_order(g_network_state->GAME_OUTPUT_PORT_CLIENT);
    bind_network_socket_to_port(&g_network_state->main_network_socket,
                                address);

     set_socket_to_non_blocking_mode(&g_network_state->main_network_socket);
    
    client_join_packet_t packet_to_send = {};
    packet_to_send.header.packet_mode = packet_header_t::packet_mode_t::CLIENT_MODE;
    packet_to_send.header.packet_type = packet_header_t::client_packet_type_t::CLIENT_JOIN;
    packet_to_send.header.total_packet_size = sizeof(packet_to_send);
    const char *message = "saska\0";
    memcpy(packet_to_send.client_name, message, strlen(message));

    network_address_t server_address { host_to_network_byte_order(g_network_state->GAME_OUTPUT_PORT_SERVER), str_to_ipv4_int32("127.0.0.1") };
    send_to(&g_network_state->main_network_socket,
            server_address,
            (char *)&packet_to_send,
            sizeof(packet_to_send));
}

void initialize_as_server(void)
{
     add_network_socket(&g_network_state->main_network_socket);
     initialize_network_socket(&g_network_state->main_network_socket,
                               AF_INET,
                               SOCK_DGRAM,
                               IPPROTO_UDP);
     
     network_address_t address = {};
     address.port = host_to_network_byte_order(g_network_state->GAME_OUTPUT_PORT_SERVER);
     bind_network_socket_to_port(&g_network_state->main_network_socket,
                                 address);

     set_socket_to_non_blocking_mode(&g_network_state->main_network_socket);
}

void initialize_network_translation_unit(struct game_memory_t *memory)
{
    g_network_state = &memory->network_state;
}

// Might have to be done on a separate thread just for updating world data
void update_as_server(void)
{
    network_address_t received_address = {};
    char buffer[100] = {};
    bool received = receive_from(&g_network_state->main_network_socket, buffer, sizeof(buffer), &received_address);

    if (received)
    {
        char *server_message = "server_speaking\0";
        send_to(&g_network_state->main_network_socket, received_address, server_message, strlen(server_message));
    }
}

void update_as_client(void)
{
    network_address_t received_address = {};
    char buffer[100] = {};
    bool received = receive_from(&g_network_state->main_network_socket, buffer, sizeof(buffer), &received_address);

    if (received)
    {
        OutputDebugString("handshake received\n");
    }
}

void update_network_state(void)
{
    switch(g_network_state->current_app_mode)
    {
    case network_state_t::application_mode_t::CLIENT_MODE: { update_as_client(); } break;
    case network_state_t::application_mode_t::SERVER_MODE: { update_as_server(); } break;
    }
}

void initialize_network_state(game_memory_t *memory, network_state_t::application_mode_t app_mode)
{
    initialize_socket_api();
    g_network_state->current_app_mode = app_mode;

    switch(g_network_state->current_app_mode)
    {
    case network_state_t::application_mode_t::CLIENT_MODE: { initialize_as_client(); } break;
    case network_state_t::application_mode_t::SERVER_MODE: { initialize_as_server(); } break;
    }
}
