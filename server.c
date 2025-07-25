#include "zset.h"
#include "treedump.h"
#include "server.h"
#include "common.h"
#include "hashmap.h"
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>    
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
#define res_arr_element_len 100 
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
    RES_ERROR
} Response_status;

typedef enum Tag {
    TAG_NIL = 0,
    TAG_INT,
    TAG_STR,
    TAG_ARR,
    TAG_DOUBLE
} Tag;


typedef struct Response {
    Response_status status;
    void *message;
    int response_len;
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

char *response_enum(int status){
    switch(status){
        case RES_OK:
            return "OK";
        case RES_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN/INVALID COMMAND";
    }
}
bool str2dbl(const char *s, double *out) {
    char *endp = NULL;
    *out = strtod(s, &endp);
    return endp == s + strlen(s) && !isnan(*out);
}

bool eq_entry(const char* key, HNode *node){
    return strcmp(hnode_to_entry(node)->key, key) == 0;
}

// takes HNode and prints out (key, value) for entry
void print_val_entry(HNode* n){
    Entry *entry = hnode_to_entry(n);
    printf("> %s, %s\n", entry->key, entry->value);
}

// KEY VALUE STORE
void set_kv(const char *key, const char *value) {
    HNode *n = h_lookup(State.HashDB, key, eq_entry);
    // If entry is already in the hashmap modify
    if ( n != NULL) {
        Entry *e = hnode_to_entry(n);
        free(e->value);
        e->value = strdup(value);
    }
    else { 
        Entry *entry = malloc(sizeof(Entry));
        entry->key = strdup(key);
        entry->type = K_V_PAIR;
        entry->value = strdup(value);
        entry->node.hash = str_hash((uint8_t*) key, strlen(key));
        insert_node(&entry->node, State.HashDB);

        possibly_resize(State.HashDB);
        State.HashDB->entries++;
    }
    scan_map(State.HashDB, print_val_entry);
    h_rehash(State.HashDB);
}

char *get_kv(const char *k) {
    HNode *n = h_lookup(State.HashDB, k, eq_entry);
    if (n) return hnode_to_entry(n)->value;
    return NULL;
}

int del_kv(const char *key) {
    HTab *db = State.HashDB->newer;
    HNode *n = htab_lookup(db, key, eq_entry);

    if (State.HashDB->older && n == NULL) {
        db = State.HashDB->older;
        n = htab_lookup(db, key, eq_entry);
    }

    if (n == NULL) {
        printf("Entry not found\n");
        return 0;
    }
    Entry *e = hnode_to_entry(n);
    detach_node(n, db);
    free(e->key);
    free(e->value);
    free(e);
    State.HashDB->entries--;
    return 1;
}

int Z_add(char* params[4]){

    char *endp = NULL;
    char *key = params[1];
    char *name = params[2];
    double score;

    Entry* entry;
    if(!str2dbl(params[3], &score)){
        return 0;
    };
    // check if "leaderboard" exist in global hashmap;
    HNode *node = h_lookup(State.HashDB, key, eq_entry);
    if(node && hnode_to_entry(node)->type != Z_SET){
        return 0;
    }
    else if (!node){
        entry = malloc(sizeof(Entry));
        entry->key = strdup(key);
        entry->node.hash = str_hash((uint8_t*) key, strlen(key));
        entry->type = Z_SET;
        entry->set.map = init_hashmap();
        entry->set.root = NULL;
        insert_node(&entry->node, State.HashDB);
        State.HashDB->entries++;
        possibly_resize(State.HashDB);
    }
    else{
        entry = hnode_to_entry(node);
    }
    HNode *old_name_hnode = h_lookup(entry->set.map, name, eq_znode);
    if(old_name_hnode){
        remove_zset(&entry->set, name);
        add_zset(&entry->set, name, score);
        return 1;
    }

    ZNode *new_ZNode = add_zset(&entry->set, name, score);
    if(!new_ZNode){
        return 0;
    }
    printf("%s: %f\n", new_ZNode->name, new_ZNode->score);
    return 1;
};

double Z_score(char* key, char* name){
    HNode *n = h_lookup(State.HashDB, key, eq_entry);
    if(!n){
        return -1;
    }
    Entry *ent = hnode_to_entry(n);
    if(ent->type != Z_SET){
        return -1;
    }
    ZNode *z_node = lookup_zset(ent->set, name);
    if(!z_node){
        return -1;
    }
    return z_node->score;
}

int Z_rem(char *key, char *name){
    HNode *n = h_lookup(State.HashDB, key, eq_entry);
    if(!n){
        return 0;
    }
    Entry *ent = hnode_to_entry(n);
    if(ent->type != Z_SET){
        return -1;
    }
    int ret = remove_zset(&ent->set, name);
    return ret;
}

int Z_rank(char *key, char *name){
    HNode *n = h_lookup(State.HashDB, key, eq_entry);
    if(!n){
        return 0;
    }
    Entry *ent = hnode_to_entry(n);
    if(ent->type != Z_SET){
        return -1;
    }
    return lookup_zset_rank(&ent->set, name);
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

//                     |  4 bytes  |     n     |
// message, n (len) -> |     n     |  message  | -> outgoing buffer
void feed_outgoing(Conn *conn, char *msg, int n){
    int total_bytes = sizeof(uint32_t) + n;
    if (conn->outgoing_len + total_bytes > OUTGOING_SIZE) {
        printf("Buffer overflow\n");
        return;
    }
    memcpy(conn->outgoing + conn->outgoing_len, &n, sizeof(uint32_t));
    memcpy(conn->outgoing + conn->outgoing_len + sizeof(uint32_t), msg, n);
    conn->outgoing_len += total_bytes;
}
int deserialize_params(uint32_t len, char *request, int *params_len, char ***params) {
    // this will be incremented
    int total_bytes = 0;

    uint32_t n;
    if (len <= total_bytes + sizeof(uint32_t) ) {
        return 0;
    }
    request += read_32bit(&n, request);
    total_bytes += sizeof(uint32_t);
    *params_len = n;
    *params = malloc(n * sizeof(char*));

    for (uint32_t i = 0; i < n; i++) {
        if (len <= total_bytes + sizeof(uint32_t)) {
            return 0;
        }
        uint32_t param_len;
        request += read_32bit(&param_len, request);
        total_bytes += sizeof(uint32_t);
        if (len < total_bytes + param_len) {
            return 0;
        }
        char *param = malloc(param_len + 1);
        memcpy(param, request, param_len);
        param[param_len] = '\0';
        (*params)[i] = param;
        request += param_len;
        total_bytes += param_len;
    }
    return 1;
}

// void out_str(char *str, int len, Response *response){
//     response->message = malloc(sizeof(uint8_t) + sizeof(uint32_t) + len);
//     response->response_len = 0;
//
//     uint8_t tag = TAG_STR;
//     printf("tag: %i\n", tag);
//
//     memcpy(response->message, &tag, sizeof(uint8_t));
//     response->response_len += sizeof(uint8_t);
//
//     memcpy(response->message + sizeof(uint8_t), &len, sizeof(uint32_t));
//     response->response_len += sizeof(uint32_t);
//
//     memcpy(response->message + sizeof(uint8_t) + sizeof(uint32_t), str, len);
//     response->response_len += len;
// }
//
void out_double(double val, Response *response){
    response->message = malloc(sizeof(uint8_t) + sizeof(uint64_t));
    response->response_len = sizeof(uint8_t) + sizeof(uint64_t);

    char* ptr = response->message;
    uint8_t tag = TAG_DOUBLE;

    memcpy(ptr, &tag, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    memcpy(ptr, &val, sizeof(uint64_t));
}

void out_str(char *str, int len, Response *response){
    response->message = malloc(sizeof(uint8_t) + sizeof(uint32_t) + len);
    response->response_len = sizeof(uint8_t) + sizeof(uint32_t) + len;

    char* ptr = response->message;
    uint8_t tag = TAG_STR;

    memcpy(ptr, &tag, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    memcpy(ptr, &len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, str, len);
}

void out_arr(char **arr, int len, Response *response){
    int total_keys_len = 0;
    int tag_and_len = sizeof(uint8_t) + sizeof(uint32_t);

    for(int x = 0; x < len; x++){
        total_keys_len+=strlen(arr[x]);
    }
    response->message = malloc( tag_and_len + len * tag_and_len + total_keys_len);
    response->response_len = tag_and_len + len * tag_and_len + total_keys_len;

    char* ptr = response->message;
    uint8_t tag = TAG_ARR;
    memcpy(ptr, &tag, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    memcpy(ptr, &len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    for(int x = 0; x < len; x++){
        char* el = arr[x];
        int el_len = strlen(el);
        uint8_t tag = TAG_STR;
        memcpy(ptr, &tag, sizeof(uint8_t));
        ptr += sizeof(uint8_t);

        memcpy(ptr, &el_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        memcpy(ptr, el, strlen(el));
        ptr += el_len;
    }
}

int perform_request(int params_len, char **params, Response *response){
    if ( params_len == 3 && strcmp(params[0], "set") == 0) {
        set_kv(params[1], params[2]);
        response->status = RES_OK;
        return 1;
    }
    else if ( params_len == 2 && strcmp(params[0], "get") == 0) {
        char *val = get_kv(params[1]);
        if (val) {
            response->status = RES_OK;
            out_str(val, strlen(val), response);
            return 1;
        } 
        response->status = RES_ERROR;
        char *err = "KEY_NOT_FOUND";
        out_str(err, strlen(err), response);
        return 1;
    }
    else if ( params_len == 2 && strcmp(params[0], "del") == 0) {
        if (del_kv(params[1])) {
            response->status = RES_OK;
            return 1;
        }
        response->status = RES_ERROR;
        char *err = "KEY_NOT_FOUND";
        out_str(err, strlen(err), response);
        return 1;
    }
    else if ( params_len == 1 && strcmp(params[0], "keys") == 0){
        char **keys = all_keys(State.HashDB);
        out_arr(keys, State.HashDB->entries, response); 
        response->status = RES_OK;
        return 1;
    }
    else if ( params_len == 4 && strcmp(params[0], "zadd") == 0){
        if (Z_add(params)){
            response->status = RES_OK;
            return 1;
        }
        response->status = RES_ERROR;
        char *err = "Something went wrong";
        out_str(err, strlen(err), response);
        return 1;
    }
    else if ( params_len == 3 && strcmp(params[0], "zscore") == 0){
        double score = Z_score(params[1], params[2]);
        if (score != -1){
            response->status = RES_OK;
            out_double(score, response);
            return 1;
        }
        response->status = RES_ERROR;
        char *err = "KEY_NOT_FOUND";
        out_str(err, strlen(err), response);
        return 1;
    }
    else if ( params_len == 3 && strcmp(params[0], "zrem") == 0){
        int status = Z_rem(params[1], params[2]);
        if (status){
            response->status = RES_OK;
            return 1;
        }
        response->status = RES_ERROR;
        char *err = "KEY_NOT_FOUND";
        out_str(err, strlen(err), response);
        return 1;
    }
    else if ( params_len == 3 && strcmp(params[0], "zrank") == 0){
        double rank = Z_rank(params[1], params[2]);
        out_double(*((double*) &rank), response);
        return 1;
    }
    response->status = RES_ERROR;
    return 0;
}

//               |  4 bytes   |     n     |
// Response* ->  | RES_STATUS |  message  | -> feed_outgoing()
int feed_response(Conn *conn, Response *response){
    int total_bytes = sizeof(uint32_t) + response->response_len;
    char *resp = malloc(total_bytes);
    memcpy(resp, &response->status, sizeof(uint32_t));
    memcpy(resp + sizeof(uint32_t), response->message, response->response_len);
    feed_outgoing(conn, resp, total_bytes);
    free(resp);
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
    if(!deserialize_params(len, request, &params_len, &params)){
        printf("somethinq went wrong\n");
        return false;
    }
    for (int i = 0; i < params_len; i++) {
        printf("param: %s\n", params[i]);
    }
    Response *response = calloc(1, sizeof(Response));
    perform_request(params_len, params, response);
    if (!response->message) {
        char *res = response_enum(response->status);
        response->message = malloc(response->response_len + sizeof(uint8_t) + sizeof(uint32_t));
        out_str(res, strlen(res), response);
    }

    // wrting the response to client's connection's outgoing buffer
    feed_response(conn, response);


    // flushing the current request from incoming buffer
    memmove(conn->incoming, conn->incoming + total_len, conn->incoming_len - total_len);
    conn->incoming_len -= total_len;
    for (int i = 0; i < params_len; i++) {
        free(params[i]);
    }
    free(params);
    free(request);
    free(response);
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
    Conn *newConn = calloc(1, sizeof(Conn));
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
        if (errno == EAGAIN || errno == EINTR) {
            return; // actually not ready
        }
        die("write() error");
    }
    if (rv == 0) {
        conn->want_close = true;
        return;
    }
    printf("byte_written: %li\n", rv);
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


// --------------------------------------------------------
// |     4     |                      n                   |
// |     n     |                   message                |
// --------------------------------------------------------
// +                                                      +
// ------------------------------------------------------------
// |           |   4    |   1   |   4  (except int)  |  len   |
// |           | status |  Tag  |  len (except int)  |  data  |
// ------------------------------------------------------------
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

        // Preparing polling args for connections
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

        // Handling Home Socket ( TAKING NEW CONNECTIONS )
        if (home_socket_arg.revents) {
            handle_accept(home_socket_arg.fd);
        }

        // Handling Already Established Connections
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
