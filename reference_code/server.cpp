#include <bits/stdc++.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <thread>

using namespace std;

static int readn(int fd, void* buf, size_t n) {
    size_t left = n; char *p = (char*)buf;
    while (left) {
        ssize_t r = ::read(fd, p, left);
        if (r <= 0) return r;
        left -= r; p += r;
    }
    return n;
}
static int writen(int fd, const void* buf, size_t n) {
    size_t left = n; const char *p = (const char*)buf;
    while (left) {
        ssize_t w = ::write(fd, p, left);
        if (w <= 0) return w;
        left -= w; p += w;
    }
    return n;
}



static string joinpath(const string &root, const string &path) {
    if (path.empty() || path == "/") return root;
    if (path[0] == '/') return root + path;
    return root + "/" + path;
}

enum Op : uint32_t {
    OP_GETATTR=1, OP_READDIR=2, OP_OPEN=3, OP_READ=4, OP_WRITE=5,
OP_CREATE=6, OP_UNLINK=7, OP_MKDIR=8, OP_RMDIR=9, OP_TRUNCATE=10,
    OP_UTIMENS=11, OP_STATFS=12, OP_RELEASE=13
};



// Handles all requests from a single client over a persistent connection
int handle_one(int client, const string &root) {
    while (true) {
        auto send_errno = [&](int eno) {
            uint32_t status = htonl((uint32_t)eno);
            uint32_t zlen = htonl(0);
            writen(client, &status, sizeof(status));
            writen(client, &zlen, sizeof(zlen));
        };
        auto send_ok_with_data = [&](const void *data, uint32_t dlen) {
            uint32_t status = htonl(0);
            uint32_t dlen_be = htonl(dlen);
            writen(client, &status, sizeof(status));
            writen(client, &dlen_be, sizeof(dlen_be));
            if (dlen) writen(client, data, dlen);
        };


        uint32_t len_be;

        // Read the first 4 bytes from the client: message length in network byte order (big-endian)
        if (readn(client, &len_be, sizeof(len_be)) != sizeof(len_be)) return 0;

        uint32_t len = ntohl(len_be); // convert length to host byte order
        if (len < 4) return 0;        // minimum length must include opcode

        // Allocate buffer to hold the entire message
        vector<char> buf(len);

        // Read the full message into buffer
        if (readn(client, buf.data(), len) != (int)len) return 0;

        const char *p = buf.data();   // pointer to traverse the payload
        uint32_t op;

        // Extract the operation code (first 4 bytes of the message)
        memcpy(&op, p, 4); 
        p += 4;           // move pointer past the opcode
        op = ntohl(op);   // convert opcode to host byte order

        try {
            // ===== OP_GETATTR: get file or directory metadata =====
            if (op == OP_GETATTR) {
                uint32_t pathlen;
                memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen); // extract path length
                string path(p, p + pathlen);                                // read path string
                string full = joinpath(root, path);                        // join with server root

                struct stat st;
                // Fetch file/directory attributes
                if (lstat(full.c_str(), &st) == -1) { send_errno(errno); continue; }

                // Send struct stat bytes as success response
                send_ok_with_data(&st, sizeof(st));

            // ===== OP_READDIR: list directory entries =====
            } else if (op == OP_READDIR) {
                uint32_t pathlen;
                memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen); // extract path length
                string path(p, p + pathlen);                               // read path string
                string full = joinpath(root, path);                       // full path

                DIR *d = opendir(full.c_str());                            // open directory
                if (!d) { send_errno(errno); continue; }

                string out; // buffer to hold concatenated directory entries
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name,"..") == 0) continue; // skip "." & ".."
                    out.push_back('\0'); // delimiter
                    out += e->d_name;    // append entry name
                }
                closedir(d);

                send_ok_with_data(out.data(), out.size()); // send all entries to client

            // ===== OP_OPEN or OP_CREATE: open or create a file =====
            } else if (op == OP_OPEN || op == OP_CREATE) {
                uint32_t pathlen; memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen);
                string path(p, p + pathlen); p += pathlen;

                int flags; memcpy(&flags, p, 4); p += 4; flags = ntohl(flags);
                string full = joinpath(root, path);

                int fd;
                if (op == OP_CREATE) {
                    int mode; memcpy(&mode, p, 4); p += 4; mode = ntohl(mode);
                    fd = open(full.c_str(), flags | O_CREAT, mode); // create file
                } else {
                    fd = open(full.c_str(), flags);                 // open existing file
                }

                if (fd == -1) { send_errno(errno); continue; }

                uint64_t fdbe = htobe64((uint64_t)fd); // convert fd to network byte order
                send_ok_with_data(&fdbe, sizeof(fdbe));

            // ===== OP_READ: read data from file =====
            } else if (op == OP_READ) {
                uint64_t sfd; memcpy(&sfd, p, 8); p += 8; sfd = be64toh(sfd);
                uint64_t off; memcpy(&off, p, 8); p += 8; off = be64toh(off);
                uint32_t size; memcpy(&size, p, 4); p += 4; size = ntohl(size);

                vector<char> datab(size);
                ssize_t r = pread((int)sfd, datab.data(), size, (off_t)off);
                if (r == -1) { send_errno(errno); continue; }

                send_ok_with_data(datab.data(), (uint32_t)r);

            // ===== OP_WRITE: write data to file =====
            } else if (op == OP_WRITE) {
                uint64_t sfd; memcpy(&sfd, p, 8); p += 8; sfd = be64toh(sfd);
                uint64_t off; memcpy(&off, p, 8); p += 8; off = be64toh(off);
                uint32_t wsize; memcpy(&wsize, p, 4); p += 4; wsize = ntohl(wsize);

                const char *data = p; // pointer to data to write
                ssize_t w = pwrite((int)sfd, data, wsize, (off_t)off);
                if (w == -1) { send_errno(errno); continue; }

                uint32_t wrote_be = htonl((uint32_t)w);
                send_ok_with_data(&wrote_be, sizeof(wrote_be));

            // ===== OP_RELEASE: close a file descriptor =====
            } else if (op == OP_RELEASE) {
                uint64_t sfd; memcpy(&sfd, p, 8); p += 8; sfd = be64toh(sfd);
                close((int)sfd);
                send_ok_with_data(nullptr, 0);

            // ===== OP_UNLINK: delete a file =====
            } else if (op == OP_UNLINK) {
                uint32_t pathlen; memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen);
                string path(p, p + pathlen);
                string full = joinpath(root, path);
                if (unlink(full.c_str()) == -1) { send_errno(errno); continue; }
                send_ok_with_data(nullptr, 0);

            // ===== OP_MKDIR: create a directory =====
            } else if (op == OP_MKDIR) {
                uint32_t pathlen; memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen);
                string path(p, p + pathlen); p += pathlen;
                int mode; memcpy(&mode, p, 4); p += 4; mode = ntohl(mode);
                string full = joinpath(root, path);
                if (mkdir(full.c_str(), mode) == -1) { send_errno(errno); continue; }
                send_ok_with_data(nullptr, 0);

            // ===== OP_RMDIR: remove a directory =====
            } else if (op == OP_RMDIR) {
                uint32_t pathlen; memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen);
                string path(p, p + pathlen);
                string full = joinpath(root, path);
                if (rmdir(full.c_str()) == -1) { send_errno(errno); continue; }
                send_ok_with_data(nullptr, 0);

            // ===== OP_TRUNCATE: truncate a file =====
            } else if (op == OP_TRUNCATE) {
                uint32_t pathlen; memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen);
                string path(p, p + pathlen); p += pathlen;
                uint64_t size; memcpy(&size, p, 8); p += 8; size = be64toh(size);
                string full = joinpath(root, path);
                if (truncate(full.c_str(), (off_t)size) == -1) { send_errno(errno); continue; }
                send_ok_with_data(nullptr, 0);

            // ===== OP_UTIMENS: update file timestamps =====
            } else if (op == OP_UTIMENS) {
                uint32_t pathlen; memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen);
                string path(p, p + pathlen); p += pathlen;

                uint64_t at_sec, at_nsec, mt_sec, mt_nsec;
                memcpy(&at_sec, p, 8); p+=8; at_sec = be64toh(at_sec);
                memcpy(&at_nsec, p, 8); p+=8; at_nsec = be64toh(at_nsec);
                memcpy(&mt_sec, p, 8); p+=8; mt_sec = be64toh(mt_sec);
                memcpy(&mt_nsec, p, 8); p+=8; mt_nsec = be64toh(mt_nsec);

                struct timespec times[2];
                times[0].tv_sec = (time_t)at_sec; times[0].tv_nsec = (long)at_nsec;
                times[1].tv_sec = (time_t)mt_sec; times[1].tv_nsec = (long)mt_nsec;

                string full = joinpath(root, path);
                if (utimensat(AT_FDCWD, full.c_str(), times, AT_SYMLINK_NOFOLLOW) == -1) { send_errno(errno); continue; }
                send_ok_with_data(nullptr, 0);

            // ===== OP_STATFS: get filesystem stats =====
            } else if (op == OP_STATFS) {
                uint32_t pathlen; memcpy(&pathlen, p, 4); p += 4; pathlen = ntohl(pathlen);
                string path(p, p + pathlen);
                string full = joinpath(root, path);
                struct statvfs st;
                if (statvfs(full.c_str(), &st) == -1) { send_errno(errno); continue; }
                send_ok_with_data(&st, sizeof(st));

            // ===== Unknown operation =====
            } else {
                send_errno(EINVAL); // invalid operation
            }

        } catch(...) {
            // Catch-all exception handler for unexpected errors
            send_errno(EIO);
        }
    }

    return 0; // function ends only if client disconnects
}


