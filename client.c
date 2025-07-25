#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>

#define PORT 8090
#define MAXLEN (32 << 20)

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
    uint8_t Tag;
    uint32_t Status;
    union {
        uint32_t number;
        uint64_t double_num;
        struct {
            uint32_t len;
            char *val;
        } str;
        struct {
            uint32_t len;
            struct Response **val;
        } arr;
    };
} Response;

void die(char *msg) {
    perror(msg);
    exit(1);
}

#define K_MAX_MSG (32 << 20)

int write_full(int fd, char *msg, int n) {
    int total_bytes = sizeof(uint32_t) + n;
    char *buf = malloc(total_bytes);
    memcpy(buf, &n, sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), msg, n);
    int bytes_sent = 0;
    while(bytes_sent < total_bytes) {
        int bytes_rt = write(fd, &buf[bytes_sent], total_bytes - bytes_sent);
        if (bytes_rt <= 0) return -1;
        bytes_sent += bytes_rt;
    }
    free(buf);
    return 0;
};

int read_n_bytes(int fd, void *buf, int n) {
    while(n > 0){
        ssize_t bytes_read = read(fd, buf, n);
        if (bytes_read < 0) return -1;
        else if ( bytes_read == 0 ){
            close(fd);
            return -1;
        };
        n -= bytes_read;
        buf += bytes_read;
    };
    return 0;
}

int read_by_tag(int client_fd, Response *response){
    int ret;
    switch (response->Tag) {
        case TAG_STR: {
            int len;
            read_n_bytes(client_fd,  &len, sizeof(uint32_t));
            response->str.val = calloc(len+1, 1);
            response->str.len = len;
            ret = read_n_bytes(client_fd, response->str.val, len);
            if( ret == -1 ) die("read_n_bytes");
            response->str.val[len] = '\0';
            return 1;
        };
        case TAG_INT: {
            ret = read_n_bytes(client_fd, &response->number, sizeof(uint32_t));
            if( ret == -1 ) die("read_n_bytes");
            return 1;
        };
        case TAG_DOUBLE: {
            ret = read_n_bytes(client_fd, &response->double_num, sizeof(uint64_t));
            if( ret == -1 ) die("read_n_bytes");
            return 1;
        };
        case TAG_ARR: {
            int len;
            read_n_bytes(client_fd, &len, sizeof(uint32_t));
            response->arr.val = calloc(len, sizeof(Response*));
            response->arr.len = len;
            for(int i = 0; i < len; i++){
                Response *element = calloc(1, sizeof(Response));
                read_n_bytes(client_fd, &element->Tag, sizeof(uint8_t));
                read_by_tag(client_fd, element);
                response->arr.val[i] = element;
            } 
            return 1;
        };
        case TAG_NIL:{
            return 1;
        }
    };
    return 0;
};

int read_full(int client_fd, Response *response) {
    int ret;
    uint32_t res_len;
    // TOTAL RES LEN
    ret = read_n_bytes(client_fd, &res_len, sizeof(uint32_t));
    if( ret == -1 ) die("read_n_bytes");
    printf("res_len: %i\n", res_len);

    if(res_len > MAXLEN) {
        die("Response too long\n");
    }

    // STATUS
    read_n_bytes(client_fd, &response->Status, sizeof(uint32_t));
    read_n_bytes(client_fd, &response->Tag, sizeof(uint8_t));
    printf("tag: %i\n", response->Tag);
    read_by_tag(client_fd, response);
    return 0;
}

void output_response(Response *response){
    switch (response->Tag) {
        case TAG_STR:{
            printf("element: %s\n", response->str.val);
            return;
        };
        case TAG_ARR: {
            for(uint32_t x = 0; x < response->arr.len; x++){
                output_response(response->arr.val[x]);
            }
            return;
        };
        case TAG_DOUBLE: {
            double val;
            memcpy(&val, &response->double_num, sizeof(val));
            printf("element: %f\n", val);
            return;
        }
    }
}
void* handle_read_thread(void* raw_fd) {
    printf("reading thread started\n");
    int client_fd = *(int*) raw_fd;
    while(1) {
        Response *response = malloc(sizeof(Response));
        read_full(client_fd, response);
        output_response(response);
    }
    printf("thread exited\n");
}

