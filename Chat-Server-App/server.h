#ifndef SERVER_H
#define SERVER_H

/* System headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* local */
#include "list.h"

#define PORT 8888
#define BACKLOG 5
#define DEFAULT_ROOM "Lobby"
#define MAXBUFF 2048

/* Reader/Writer globals (declared in server.c) */
extern int numReaders;
extern pthread_mutex_t mutex;     // protects numReaders
extern pthread_mutex_t rw_lock;   // writer lock

/* reader lock helpers (defined in server.c) */
void reader_lock(void);
void reader_unlock(void);

/* Server MOTD */
extern const char *server_MOTD;

/* Global heads */
extern UserNode *user_head;
extern RoomNode *room_head;

/* Core functions */
int get_server_socket(void);
int start_server(int serv_socket, int backlog);
int accept_client(int serv_sock);
void sigintHandler(int sig_num);
void *client_receive(void *ptr);

/* Safe operations exposed to client thread */
void addRoomSafe(const char *roomname);
void addUserSafe(int socket, const char *username);
void addUserToRoomSafe(const char *username, const char *roomname);
void removeUserFromRoomSafe(const char *username, const char *roomname);
void removeAllUserConnectionsSafe(const char *username);
void removeUserSafe(int socket);
void renameUserSafe(int socket, const char *newName);
void listAllRooms(int client_socket);
void listAllUsers(int client_socket, int requester_socket);

#endif // SERVER_H
