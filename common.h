#ifndef __RDMA_COMMON_H__
#define __RDMA_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <infiniband/core.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <stdint.h>
#include <time.h>

#define RDMA_PORT 20079
#define MAX_MESSAGE_SIZE 1024
#define TIMEOUT_MS 5000

/* Logging macros */
#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

/* Error checking macros */
#define CHECK_ERROR(ret, msg) \
    do { \
        if ((ret) != 0) { \
            LOG_ERROR("%s: %s", msg, strerror(errno)); \
            return -1; \
        } \
    } while (0)

#define CHECK_ERROR_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            LOG_ERROR("%s: %s", msg, strerror(errno)); \
            return -1; \
        } \
    } while (0)

/* RDMA context structure */
struct rdma_context {
    struct rdma_event_channel *ec;
    struct rdma_cm_id *cm_id;
    struct rdma_cm_event *event;
    
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    
    struct ibv_mr *send_mr;
    struct ibv_mr *recv_mr;
    
    char *send_buffer;
    char *recv_buffer;
    
    uint64_t remote_addr;
    uint32_t remote_rkey;
};

#endif /* __RDMA_COMMON_H__ */