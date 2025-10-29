FROM ubuntu:22.04

# Install dependencies (g++, netcat, etc.)
RUN apt-get update && apt-get install -y g++ netcat && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy and build the server
COPY server.cpp .
RUN g++ -o server server.cpp

# Expose server port (example: 8080)
EXPOSE 3030

# Run server
CMD ["./server"]

