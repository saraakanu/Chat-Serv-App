#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_NAME_LEN 50

/////////////////// USER LIST //////////////////////////
typedef struct UserNode {
    char username[MAX_NAME_LEN];
    int socket;
    struct RoomListNode *rooms;
    struct DirectConnNode *directConns;
    struct UserNode *next;
} UserNode;

typedef struct RoomListNode {
    char roomName[MAX_NAME_LEN];
    struct RoomListNode *next;
} RoomListNode;

typedef struct DirectConnNode {
    char username[MAX_NAME_LEN];
    struct DirectConnNode *next;
} DirectConnNode;

/////////////////// ROOM LIST //////////////////////////
typedef struct RoomNode {
    char name[MAX_NAME_LEN];
    struct RoomUserNode *users;
    struct RoomNode *next;
} RoomNode;

typedef struct RoomUserNode {
    char username[MAX_NAME_LEN];
    struct RoomUserNode *next;
} RoomUserNode;

/////////////////// USER FUNCTIONS //////////////////////////
UserNode* insertFirstUser(UserNode *head, int socket, const char *username);
UserNode* findUserByNameU(UserNode *head, const char *username);
UserNode* findUserBySocketU(UserNode *head, int socket);
void removeUserU(UserNode **head, int socket);
void freeAllUsersU(UserNode **head);

void addRoomToUserU(UserNode *user, const char *roomname);
void removeRoomFromUserU(UserNode *user, const char *roomname);
void removeAllRoomsFromUserU(UserNode *user);

void addDirectConnU(UserNode *user, const char *toUser);
void removeDirectConnU(UserNode *user, const char *toUser);

/////////////////// ROOM FUNCTIONS //////////////////////////
RoomNode* insertFirstRoom(RoomNode *head, const char *roomname);
RoomNode* findRoomByNameR(RoomNode *head, const char *roomname);
void removeUserFromRoomR(RoomNode *room_head, const char *username, const char *roomname);
void addUserToRoomR(RoomNode *room_head, const char *username, const char *roomname);
void freeAllRoomsR(RoomNode **head);

#endif
