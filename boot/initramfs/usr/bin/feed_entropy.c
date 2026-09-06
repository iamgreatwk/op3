/* Project-owned early CRNG feeder for OP3 (no hardware RNG seed). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/random.h>
#include <time.h>

int main(void)
{
    int fd = open("/dev/random", O_WRONLY);
    if (fd < 0) { perror("open /dev/random"); return 1; }

    char buf[8192];
    size_t n = 0;
    const char *srcs[] = { "/proc/interrupts", "/proc/stat",
                           "/proc/meminfo", "/proc/uptime", NULL };
    for (int i = 0; srcs[i] && n < sizeof(buf) - 256; i++) {
        int f = open(srcs[i], O_RDONLY);
        if (f >= 0) {
            ssize_t r = read(f, buf + n, sizeof(buf) - n - 1);
            if (r > 0) n += (size_t)r;
            close(f);
        }
    }

    char extra[96];
    int el = snprintf(extra, sizeof(extra), "feed_entropy %ld %d %ld",
                      (long)time(NULL), (int)getpid(), (long)random());
    if (el > 0 && n + (size_t)el < sizeof(buf)) {
        memcpy(buf + n, extra, (size_t)el);
        n += (size_t)el;
    }
    if (n < 64) {
        fprintf(stderr, "too little data: %zu\n", n);
        return 3;
    }
    if (n > 4096) n = 4096;

    struct {
        struct rand_pool_info rpi;
        char data[4096];
    } info;
    memset(&info, 0, sizeof(info));
    info.rpi.entropy_count = (int)(n * 8);
    info.rpi.buf_size = (int)n;
    memcpy(info.rpi.buf, buf, n);
    if (ioctl(fd, RNDADDENTROPY, &info) < 0) {
        perror("RNDADDENTROPY");
        close(fd);
        return 2;
    }
    close(fd);
    printf("feed_entropy: added %zu bytes\n", n);

    int pf = open("/dev/urandom", O_RDONLY);
    if (pf >= 0) {
        char probe[16];
        if (read(pf, probe, sizeof(probe)) == (ssize_t)sizeof(probe))
            puts("feed_entropy: urandom OK");
        close(pf);
    }
    return 0;
}
