/* Project-owned tiny nc for early OP3 diagnostics. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

int main(int argc, char **argv)
{
    int listen_mode = 0, port = 0;
    const char *host = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-l")) listen_mode = 1;
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) i++;
        else if (argv[i][0] != '-' && !host) host = argv[i];
        else if (argv[i][0] != '-') port = atoi(argv[i]);
    }
    if (port <= 0) {
        fprintf(stderr, "usage: nc [-l] [-p port] [host] port\n");
        return 1;
    }

    int fd;
    if (listen_mode) {
        int ls = socket(AF_INET, SOCK_STREAM, 0);
        if (ls < 0) { perror("socket"); return 2; }
        int on = 1;
        setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)port);
        a.sin_addr.s_addr = INADDR_ANY;
        if (bind(ls, (struct sockaddr *)&a, sizeof(a)) < 0) {
            perror("bind");
            return 2;
        }
        if (listen(ls, 1) < 0) { perror("listen"); return 2; }
        fd = accept(ls, NULL, NULL);
        close(ls);
        if (fd < 0) { perror("accept"); return 2; }
    } else {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { perror("socket"); return 2; }
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)port);
        a.sin_addr.s_addr = inet_addr(host ? host : "127.0.0.1");
        if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
            perror("connect");
            return 2;
        }
    }

    struct pollfd pfd[2] = {
        { .fd = 0, .events = POLLIN },
        { .fd = fd, .events = POLLIN },
    };
    char buf[4096];
    while (poll(pfd, 2, -1) >= 0) {
        for (int i = 0; i < 2; i++) {
            if (pfd[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                ssize_t n = read(pfd[i].fd, buf, sizeof(buf));
                if (n <= 0) { close(fd); return 0; }
                if (write(pfd[i].fd == 0 ? fd : 1, buf, (size_t)n) < 0) {
                    close(fd);
                    return 0;
                }
            }
        }
    }
    close(fd);
    return 0;
}
