#include "hashmap.h"
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/poll.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8090
#define MAXLEN (32 << 20)
#define INCOMING_SIZE (MAXLEN + sizeof(uint32_t))
#define OUTGOING_SIZE (INCOMING_SIZE + 100)

typedef struct Conn{
    int fd;
    char *incoming;
    uint32_t incoming_len;
    char *outgoing;
    uint32_t outgoing_len;
    bool want_read; 
    bool want_write;
    bool want_close;
} Conn;

typedef struct serverState{
    Conn** fd2Conn;
    int home_fd;
    struct pollfd* fds_polling_arg;
    int pollArgsLen;
    int maxFd;
    int fds;
    HMap *HashDB;
} serverState;

typedef enum Response_status {
    RES_OK = 0,
    RES_ERROR,
    RES_KEY_NOT_FOUND
} Response_status;

typedef struct Response {
    Response_status status;
    char *message;
} Response;

serverState State;

void init(){
    State.fds = 0;
    State.home_fd = -1;
    State.fd2Conn = NULL;
    State.fds_polling_arg = NULL;
    State.pollArgsLen = 0;
    State.HashDB = init_hashmap();
}

void die(const char *msg){
    perror(msg);
    exit(1);
}

// KEY VALUE STORE
void set_kv(const char *key, const char *value) {
    Entry *e = h_lookup(State.HashDB, key, 0);

    // If entry is already in the hashmap modify
    if ( e != NULL) {
        free(e->value);
        e->value = strdup(value);
    }

    Entry *entry = malloc(sizeof(Entry));
    entry->key = strdup(key);
    entry->value = strdup(value);
    insert_entry(entry, State.HashDB);

    float load_factor = current_load_factor(State.HashDB);
    State.HashDB->newer->load_factor = load_factor;

    if (load_factor > MAX_LOAD_FACTOR) {
        printf("Triggering resize\n");
        trigger_resize(State.HashDB);
    }
    State.HashDB->entries++;
}

char *get_kv(const char *k) {
    Entry *e = h_lookup(State.HashDB, k, 0);
    if (e) return e->value;
    return NULL;
}

int del_kv(const char *key) {
    const uint64_t hash = str_hash((uint8_t*)key, strlen(key));

    HTab *db = State.HashDB->newer;
    Entry *e = htab_lookup(db, key, hash);

    if (State.HashDB->older && e == NULL) {
        db = State.HashDB->older;
        e = htab_lookup(db, key, hash);
    }

    if (e == NULL) {
        printf("Entry not found\n");
        return 0;
    }
    detach_node(&e->node, db);
    free(e->key);
    free(e->value);
    free(e);
    State.HashDB->entries--;
    return 1;
}

static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}

int read_32bit(uint32_t *n, char *buf) {
    memcpy(n, buf, sizeof(uint32_t));
    return sizeof(uint32_t);
}

void feed_outgoing(Conn *conn, char *msg, int n){
    int total_bytes = sizeof(uint32_t) + n;
    if (conn->outgoing_len + total_bytes > OUTGOING_SIZE) {
        printf("Buffer overflow\n");
        return;
    }
    memcpy(conn->outgoing + conn->outgoing_len, &n, sizeof(uint32_t));
    memcpy(conn->outgoing + sizeof(uint32_t) + conn->outgoing_len, msg, n);
    conn->outgoing_len += total_bytes;
}
int deserialize_params(uint32_t len, char **request, int *params_len, char ***params) {
    // this will be incremented
    int total_bytes = 0;

    char *ptr = *request;
    uint32_t n;
    if (len <= total_bytes + sizeof(uint32_t) ) {
        return 0;
    }
    ptr += read_32bit(&n, ptr);
    total_bytes += sizeof(uint32_t);
    *params_len = n;
    *params = malloc(n * sizeof(char*));

    for (uint32_t i = 0; i < n; i++) {
        if (len <= total_bytes + sizeof(uint32_t)) {
            return 0;
        }
        uint32_t param_len;
        ptr += read_32bit(&param_len, ptr);
        total_bytes += sizeof(uint32_t);
        if (len < total_bytes + param_len) {
            return 0;
        }
        char *param = malloc(param_len + 1);
        memcpy(param, ptr, param_len);
        param[param_len] = '\0';
        (*params)[i] = param;
        ptr += param_len;
        total_bytes += param_len;
    }
    return 1;
}

