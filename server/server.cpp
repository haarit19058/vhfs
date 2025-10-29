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
using namespace std;



int main(int argc,char** argv){

    if(argc < 3){
        fprintf(stderr, "Usage %s <root-path> <port>\n",argv[0]);
    }

    string root = argv[1];
    int port = atoi(argv[2]);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1; 
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(port);
    
    if (bind(listenfd, (struct sockaddr*)&sa, sizeof(sa)) == -1) { perror("bind"); return 1;}
    
    if (listen(listenfd, 10) == -1) { perror("listen"); return 1;}
    
    printf("Server serving root=%s on port %d\n", root.c_str(), port);

    struct sockaddr_in cli;
    socklen_t clilen = sizeof(cli);

    int client = accept(listenfd, (struct sockaddr*)&cli, &clilen);
    if (client == -1) { perror("accept"); return 0; }
    printf("Client connected: %s\n", inet_ntoa(cli.sin_addr));
    
    // while (true) {
    //
    //
    //
    //
    //     // handle_one(client, root);
    //     // close(client);
    //     printf("Client disconnected\n");
    // }

    return 0;
} 
