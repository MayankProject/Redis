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
        printf("%i\n", bytes_rt);
        if (bytes_rt <= 0) return -1;
        bytes_sent += bytes_rt;
    }
    free(buf);
    return 0;
};

int read_n_bytes(int fd, char *buf, int n) {
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

int read_full(int client_fd, char *status, char *buf) {
    int ret;
    uint32_t len;

    // TOTAL RES LEN
    ret = read_n_bytes(client_fd, (char*) &len, sizeof(uint32_t));
    if( ret == -1 ) die("read_n_bytes");

    // STATUS
    read_n_bytes(client_fd, (char*) &status, sizeof(uint32_t));
    len -= sizeof(uint32_t);

    if(len > MAXLEN) {
        die("Request too long\n");
    }

    ret = read_n_bytes(client_fd, buf, len);
    if( ret == -1 ) die("read_n_bytes");

    buf[len] = '\0';
    return 0;
}

void* handle_read_thread(void* raw_fd) {
    printf("reading thread started\n");
    int client_fd = *(int*) raw_fd;
    while(1) {
        char *buf = malloc(MAXLEN);
        int status = 0;
        read_full(client_fd, (char *) &status, buf);
        printf("status: %i\n", status);
        printf("message from server: %s\n", buf);
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

int main() {
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);
    addr.sin_family = AF_INET;
    int ret;

    int sockfd = socket(addr.sin_family, SOCK_STREAM, 0);
    ret = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    if( ret == -1 ) die("connect");

    #define total_params 3
    char* params1[total_params] = {"set", "name", "mayank"};
    char* params2[total_params-1] = {"del", "name"};
    char* params3[total_params-1] = {"get", "name"};
    char* request;
    int request_len;

    request_len = 0;
    request = malloc(1024);
    make_request(params1, total_params, request, &request_len);
    write_full(sockfd, request, request_len);
    free(request);

    request_len = 0;
    request = malloc(1024);
    make_request(params2, total_params-1, request, &request_len);
    write_full(sockfd, request, request_len);
    free(request);


    request_len = 0;
    request = malloc(1024);
    make_request(params3, total_params-1, request, &request_len);
    write_full(sockfd, request, request_len);
    free(request);
    
    pthread_t thread;
    pthread_create(&thread, NULL, handle_read_thread, &sockfd);  
    while(1){
        sleep(1);
    }
}