int make_request(char **params, int params_len, char *request, int *request_len) {
    memcpy(request, &params_len, sizeof(uint32_t));
    *request_len += sizeof(uint32_t);

    for (int i = 0; i < params_len; i++) {
        char *param = params[i];

        int len = strlen(param);
        memcpy(request + *request_len, &len, sizeof(len));
        *request_len += sizeof(len);

        memcpy(request + *request_len, param, len);
        *request_len += len;
    }
    return 1;
}

typedef struct Request {
    char **params;
    int    len;
} Request;

#define MAX_BATCH 16

static Request *gen_request(int len, char *argv[]){
    Request *req  = malloc(sizeof *req);
    req->len      = len;
    req->params   = argv;
    return req;
}

static size_t build_sample_batch(Request *batch[MAX_BATCH])
{
    static const struct {
        int   argc;
        char *argv[6];
    } script[] = {
        {3, {"set", "mayank", "8"}},

        {4, {"zadd", "leaderboard", "ansh", "8"}},
        {4, {"zadd", "leaderboard", "bravo", "8"}},
        {4, {"zadd", "leaderboard", "charlie", "8"}},
        {4, {"zadd", "leaderboard", "delta", "8"}},
        {4, {"zadd", "leaderboard", "mayank", "6"}},
        {3, {"zrank", "leaderboard", "ansh"}},
        {3, {"zrank", "leaderboard", "bravo"}},
        {3, {"zrank", "leaderboard", "charlie"}},
        {3, {"zrank", "leaderboard", "delta"}},
        {3, {"zrank", "leaderboard", "mayank"}},
        // {4, {"zadd", "leaderboard", "kyle", "1"}},
        // {4, {"zadd", "leaderboard", "sam", "3"}},
        // {4, {"zadd", "leaderboard", "lara", "17"}},
        // {4, {"zadd", "leaderboard", "nina", "14"}},
        // {4, {"zadd", "leaderboard", "zara", "18"}},
        // {4, {"zadd", "leaderboard", "aaron", "4"}},
        // {4, {"zadd", "leaderboard", "leo", "19"}},
        // {3, {"zrem", "leaderboard", "ansh"}},
        // {3, {"zrem", "leaderboard", "nina"}},
        // {3, {"zscore", "leaderboard", "jdksaksadlj"}},
        // {2, {"get", "mayank"}},
    };

    size_t nreq = sizeof script / sizeof script[0];
    if (nreq > MAX_BATCH) nreq = MAX_BATCH;

    for (size_t i = 0; i < nreq; i++){
        batch[i] = gen_request(script[i].argc, (char**) script[i].argv);
    }

    for (size_t i = nreq; i < MAX_BATCH; i++){
        batch[i] = NULL;
    }

    return nreq;
}

static void dump_batch(Request *batch[], size_t nreq)
{
    for (size_t i = 0; i < nreq; ++i) {
        Request *r = batch[i];
        printf("[%zu] (len=%d):", i, r->len);
        for (int j = 0; j < r->len; ++j)
            printf(" %s", r->params[j]);
        putchar('\n');
    }
}
int main() {
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);
    addr.sin_family = AF_INET;
    int ret;

    int sockfd = socket(addr.sin_family, SOCK_STREAM, 0);
    ret = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    if( ret == -1 ) die("connect");

    Request *batch_requests[MAX_BATCH];

    size_t count = build_sample_batch(batch_requests);
    dump_batch(batch_requests, count);

    for (int x = 0; x < count; x++){
        struct Request *request = batch_requests[x];
        if(!request){
            continue;
        }
        char* response;
        int res_len = 0;
        response = malloc(1024);
        make_request(request->params, request->len, response, &res_len);
        write_full(sockfd, response, res_len);
    }

    pthread_t thread;
    pthread_create(&thread, NULL, handle_read_thread, &sockfd);  
    while(1){
        sleep(1);
    }
}
