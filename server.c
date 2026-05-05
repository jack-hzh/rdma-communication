#include "common.h"

/* Setup RDMA resources for server */
int setup_server_resources(struct rdma_context *ctx) {
    struct ibv_qp_init_attr qp_attr = {0};
    
    LOG_INFO("Setting up RDMA resources...");
    
    /* Create protection domain */
    ctx->pd = ibv_alloc_pd(ctx->cm_id->verbs);
    CHECK_ERROR_NULL(ctx->pd, "Failed to allocate protection domain");
    LOG_DEBUG("Protection domain allocated");
    
    /* Create completion queue */
    ctx->cq = ibv_create_cq(ctx->cm_id->verbs, 10, NULL, NULL, 0);
    CHECK_ERROR_NULL(ctx->cq, "Failed to create completion queue");
    LOG_DEBUG("Completion queue created");
    
    /* Initialize QP attributes */
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.send_cq = ctx->cq;
    qp_attr.recv_cq = ctx->cq;
    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    
    /* Create queue pair */
    ctx->qp = rdma_create_qp(ctx->cm_id, ctx->pd, &qp_attr);
    CHECK_ERROR_NULL(ctx->qp, "Failed to create queue pair");
    LOG_DEBUG("Queue pair created");
    
    return 0;
}

/* Register memory buffers */
int register_server_memory(struct rdma_context *ctx) {
    LOG_INFO("Registering memory buffers...");
    
    /* Allocate send and receive buffers */
    ctx->send_buffer = malloc(MAX_MESSAGE_SIZE);
    ctx->recv_buffer = malloc(MAX_MESSAGE_SIZE);
    CHECK_ERROR_NULL(ctx->send_buffer, "Failed to allocate send buffer");
    CHECK_ERROR_NULL(ctx->recv_buffer, "Failed to allocate receive buffer");
    
    /* Register send buffer */
    ctx->send_mr = ibv_reg_mr(ctx->pd, ctx->send_buffer, MAX_MESSAGE_SIZE,
                              IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                              IBV_ACCESS_REMOTE_WRITE);
    CHECK_ERROR_NULL(ctx->send_mr, "Failed to register send buffer");
    LOG_DEBUG("Send buffer registered (key: %u)", ctx->send_mr->rkey);
    
    /* Register receive buffer */
    ctx->recv_mr = ibv_reg_mr(ctx->pd, ctx->recv_buffer, MAX_MESSAGE_SIZE,
                              IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                              IBV_ACCESS_REMOTE_WRITE);
    CHECK_ERROR_NULL(ctx->recv_mr, "Failed to register receive buffer");
    LOG_DEBUG("Receive buffer registered (key: %u)", ctx->recv_mr->rkey);
    
    return 0;
}

/* Post receive work request */
int post_server_receive(struct rdma_context *ctx) {
    struct ibv_recv_wr recv_wr = {0};
    struct ibv_sge sge = {0};
    struct ibv_recv_wr *bad_recv_wr = NULL;
    
    LOG_DEBUG("Posting receive work request...");
    
    /* Setup scatter-gather element */
    sge.addr = (uintptr_t)ctx->recv_buffer;
    sge.length = MAX_MESSAGE_SIZE;
    sge.lkey = ctx->recv_mr->lkey;
    
    /* Setup receive work request */
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;
    recv_wr.wr_id = 1;
    
    /* Post receive work request */
    int ret = ibv_post_recv(ctx->qp, &recv_wr, &bad_recv_wr);
    CHECK_ERROR(ret, "Failed to post receive work request");
    
    return 0;
}

/* Post send work request */
int post_server_send(struct rdma_context *ctx, const char *message, int length) {
    struct ibv_send_wr send_wr = {0};
    struct ibv_sge sge = {0};
    struct ibv_send_wr *bad_send_wr = NULL;
    
    LOG_DEBUG("Posting send work request (length: %d)...", length);
    
    /* Copy message to send buffer */
    strncpy(ctx->send_buffer, message, MAX_MESSAGE_SIZE - 1);
    
    /* Setup scatter-gather element */
    sge.addr = (uintptr_t)ctx->send_buffer;
    sge.length = length;
    sge.lkey = ctx->send_mr->lkey;
    
    /* Setup send work request */
    send_wr.opcode = IBV_WR_SEND;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    send_wr.wr_id = 0;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    
    /* Post send work request */
    int ret = ibv_post_send(ctx->qp, &send_wr, &bad_send_wr);
    CHECK_ERROR(ret, "Failed to post send work request");
    
    return 0;
}

