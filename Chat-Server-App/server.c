#include "server.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* globals */
int numReaders = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rw_lock = PTHREAD_MUTEX_INITIALIZER;

UserNode *user_head = NULL;
RoomNode *room_head = NULL;

const char *server_MOTD = "Thanks for connecting to BisonChat Server.\n\nchat>";

/* reader/writer helpers (exported) */
void reader_lock(void) {
    pthread_mutex_lock(&mutex);
    numReaders++;
    if (numReaders == 1) pthread_mutex_lock(&rw_lock);
    pthread_mutex_unlock(&mutex);
}
void reader_unlock(void) {
    pthread_mutex_lock(&mutex);
    numReaders--;
    if (numReaders == 0) pthread_mutex_unlock(&rw_lock);
    pthread_mutex_unlock(&mutex);
}

/* socket helpers */
int get_server_socket(void) {
    int opt = 1;
    int master_socket;
    struct sockaddr_in address;
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(master_socket);
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(master_socket);
        return -1;
    }
    return master_socket;
}

int start_server(int serv_socket, int backlog) {
    return listen(serv_socket, backlog);
}

int accept_client(int serv_sock) {
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    struct sockaddr_storage addr;
    return accept(serv_sock, (struct sockaddr *)&addr, &addrlen);
}

/* safe list ops */
/* add room (writer) */
void addRoomSafe(const char *roomname) {
    if (!roomname) return;
    pthread_mutex_lock(&rw_lock);
    room_head = insertFirstRoom(room_head, roomname);
    pthread_mutex_unlock(&rw_lock);
}

/* add user and join default room (writer) */
void addUserSafe(int socket, const char *username) {
    if (!username) return;
    pthread_mutex_lock(&rw_lock);
    user_head = insertFirstUser(user_head, socket, username);
    room_head = insertFirstRoom(room_head, DEFAULT_ROOM);
    addUserToRoomR(room_head, username, DEFAULT_ROOM);
    UserNode *u = findUserByNameU(user_head, username);
    if (u) addRoomToUserU(u, DEFAULT_ROOM);
    pthread_mutex_unlock(&rw_lock);
}

/* add user to room - both directions (writer) */
void addUserToRoomSafe(const char *username, const char *roomname) {
    if (!username || !roomname) return;
    pthread_mutex_lock(&rw_lock);
    room_head = insertFirstRoom(room_head, roomname);
    addUserToRoomR(room_head, username, roomname);
    UserNode *u = findUserByNameU(user_head, username);
    if (u) addRoomToUserU(u, roomname);
    pthread_mutex_unlock(&rw_lock);
}

/* remove user->room and room->user (writer) */
void removeUserFromRoomSafe(const char *username, const char *roomname) {
    if (!username || !roomname) return;
    pthread_mutex_lock(&rw_lock);
    removeUserFromRoomR(room_head, username, roomname);
    UserNode *u = findUserByNameU(user_head, username);
    if (u) removeRoomFromUserU(u, roomname);
    pthread_mutex_unlock(&rw_lock);
}

/* remove all DCs and memberships (writer) */
void removeAllUserConnectionsSafe(const char *username) {
    if (!username) return;
    pthread_mutex_lock(&rw_lock);
    UserNode *u = findUserByNameU(user_head, username);
    if (u) {
        DirectConnNode *dc = u->directConns;
        while (dc) {
            DirectConnNode *tmp = dc;
            dc = dc->next;
            free(tmp);
        }
        u->directConns = NULL;
        RoomListNode *rln = u->rooms;
        while (rln) {
            removeUserFromRoomR(room_head, username, rln->roomName);
            rln = rln->next;
        }
        removeAllRoomsFromUserU(u);
    }
    pthread_mutex_unlock(&rw_lock);
}

/* remove user by socket (writer) */
void removeUserSafe(int socket) {
    pthread_mutex_lock(&rw_lock);
    removeUserU(&user_head, socket);
    pthread_mutex_unlock(&rw_lock);
}

/* rename user safely and update all references (writer) */
void renameUserSafe(int socket, const char *newName) {
    if (!newName || strlen(newName) == 0) return;
    pthread_mutex_lock(&rw_lock);

    UserNode *u = findUserBySocketU(user_head, socket);
    if (!u) { pthread_mutex_unlock(&rw_lock); return; }

    if (findUserByNameU(user_head, newName)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Username '%s' is already taken.\nchat>", newName);
        send(socket, msg, strlen(msg), 0);
        pthread_mutex_unlock(&rw_lock);
        return;
    }

    char oldName[MAX_NAME_LEN];
    strncpy(oldName, u->username, MAX_NAME_LEN-1);
    oldName[MAX_NAME_LEN-1] = '\0';

    strncpy(u->username, newName, MAX_NAME_LEN-1);
    u->username[MAX_NAME_LEN-1] = '\0';

    /* update room members */
    RoomNode *r = room_head;
    while (r) {
        RoomUserNode *ru = r->users;
        while (ru) {
            if (strcmp(ru->username, oldName) == 0) {
                strncpy(ru->username, newName, MAX_NAME_LEN-1);
                ru->username[MAX_NAME_LEN-1] = '\0';
            }
            ru = ru->next;
        }
        r = r->next;
    }

    /* update direct connections across all users */
    UserNode *it = user_head;
    while (it) {
        DirectConnNode *dc = it->directConns;
        while (dc) {
            if (strcmp(dc->username, oldName) == 0) {
                strncpy(dc->username, newName, MAX_NAME_LEN-1);
                dc->username[MAX_NAME_LEN-1] = '\0';
            }
            dc = dc->next;
        }
        it = it->next;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Logged in as '%s'.\nchat>", newName);
    send(socket, msg, strlen(msg), 0);

    pthread_mutex_unlock(&rw_lock);
}

/* list functions (reader) */
void listAllRooms(int client_socket) {
    char buffer[256];
    reader_lock();
    snprintf(buffer, sizeof(buffer), "Rooms list:\n");
    send(client_socket, buffer, strlen(buffer), 0);
    RoomNode *cur = room_head;
    while (cur) {
        snprintf(buffer, sizeof(buffer), "%s\n", cur->name);
        send(client_socket, buffer, strlen(buffer), 0);
        cur = cur->next;
    }
    reader_unlock();
    send(client_socket, "chat>", 5, 0);
}

void listAllUsers(int client_socket, int requester_socket) {
    char buffer[256];
    reader_lock();
    snprintf(buffer, sizeof(buffer), "Users list:\n");
    send(client_socket, buffer, strlen(buffer), 0);
    UserNode *cur = user_head;
    while (cur) {
        snprintf(buffer, sizeof(buffer), "%s\n", cur->username);
        send(client_socket, buffer, strlen(buffer), 0);
        cur = cur->next;
    }
    reader_unlock();
    send(client_socket, "chat>", 5, 0);
}

/* SIGINT cleanup */
void sigintHandler(int sig_num) {
    (void)sig_num;
    fprintf(stderr, "\nServer shutting down...\n");
    pthread_mutex_lock(&rw_lock);
    UserNode *u = user_head;
    while (u) {
        close(u->socket);
        u = u->next;
    }
    freeAllUsersU(&user_head);
    freeAllRoomsR(&room_head);
    pthread_mutex_unlock(&rw_lock);
    close(get_server_socket()); /* optional: close server socket; safe */
    fprintf(stderr, "All resources freed. Exiting.\n");
    exit(0);
}
