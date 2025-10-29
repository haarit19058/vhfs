#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>      // htobe64 / be64toh
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <errno.h>

using namespace std;

// Helper that prints errno and exits (simple fail-fast for setup errors).
static void die(const char *m) {
    perror(m);
    exit(1);
}

static int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints{};
    struct addrinfo *res = nullptr;

    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        die("getaddrinfo");
    }

    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) {
        freeaddrinfo(res);
        die("socket");
    }

    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        close(s);
        die("connect");
    }

    freeaddrinfo(res);
    return s;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <mountpoint> <server-host> <server-port> [fuse-args...]\n", argv[0]);
        return 1;
    }

    const char *mountpoint = argv[1];
    const char *host = argv[2];
    const char *port = argv[3];

    // Establish persistent connection to the remote server before mounting.
    int sockfd = connect_to_server(host, port);
    if (sockfd < 0) die("connect_to_server");

    return 0;
}