int perform_request(int params_len, char **params, Response *response){
    h_rehash(State.HashDB);
    if ( params_len == 3 && strcmp(params[0], "set") == 0) {
        set_kv(params[1], params[2]);
        response->status = RES_OK;
        response->message = strdup("OK");
        return 1;
    }
    else if ( params_len == 2 && strcmp(params[0], "get") == 0) {
        char *val = get_kv(params[1]);
        if (val != NULL) {
            response->status = RES_OK;
            response->message = strdup(val);
            return 1;
        } 
        response->status = RES_KEY_NOT_FOUND;
        response->message = strdup("Key not found");
        return 1;
    }
    else if ( params_len == 2 && strcmp(params[0], "del") == 0) {
        if (del_kv(params[1])) {
            response->status = RES_OK;
            response->message = strdup("OK");
            return 1;
        }
        response->status = RES_KEY_NOT_FOUND;
        response->message = strdup("Key not found");
        return 1;
    }
    response->status = RES_ERROR;
    response->message = strdup("Unknown command");
    return 0;
}

int feed_response(Conn *conn, Response *response){
    int total_bytes = sizeof(uint32_t) + strlen(response->message);
    char *resp = malloc(total_bytes);
    memcpy(resp, &response->status, sizeof(uint32_t));
    memcpy(resp + sizeof(uint32_t), response->message, strlen(response->message));
    feed_outgoing(conn, resp, total_bytes);
    return 1;
}

bool parse_one_request(Conn *conn){
    // getting header ( length of the request )
    uint32_t len;
    if (conn->incoming_len <= sizeof(uint32_t)) {
        return false;
    }
    read_32bit(&len, conn->incoming);
    if (len > MAXLEN) {
        printf("Request too long\n");
        conn->want_close = true;
        return false;
    }
    if (!len) {
        return false;
    }
    if (conn->incoming_len < sizeof(uint32_t) + len) {
        return false;
    }

    // getting the request
    char *request = malloc(len);
    memcpy(request, &conn->incoming[sizeof(uint32_t)], len);
    int total_len = len + sizeof(uint32_t);

    char **params = NULL;
    int params_len = 0;
    if(!deserialize_params(len, &request, &params_len, &params)){
        printf("somethinq went wrong\n");
        return false;
    }
    Response *response = malloc(sizeof(Response));
    perform_request(params_len, params, response);

    // wrting the response to client's connection's outgoing buffer
    feed_response(conn, response);

    // flushing the current request from incoming buffer
    memmove(conn->incoming, conn->incoming + total_len, conn->incoming_len - total_len);
    conn->incoming_len -= total_len;

    free(request);
    return true;
}

void handle_accept(int fd){
    struct sockaddr_storage client_addr;
    socklen_t len = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &len);
    printf("connection on fd: %i\n", connfd);
    if (connfd < 0) {
        die("accept() error");
        return;
    }
    fd_set_nb(connfd);
    Conn *newConn = malloc(sizeof(Conn));
    newConn->fd = connfd;
    newConn->want_read = true;
    newConn->outgoing_len = 0;
    newConn->incoming = malloc(MAXLEN + sizeof(uint32_t));
    newConn->outgoing = malloc(MAXLEN+ sizeof(uint32_t) + 100);
    newConn->incoming_len = 0;
    newConn->want_write = false;
    newConn->want_close = false;
    if(connfd > State.maxFd){
        State.maxFd = connfd;
        State.fds++;
        State.fd2Conn = realloc(State.fd2Conn, sizeof(Conn*)*(State.maxFd+1));
    }
    else if(State.fd2Conn[connfd] != NULL){
        free(State.fd2Conn[connfd]);
    }
    State.fd2Conn[connfd] = newConn;
}