/* Wait for completion event */
int wait_server_completion(struct rdma_context *ctx) {
    struct ibv_wc wc = {0};
    int ne;
    
    LOG_DEBUG("Waiting for completion event...");
    
    ne = ibv_poll_cq(ctx->cq, 1, &wc);
    if (ne < 0) {
        LOG_ERROR("Failed to poll completion queue");
        return -1;
    }
    
    if (ne == 0) {
        LOG_ERROR("Timeout waiting for completion");
        return -1;
    }
    
    if (wc.status != IBV_WC_SUCCESS) {
        LOG_ERROR("Work completion failed with status: %s", ibv_wc_status_str(wc.status));
        return -1;
    }
    
    LOG_DEBUG("Completion received (opcode: %d)", wc.opcode);
    return 0;
}

/* Main server function */
int main(int argc, char *argv[]) {
    struct rdma_context ctx = {0};
    struct rdma_cm_id *listen_id = NULL;
    struct sockaddr_in sin = {0};
    int ret;
    
    LOG_INFO("RDMA Ping-Pong Server Starting...");
    
    /* Create event channel */
    ctx.ec = rdma_create_event_channel();
    CHECK_ERROR_NULL(ctx.ec, "Failed to create event channel");
    LOG_DEBUG("Event channel created");
    
    /* Create listening endpoint */
    ret = rdma_create_id(ctx.ec, &listen_id, NULL, RDMA_PS_TCP);
    CHECK_ERROR(ret, "Failed to create listening endpoint");
    LOG_DEBUG("Listening endpoint created");
    
    /* Bind to port */
    sin.sin_family = AF_INET;
    sin.sin_port = htons(RDMA_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    
    ret = rdma_bind_addr(listen_id, (struct sockaddr *)&sin);
    CHECK_ERROR(ret, "Failed to bind address");
    LOG_INFO("Bound to port %d", RDMA_PORT);
    
    /* Start listening */
    ret = rdma_listen(listen_id, 1);
    CHECK_ERROR(ret, "Failed to listen");
    LOG_INFO("Server listening on port %d...", RDMA_PORT);
    
    /* Wait for connection request */
    LOG_INFO("Waiting for client connection...");
    ret = rdma_get_cm_event(ctx.ec, &ctx.event);
    CHECK_ERROR(ret, "Failed to get CM event");
    
    if (ctx.event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        LOG_ERROR("Unexpected CM event: %d", ctx.event->event);
        return -1;
    }
    
    ctx.cm_id = ctx.event->id;
    rdma_ack_cm_event(ctx.event);
    LOG_INFO("Connection request received");
    
    /* Setup RDMA resources */
    ret = setup_server_resources(&ctx);
    CHECK_ERROR(ret, "Failed to setup resources");
    
    /* Register memory */
    ret = register_server_memory(&ctx);
    CHECK_ERROR(ret, "Failed to register memory");
    
    /* Post initial receive */
    ret = post_server_receive(&ctx);
    CHECK_ERROR(ret, "Failed to post receive");
    
    /* Accept connection */
    struct rdma_conn_param conn_param = {0};
    conn_param.initiator_depth = 1;
    conn_param.responder_resources = 1;
    ret = rdma_accept(ctx.cm_id, &conn_param);
    CHECK_ERROR(ret, "Failed to accept connection");
    LOG_INFO("Connection accepted");
    
    /* Wait for established event */
    ret = rdma_get_cm_event(ctx.ec, &ctx.event);
    CHECK_ERROR(ret, "Failed to get established event");
    if (ctx.event->event != RDMA_CM_EVENT_ESTABLISHED) {
        LOG_ERROR("Unexpected event: %d", ctx.event->event);
        return -1;
    }
    rdma_ack_cm_event(ctx.event);
    LOG_INFO("Connection established");
    
    /* Receive message from client */
    LOG_INFO("Waiting for client message...");
    ret = wait_server_completion(&ctx);
    CHECK_ERROR(ret, "Failed to receive message");
    LOG_INFO("Received message: %s", ctx.recv_buffer);
    
    /* Send response */
    const char *response = "Hello from RDMA Server!";
    ret = post_server_send(&ctx, response, strlen(response) + 1);
    CHECK_ERROR(ret, "Failed to post send");
    
    ret = wait_server_completion(&ctx);
    CHECK_ERROR(ret, "Failed to send response");
    LOG_INFO("Response sent");
    
    /* Cleanup */
    LOG_INFO("Cleaning up resources...");
    if (ctx.send_mr) ibv_dereg_mr(ctx.send_mr);
    if (ctx.recv_mr) ibv_dereg_mr(ctx.recv_mr);
    if (ctx.qp) rdma_destroy_qp(ctx.cm_id);
    if (ctx.cq) ibv_destroy_cq(ctx->cq);
    if (ctx.pd) ibv_dealloc_pd(ctx->pd);
    if (ctx.send_buffer) free(ctx.send_buffer);
    if (ctx.recv_buffer) free(ctx.recv_buffer);
    if (ctx.cm_id) rdma_destroy_id(ctx.cm_id);
    if (ctx.ec) rdma_destroy_event_channel(ctx.ec);
    
    LOG_INFO("Server shutdown complete");
    return 0;
}