#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/////////////////// USER LIST //////////////////////////
UserNode* insertFirstUser(UserNode *head, int socket, const char *username) {
    if(findUserByNameU(head, username)) return head; // prevent duplicate

    UserNode *newUser = (UserNode *)malloc(sizeof(UserNode));
    newUser->socket = socket;
    strncpy(newUser->username, username, MAX_NAME_LEN);
    newUser->rooms = NULL;
    newUser->directConns = NULL;
    newUser->next = head;
    return newUser;
}

UserNode* findUserByNameU(UserNode *head, const char *username) {
    UserNode *cur = head;
    while(cur) {
        if(strcmp(cur->username, username) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

UserNode* findUserBySocketU(UserNode *head, int socket) {
    UserNode *cur = head;
    while(cur) {
        if(cur->socket == socket) return cur;
        cur = cur->next;
    }
    return NULL;
}

void removeUserU(UserNode **head, int socket) {
    UserNode *prev = NULL, *cur = *head;
    while(cur) {
        if(cur->socket == socket) {
            removeAllRoomsFromUserU(cur);
            // free direct connections
            DirectConnNode *dc = cur->directConns;
            while(dc) {
                DirectConnNode *tmp = dc;
                dc = dc->next;
                free(tmp);
            }
            if(prev) prev->next = cur->next;
            else *head = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void freeAllUsersU(UserNode **head) {
    UserNode *cur = *head;
    while(cur) {
        removeAllRoomsFromUserU(cur);
        DirectConnNode *dc = cur->directConns;
        while(dc) {
            DirectConnNode *tmp = dc;
            dc = dc->next;
            free(tmp);
        }
        UserNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    *head = NULL;
}

void addRoomToUserU(UserNode *user, const char *roomname) {
    RoomListNode *cur = user->rooms;
    while(cur) {
        if(strcmp(cur->roomName, roomname) == 0) return; // already added
        cur = cur->next;
    }
    RoomListNode *newNode = (RoomListNode *)malloc(sizeof(RoomListNode));
    strncpy(newNode->roomName, roomname, MAX_NAME_LEN);
    newNode->next = user->rooms;
    user->rooms = newNode;
}

void removeRoomFromUserU(UserNode *user, const char *roomname) {
    RoomListNode *prev = NULL, *cur = user->rooms;
    while(cur) {
        if(strcmp(cur->roomName, roomname) == 0) {
            if(prev) prev->next = cur->next;
            else user->rooms = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void removeAllRoomsFromUserU(UserNode *user) {
    RoomListNode *cur = user->rooms;
    while(cur) {
        RoomListNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    user->rooms = NULL;
}

void addDirectConnU(UserNode *user, const char *toUser) {
    DirectConnNode *cur = user->directConns;
    while(cur) {
        if(strcmp(cur->username, toUser) == 0) return; // already connected
        cur = cur->next;
    }
    DirectConnNode *newNode = (DirectConnNode *)malloc(sizeof(DirectConnNode));
    strncpy(newNode->username, toUser, MAX_NAME_LEN);
    newNode->next = user->directConns;
    user->directConns = newNode;
}

void removeDirectConnU(UserNode *user, const char *toUser) {
    DirectConnNode *prev = NULL, *cur = user->directConns;
    while(cur) {
        if(strcmp(cur->username, toUser) == 0) {
            if(prev) prev->next = cur->next;
            else user->directConns = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

/////////////////// ROOM LIST //////////////////////////
RoomNode* insertFirstRoom(RoomNode *head, const char *roomname) {
    if(findRoomByNameR(head, roomname)) return head; // prevent duplicate
    RoomNode *newRoom = (RoomNode *)malloc(sizeof(RoomNode));
    strncpy(newRoom->name, roomname, MAX_NAME_LEN);
    newRoom->users = NULL;
    newRoom->next = head;
    return newRoom;
}

RoomNode* findRoomByNameR(RoomNode *head, const char *roomname) {
    RoomNode *cur = head;
    while(cur) {
        if(strcmp(cur->name, roomname) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

void addUserToRoomR(RoomNode *head, const char *username, const char *roomname) {
    RoomNode *room = findRoomByNameR(head, roomname);
    if(!room) return;
    RoomUserNode *cur = room->users;
    while(cur) {
        if(strcmp(cur->username, username) == 0) return; // already exists
        cur = cur->next;
    }
    RoomUserNode *newUser = (RoomUserNode *)malloc(sizeof(RoomUserNode));
    strncpy(newUser->username, username, MAX_NAME_LEN);
    newUser->next = room->users;
    room->users = newUser;
}

void removeUserFromRoomR(RoomNode *head, const char *username, const char *roomname) {
    RoomNode *room = findRoomByNameR(head, roomname);
    if(!room) return;
    RoomUserNode *prev = NULL, *cur = room->users;
    while(cur) {
        if(strcmp(cur->username, username) == 0) {
            if(prev) prev->next = cur->next;
            else room->users = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void freeAllRoomsR(RoomNode **head) {
    RoomNode *cur = *head;
    while(cur) {
        RoomUserNode *ru = cur->users;
        while(ru) {
            RoomUserNode *tmp = ru;
            ru = ru->next;
            free(tmp);
        }
        RoomNode *tmpR = cur;
        cur = cur->next;
        free(tmpR);
    }
    *head = NULL;
}
