#include "server.h"
#include "list.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

extern int numReaders;
extern pthread_mutex_t mutex;
extern pthread_mutex_t rw_lock;
extern UserNode *user_head;
extern RoomNode *room_head;
extern const char *server_MOTD;

#define DEFAULT_ROOM "Lobby"
#define MAX_ARGS 80

static char *trimwhitespace(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/* Broadcast: collect recipients under reader lock then send outside lock */
static void broadcastMessage(UserNode *sender, const char *message) {
    if (!sender || !message) return;

    int cap = 16, count = 0;
    int *socks = malloc(sizeof(int) * cap);
    if (!socks) return;

    reader_lock();
    UserNode *cur = user_head;
    while (cur) {
        if (cur->socket != sender->socket) {
            int sendFlag = 0;
            RoomListNode *rln = sender->rooms;
            while (rln && !sendFlag) {
                if (findRoomByNameR(room_head, rln->roomName)) {
                    if (userIsInRoomUnlocked(cur->username, rln->roomName)) sendFlag = 1;
                }
                rln = rln->next;
            }
            DirectConnNode *dc = sender->directConns;
            while (dc && !sendFlag) {
                if (strcmp(dc->username, cur->username) == 0) sendFlag = 1;
                dc = dc->next;
            }
            if (sendFlag) {
                if (count >= cap) {
                    cap *= 2;
                    int *tmp = realloc(socks, sizeof(int) * cap);
                    if (!tmp) break;
                    socks = tmp;
                }
                socks[count++] = cur->socket;
            }
        }
        cur = cur->next;
    }
    reader_unlock();

    for (int i = 0; i < count; ++i) {
        send(socks[i], message, strlen(message), 0);
    }
    free(socks);
}

/* Utility used above: check membership without locking (caller must hold lock) */
static int userIsInRoomUnlocked(const char *username, const char *roomname) {
    RoomNode *r = findRoomByNameR(room_head, roomname);
    if (!r) return 0;
    RoomUserNode *ru = r->users;
    while (ru) {
        if (strcmp(ru->username, username) == 0) return 1;
        ru = ru->next;
    }
    return 0;
}

void *client_receive(void *ptr) {
    int client = *(int *)ptr;
    int received;
    char buffer[MAXBUFF], sbuffer[MAXBUFF];
    char tmpbuf[MAXBUFF], cmd[MAXBUFF], username[MAX_NAME_LEN];
    char *arguments[MAX_ARGS];
    const char *delimiters = " \t\n\r";

    send(client, server_MOTD, strlen(server_MOTD), 0);

    snprintf(username, sizeof(username), "guest%d", client);

    addUserSafe(client, username);

    while (1) {
        if ((received = read(client, buffer, MAXBUFF)) > 0) {
            buffer[received] = '\0';
            strncpy(cmd, buffer, sizeof(cmd)-1); cmd[sizeof(cmd)-1] = '\0';
            strncpy(sbuffer, buffer, sizeof(sbuffer)-1); sbuffer[sizeof(sbuffer)-1] = '\0';

            arguments[0] = strtok(cmd, delimiters);
            int i = 0;
            while (arguments[i] != NULL && i < MAX_ARGS-1) {
                arguments[++i] = strtok(NULL, delimiters);
                if (arguments[i]) arguments[i] = trimwhitespace(arguments[i]);
            }

            if (!arguments[0]) { send(client, "\nchat>", 6, 0); continue; }

            if (strcmp(arguments[0], "create") == 0 && arguments[1]) {
                addRoomSafe(arguments[1]);
                snprintf(buffer, sizeof(buffer), "Room '%s' created.\nchat>", arguments[1]);
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "join") == 0 && arguments[1]) {
                UserNode *u = findUserBySocketU(user_head, client);
                RoomNode *r = findRoomByNameR(room_head, arguments[1]);
                if (u && r) {
                    addUserToRoomSafe(u->username, arguments[1]);
                    snprintf(buffer, sizeof(buffer), "Joined room '%s'.\nchat>", arguments[1]);
                } else snprintf(buffer, sizeof(buffer), "Room '%s' does not exist.\nchat>", arguments[1]);
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "leave") == 0 && arguments[1]) {
                UserNode *u = findUserBySocketU(user_head, client);
                if (u) {
                    removeUserFromRoomSafe(u->username, arguments[1]);
                    snprintf(buffer, sizeof(buffer), "Left room '%s'.\nchat>", arguments[1]);
                } else snprintf(buffer, sizeof(buffer), "User not found.\nchat>");
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "connect") == 0 && arguments[1]) {
                UserNode *u = findUserBySocketU(user_head, client);
                UserNode *target = findUserByNameU(user_head, arguments[1]);
                if (u && target) {
                    pthread_mutex_lock(&rw_lock);
                    addDirectConnU(u, target->username);
                    pthread_mutex_unlock(&rw_lock);
                    snprintf(buffer, sizeof(buffer), "Connected (DM) with '%s'.\nchat>", target->username);
                } else snprintf(buffer, sizeof(buffer), "User '%s' not found.\nchat>", arguments[1]);
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "disconnect") == 0 && arguments[1]) {
                UserNode *u = findUserBySocketU(user_head, client);
                if (u) {
                    pthread_mutex_lock(&rw_lock);
                    removeDirectConnU(u, arguments[1]);
                    pthread_mutex_unlock(&rw_lock);
                    snprintf(buffer, sizeof(buffer), "Disconnected from '%s'.\nchat>", arguments[1]);
                } else snprintf(buffer, sizeof(buffer), "User not found.\nchat>");
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "rooms") == 0) {
                listAllRooms(client);
            }
            else if (strcmp(arguments[0], "users") == 0) {
                listAllUsers(client, client);
            }
            else if (strcmp(arguments[0], "login") == 0 && arguments[1]) {
                renameUserSafe(client, arguments[1]);
            }
            else if (strcmp(arguments[0], "help") == 0) {
                snprintf(buffer, sizeof(buffer), "Commands:\nlogin <username>\ncreate <room>\njoin <room>\nleave <room>\nusers\nrooms\nconnect <user>\ndisconnect <user>\nexit\n");
                send(client, buffer, strlen(buffer), 0);
            }
            else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0) {
                UserNode *u = findUserBySocketU(user_head, client);
                if (u) {
                    removeAllUserConnectionsSafe(u->username);
                    removeUserSafe(client);
                }
                close(client);
                pthread_exit(NULL);
            }
            else {
                UserNode *sender = findUserBySocketU(user_head, client);
                if (sender) {
                    snprintf(tmpbuf, sizeof(tmpbuf), "\n::%s> %s\nchat>", sender->username, sbuffer);
                    broadcastMessage(sender, tmpbuf);
                }
            }
            memset(buffer, 0, sizeof(buffer));
        }
    }
    return NULL;
}
