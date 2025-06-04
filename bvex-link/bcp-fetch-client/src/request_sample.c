#include "request_sample.h"
#include "connected_udp_socket.h"
#include "generated/nanopb/request.pb.h"
#include "generated/nanopb/response.pb.h"
#include <arpa/inet.h> // send()
#include <errno.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Requester make_requester(const char* metric_id, const char* node,
                         const char* service)
{
    int sock = connected_udp_socket(node, service);
    if(sock < 0) {
        fprintf(stderr, "Socket creation failed\n");
        return (Requester){.socket_fd = -1, .metric_id = NULL};
    }

    Requester reqr;
    reqr.socket_fd = sock;
    reqr.metric_id = metric_id;
    return reqr;
}

int send_request(int socket_fd, const Request* message)
{
    uint8_t buffer[REQUEST_PB_H_MAX_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if(!pb_encode(&stream, Request_fields, message)) {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return REQUEST_STATUS_ENCODING_ERROR;
    }
    ssize_t bytes_sent = send(socket_fd, buffer, stream.bytes_written, 0);
    if(bytes_sent == -1) {
        fprintf(stderr, "send failed: %s\n", strerror(errno));
        return REQUEST_STATUS_SEND_ERROR;
    }
    return REQUEST_STATUS_OK;
}

int recv_response(int socket_fd, Response* response)
{
    char recv_buffer[RESPONSE_PB_H_MAX_SIZE];
    ssize_t bytes_received =
        recv(socket_fd, recv_buffer, RESPONSE_PB_H_MAX_SIZE, 0);
    if(bytes_received == -1) {
        fprintf(stderr, "recv failed: %s\n", strerror(errno));
        return REQUEST_STATUS_RECV_ERROR;
    } else if(bytes_received == 0) {
        fprintf(stderr, "Connection closed by the peer\n");
        return REQUEST_STATUS_RECV_ERROR;
    }

    bool status;

    pb_istream_t stream = pb_istream_from_buffer(recv_buffer, bytes_received);

    status = pb_decode(&stream, Response_fields, response);

    if(!status) {
        fprintf(stderr, "Decoding failed: %s\n", PB_GET_ERROR(&stream));
        return REQUEST_STATUS_DECODING_ERROR;
    }
    return REQUEST_STATUS_OK;
}

typedef struct {
    request_status_t err;
    Response response;
} RequestResult;

RequestResult request(Requester* reqr)
{
    RequestResult result;

    Request request = Request_init_zero;
    strcpy(request.metric_id, reqr->metric_id);
    int send_err = send_request(reqr->socket_fd, &request);
    if(send_err) {
        result.err = send_err;
        return result;
    }

    Response response = Response_init_zero;
    int recv_err = recv_response(reqr->socket_fd, &response);
    if(recv_err) {
        result.err = recv_err;
        return result;
    }

    if(!response.has_primitive) {
#ifdef DEBUG
        fprintf(stderr, "Response indicated failure\n");
#endif
        result.err = REQUEST_STATUS_INVALID_RESPONSE_TYPE;
        return result;
    }

    result.response = response;
    return result;
}

char* primitive_val_tag_to_string(const pb_size_t tag)
{
    switch(tag) {
    case primitive_Primitive_int_val_tag:
        return "int";
    case primitive_Primitive_long_val_tag:
        return "long";
    case primitive_Primitive_float_val_tag:
        return "float";
    case primitive_Primitive_double_val_tag:
        return "double";
    case primitive_Primitive_bool_val_tag:
        return "bool";
    case primitive_Primitive_string_val_tag:
        return "string";
    default:
        char* result = (char*)malloc(50);
        if(result != NULL) {
            sprintf(result, "unknown primitive value tag %d", tag);
        }
        return result;
    }
}

RequestIntResult request_int(const Requester* reqr)
{
    RequestIntResult result;

    RequestResult request_result = request((Requester*)reqr);
    if(request_result.err) {
        result.err = request_result.err;
    } else if(request_result.response.primitive.which_value !=
              primitive_Primitive_int_val_tag) {
#ifdef DEBUG
        fprintf(stderr, "Invalid response type.\n");
        fprintf(stderr, "Received %s instead of int.\n",
                primitive_val_tag_to_string(
                    request_result.response.primitive.which_value));
#endif
        result.err = REQUEST_STATUS_INVALID_RESPONSE_TYPE;
    } else {
        result.value = request_result.response.primitive.value.int_val;
    }
    return result;
}

RequestFloatResult request_float(const Requester* reqr)
{
    RequestFloatResult result;

    RequestResult request_result = request((Requester*)reqr);
    if(request_result.err) {
        result.err = request_result.err;
        return result;
    }

    if(request_result.response.primitive.which_value !=
       primitive_Primitive_float_val_tag) {
#ifdef DEBUG
        fprintf(stderr, "Invalid response type\n");
        fprintf(stderr, "Received %s instead of float.\n",
                primitive_val_tag_to_string(
                    request_result.response.primitive.which_value));
#endif
        result.err = REQUEST_STATUS_INVALID_RESPONSE_TYPE;
        return result;
    }

    result.value = request_result.response.primitive.value.float_val;
    return result;
}

RequestDoubleResult request_double(const Requester* reqr)
{
    RequestDoubleResult result;

    RequestResult request_result = request((Requester*)reqr);
    if(request_result.err) {
        result.err = request_result.err;
        return result;
    }

    if(request_result.response.primitive.which_value !=
       primitive_Primitive_double_val_tag) {
#ifdef DEBUG
        fprintf(stderr, "Invalid response type\n");
        fprintf(stderr, "Received %s instead of double.\n",
                primitive_val_tag_to_string(
                    request_result.response.primitive.which_value));
#endif
        result.err = REQUEST_STATUS_INVALID_RESPONSE_TYPE;
        return result;
    }

    result.value = request_result.response.primitive.value.double_val;
    return result;
}

RequestStringResult request_string(const Requester* reqr)
{
    RequestStringResult result;

    RequestResult request_result = request((Requester*)reqr);
    if(request_result.err) {
        result.err = request_result.err;
        return result;
    }

    if(request_result.response.primitive.which_value !=
       primitive_Primitive_string_val_tag) {
#ifdef DEBUG
        fprintf(stderr, "Invalid response type\n");
        fprintf(stderr, "Received %s instead of string.\n",
                primitive_val_tag_to_string(
                    request_result.response.primitive.which_value));
#endif
        result.err = REQUEST_STATUS_INVALID_RESPONSE_TYPE;
        return result;
    }

    result.value = strdup(request_result.response.primitive.value.string_val);
    return result;
}