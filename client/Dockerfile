FROM ubuntu:22.04

RUN apt-get update && apt-get install -y g++ netcat && apt install -y fuse3 libfuse3-dev && rm -rf /var/lib/apt/lists/* 

WORKDIR /app

# Copy and build the client
COPY client.cpp .
RUN g++ -o client client.cpp

# The client will connect to "server" (the hostname of the other container)
CMD ["./client"]