int main(int argc,char **argv){
    if(argc < 3){
        fprintf(stderr, "Usage %s <root-path> <port> \n",argv[0]);
        return 1;
    }

    string root = argv[1];
    int port = atoi(argv[2]);

    int listenfd = socket(AF_INET,SOCK_STREAM,0); // returns a socket descriptor
    // AF_INET : address family : uses ipv4 addr for ipv6 use AF_INET6
    // SOCK_STREAM : Uses a stream socket
    // 0 : protocol : normally 0 means default and in SOCK_STREAM tcp is default
    // printf("value of listenfd is %d\n",listenfd);


    int on = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    // This line sets an option on the socket listenfd — specifically, it enables the option SO_REUSEADDR
    // SOL_SOCKET optional level means the optoin applesi to the socket layer 
    // what does SO_REUSEADDR does ?? It allows your server to bind to a port even if it’s in a TIME_WAIT state from a previous connection.

    struct sockaddr_in sa{};
    // This defines and zero-initializes a structure that holds the address information for the server socket
    // sin_family :  short address filily inet or AF_INET
    // sin_port : uint16_t : port number in network byte order
    // sin_addr : struct in_addr IPADDRESS
    sa.sin_family = AF_INET; 
    sa.sin_addr.s_addr = INADDR_ANY; // Bind this socket to all available network interfaces
    sa.sin_port = htons(port); // htons() means Host TO Network Short converts to network byte order

    if (bind(listenfd, (struct sockaddr*)&sa, sizeof(sa)) == -1) { perror("bind"); return 1;}
    // This assigns (binds) the socket to a specific IP address and port number on your machine.

    if (listen(listenfd, 10) == -1) { perror("listen"); return 1;}
    // This marks the socket as a listening (passive) socket — meaning it’s ready to accept incoming TCP connections.
    // The argument 10 is the backlog — the maximum number of pending connections that can wait in the queue before they’re accepted with accept()
    printf("Server serving root=%s on port %d\n", root.c_str(), port);


    while (true) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);

        int client = accept(listenfd, (struct sockaddr*)&cli, &clilen);
        if (client == -1) {
            perror("accept");
            continue;
        }

        std::cout << "Client connected: " << inet_ntoa(cli.sin_addr) << std::endl;

        // Create a new thread for each client
        std::thread([client, root]() {
            handle_one(client, root); // handle client requests
            close(client);            // close client socket when done
            std::cout << "Client disconnected\n";
        }).detach(); // detach so thread cleans up automatically
    }        
    return 0 ;
}
