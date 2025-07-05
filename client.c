#include <pthread.h>
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

int read_full(int client_fd, char *buf) {
    int ret;
    char total_len[sizeof(int)];
    int len;
    ret = read_n_bytes(client_fd, total_len, sizeof(int));
    if( ret == -1 ) die("read_n_bytes");
    memcpy(&len, total_len, sizeof(int));

    if(len > MAXLEN) {
        len = MAXLEN;
    }

    ret = read_n_bytes(client_fd, buf, len);
    buf[len] = '\0';
    if( ret == -1 ) die("read_n_bytes");
    return 0;
}

void* handle_read_thread(void* raw_fd) {
    printf("reading thread started\n");
    int client_fd = *(int*) raw_fd;
    while(1) {
        char *buf = malloc(MAXLEN);
        read_full(client_fd, buf);
        printf("message from server: %s\n", buf);
    }
    printf("thread exited\n");
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


    char *query_list[5];
    query_list[0] = malloc(MAXLEN);
    query_list[1] = strdup("hello2");
    query_list[2] = strdup("hello3");
    query_list[3] = strdup("hello4");
    query_list[4] = strdup("hello5");
    if (!query_list[0]) {
        perror("malloc failed");
        exit(1);
    }
    memset(query_list[0], 'z', MAXLEN);
    query_list[0][MAXLEN - 1] = '\0';
    
    for (int i = 0; i < 5; i++) {
        write_full(sockfd, query_list[i], strlen(query_list[i]));
        free(query_list[i]);
    }

    pthread_t thread;
    pthread_create(&thread, NULL, handle_read_thread, &sockfd);  
    while(1){
        sleep(1);
    }
}