void handle_write(Conn *conn){
    ssize_t rv = write(conn->fd, conn->outgoing, conn->outgoing_len);
    if (rv < 0) {
        if (errno == EAGAIN) {
            return; // actually not ready
        }
        die("write() error");
    }
    if (rv == 0) {
        conn->want_close = true;
        return;
    }
    memmove(conn->outgoing, conn->outgoing + rv, conn->outgoing_len - rv);
    if(rv == conn->outgoing_len){
        conn->want_read = true; 
        conn->want_write = false;
    }
    conn->outgoing_len -= rv;
}

void process_requests(Conn *conn){
    while(parse_one_request(conn)); // pipeline
    if (conn->outgoing_len > 0) {
        conn->want_read = false;
        conn->want_write = true;
        handle_write(conn);
    }
}

void handle_read(Conn *conn){
    ssize_t rv = read(conn->fd, conn->incoming + conn->incoming_len, INCOMING_SIZE - (conn->incoming_len) - 1);
    printf("read %li\n", rv);
    if (rv < 0) {
        if (errno == EAGAIN) {
            return; // actually not ready
        }
        die("read() error");
    }

    if (rv == 0) {
        printf("client closed %i \n", conn->fd);
        conn->want_close = true;
        return; 
    }
    conn->incoming_len += rv;
    conn->incoming[conn->incoming_len] = '\0';
    process_requests(conn);
}


int main(){
    init();
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    printf("home_socket: %i\n", fd);
    fd_set_nb(fd);

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        die("bind()");
    }
    ret = listen(fd, SOMAXCONN);
    if (ret < 0) {
        die("listen()");
    }
    State.home_fd = fd;
    State.maxFd = fd;
    State.fd2Conn = calloc((State.maxFd+1), sizeof(Conn*));
    while(true){
        if(State.fds_polling_arg == NULL){
            State.fds_polling_arg = malloc(sizeof(struct pollfd));
        }
        int new_size = State.fds + 1;
        if(State.pollArgsLen <= new_size){
            State.fds_polling_arg = realloc(State.fds_polling_arg, sizeof(struct pollfd)*(new_size));
        }

        struct pollfd newPollArg;
        newPollArg.fd = fd;
        newPollArg.events = POLLERR | POLLIN;
        State.fds_polling_arg[0] = newPollArg;
        State.pollArgsLen = 1;

        for (int i = 0; i < State.maxFd+1; i++) {
            Conn *conn = State.fd2Conn[i];
            if(!conn){
                continue;
            }
            if(conn->want_close){
                continue;
            }
            struct pollfd newPollArg;
            newPollArg.fd = conn->fd;
            newPollArg.events = POLLERR;
            if(conn->want_read){
                newPollArg.events = newPollArg.events | POLLIN;
            }
            if(conn->want_write){
                newPollArg.events = newPollArg.events | POLLOUT;
            }
            State.fds_polling_arg[State.pollArgsLen] = newPollArg;
            State.pollArgsLen++;
        }

        int rv = poll(State.fds_polling_arg, State.pollArgsLen, -1);
        if (rv < 0 ) {
            if (errno == EINTR) {
                continue;   // sys call interrupted by signal
            }
            die("poll");
        }
        struct pollfd home_socket_arg = State.fds_polling_arg[0];
        if (home_socket_arg.revents) {
            handle_accept(home_socket_arg.fd);
        }

        for (int i = 1; i < State.pollArgsLen; i++) {
            struct pollfd listening_socket = State.fds_polling_arg[i];
            Conn *conn = State.fd2Conn[listening_socket.fd];
            uint8_t ready = listening_socket.revents;
            if (conn->want_close){
                State.fds--;
                close(conn->fd);
                State.fd2Conn[conn->fd] = NULL;
                free(conn->incoming);
                free(conn->outgoing);
                free(conn);
                continue;
            }
            if (ready & POLLIN) {
                handle_read(conn);
            }
            if (ready & POLLOUT) {
                handle_write(conn);
            }
        }
    }
}
