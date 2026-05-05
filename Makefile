.PHONY: all build clean install-deps help server client

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -O2 -std=c99
LDFLAGS := -lrdmacm -libverbs

# Targets
all: build

build: server client

server: server.c common.h
	@echo "Building RDMA server..."
	$(CC) $(CFLAGS) -o rdma_server server.c $(LDFLAGS)
	@echo "Server build complete: rdma_server"

client: client.c common.h
	@echo "Building RDMA client..."
	$(CC) $(CFLAGS) -o rdma_client client.c $(LDFLAGS)
	@echo "Client build complete: rdma_client"

clean:
	@echo "Cleaning build artifacts..."
	rm -f rdma_server rdma_client *.o
	@echo "Clean complete"

install-deps:
	@echo "Installing RDMA dependencies..."
	@if command -v apt-get &> /dev/null; then \
		sudo apt-get update; \
		sudo apt-get install -y libibverbs-dev librdmacm-dev; \
	elif command -v yum &> /dev/null; then \
		sudo yum install -y libibverbs-devel librdmacm-devel; \
	else \
		echo "Unsupported package manager"; \
		exit 1; \
	fi
	@echo "Dependencies installed"

help:
	@echo "RDMA Communication - Makefile targets:"
	@echo "  make build        - Build both server and client (default)"
	@echo "  make server       - Build server only"
	@echo "  make client       - Build client only"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install-deps - Install required dependencies"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  Terminal 1: ./rdma_server"
	@echo "  Terminal 2: ./rdma_client localhost"