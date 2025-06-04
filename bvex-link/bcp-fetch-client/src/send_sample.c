#include "send_sample.h"
#include "generated/nanopb/sample.pb.h"
#include <arpa/inet.h> // send()
#include <errno.h>
#include <pb_encode.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int socket_fd;
    Sample sample;
} send_sample_data_t;

send_status_t send_sample(int socket_fd, const Sample message)
{
    uint8_t buffer[SAMPLE_PB_H_MAX_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if(!pb_encode(&stream, Sample_fields, &message)) {
#ifdef DEBUG
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
#endif
        return SEND_STATUS_ENCODING_ERROR;
    }

    ssize_t bytes_sent = send(socket_fd, buffer, stream.bytes_written, 0);
    if(bytes_sent < 0) {
#ifdef DEBUG
        fprintf(stderr, "send failed: %s\n", strerror(errno));
#endif
        return SEND_STATUS_SEND_ERROR;
    }
    return 0;
}
send_status_t send_sample_int32(int socket_fd, const char* metric_id,
                                float timestamp, int32_t value)
{
    if(!metric_id || *metric_id == 0) {
        return SEND_STATUS_ENCODING_ERROR;
    }
#ifdef BCP_FETCH_BOUNDS_CHECKING
    if(strnlen(metric_id, METRIC_ID_MAX_SIZE) == METRIC_ID_MAX_SIZE) {
        return BOUNDS_CHECK_ERROR;
    }
#endif
    Sample sample = Sample_init_zero;
    strlcpy(sample.metric_id, metric_id, METRIC_ID_MAX_SIZE);
    sample.timestamp = timestamp;
    sample.which_data = Sample_primitive_tag;
    sample.data.primitive.which_value = primitive_Primitive_int_val_tag;
    sample.data.primitive.value.int_val = value;
    return send_sample(socket_fd, sample);
}

send_status_t send_sample_int64(int socket_fd, const char* metric_id,
                                float timestamp, int64_t value)
{
    if(!metric_id || *metric_id == 0) {
        return SEND_STATUS_ENCODING_ERROR;
    }
#ifdef BCP_FETCH_BOUNDS_CHECKING
    if(strnlen(metric_id, METRIC_ID_MAX_SIZE) == METRIC_ID_MAX_SIZE) {
        return BOUNDS_CHECK_ERROR;
    }
#endif
    Sample sample = Sample_init_zero;
    strlcpy(sample.metric_id, metric_id, METRIC_ID_MAX_SIZE);
    sample.timestamp = timestamp;
    sample.which_data = Sample_primitive_tag;
    sample.data.primitive.which_value = primitive_Primitive_long_val_tag;
    sample.data.primitive.value.long_val = value;
    return send_sample(socket_fd, sample);
}

send_status_t send_sample_float(int socket_fd, const char* metric_id,
                                float timestamp, float value)
{
    if(!metric_id || *metric_id == 0) {
        return SEND_STATUS_ENCODING_ERROR;
    }
#ifdef BCP_FETCH_BOUNDS_CHECKING
    if(strnlen(metric_id, METRIC_ID_MAX_SIZE) == METRIC_ID_MAX_SIZE) {
        return BOUNDS_CHECK_ERROR;
    }
#endif
    Sample sample = Sample_init_zero;
    strlcpy(sample.metric_id, metric_id, METRIC_ID_MAX_SIZE);
    sample.timestamp = timestamp;
    sample.which_data = Sample_primitive_tag;
    sample.data.primitive.which_value = primitive_Primitive_float_val_tag;
    sample.data.primitive.value.float_val = value;
    return send_sample(socket_fd, sample);
}

send_status_t send_sample_double(int socket_fd, const char* metric_id,
                                 float timestamp, double value)
{
    if(!metric_id || *metric_id == 0) {
        return SEND_STATUS_ENCODING_ERROR;
    }
#ifdef BCP_FETCH_BOUNDS_CHECKING
    if(strnlen(metric_id, METRIC_ID_MAX_SIZE) == METRIC_ID_MAX_SIZE) {
        return BOUNDS_CHECK_ERROR;
    }
#endif
    Sample sample = Sample_init_zero;
    strlcpy(sample.metric_id, metric_id, METRIC_ID_MAX_SIZE);
    sample.timestamp = timestamp;
    sample.which_data = Sample_primitive_tag;
    sample.data.primitive.which_value = primitive_Primitive_double_val_tag;
    sample.data.primitive.value.double_val = value;
    return send_sample(socket_fd, sample);
}

send_status_t send_sample_bool(int socket_fd, const char* metric_id,
                               float timestamp, bool value)
{
    if(!metric_id || *metric_id == 0) {
        return SEND_STATUS_ENCODING_ERROR;
    }
#ifdef BCP_FETCH_BOUNDS_CHECKING
    if(strnlen(metric_id, METRIC_ID_MAX_SIZE) == METRIC_ID_MAX_SIZE) {
        return BOUNDS_CHECK_ERROR;
    }
#endif
    Sample sample = Sample_init_zero;
    strlcpy(sample.metric_id, metric_id, METRIC_ID_MAX_SIZE);
    sample.timestamp = timestamp;
    sample.which_data = Sample_primitive_tag;
    sample.data.primitive.which_value = primitive_Primitive_bool_val_tag;
    sample.data.primitive.value.bool_val = value;
    return send_sample(socket_fd, sample);
}

send_status_t send_sample_string(int socket_fd, const char* metric_id,
                                 float timestamp, const char* value)
{
    if(!metric_id || *metric_id == 0 || !value || *value == 0) {
        return SEND_STATUS_ENCODING_ERROR;
    }
#ifdef BCP_FETCH_BOUNDS_CHECKING
    if(strnlen(metric_id, METRIC_ID_MAX_SIZE) == METRIC_ID_MAX_SIZE ||
       strnlen(value, STRING_VALUE_MAX_SIZE) == STRING_VALUE_MAX_SIZE) {
        return BOUNDS_CHECK_ERROR;
    }
#endif
    Sample sample = Sample_init_zero;
    strlcpy(sample.metric_id, metric_id, METRIC_ID_MAX_SIZE);
    sample.timestamp = timestamp;
    sample.which_data = Sample_primitive_tag;
    sample.data.primitive.which_value = primitive_Primitive_string_val_tag;
    strlcpy(sample.data.primitive.value.string_val, value,
            STRING_VALUE_MAX_SIZE);
    return send_sample(socket_fd, sample);
}

send_status_t send_sample_file(int socket_fd, const char* metric_id,
                               float timestamp, const char* filepath,
                               const char* extension)
{
    if(!metric_id || *metric_id == 0 || !filepath || *filepath == 0 ||
       !extension || *extension == 0) {
        return SEND_STATUS_ENCODING_ERROR;
    }
#ifdef BCP_FETCH_BOUNDS_CHECKING
    if(strnlen(metric_id, METRIC_ID_MAX_SIZE) == METRIC_ID_MAX_SIZE ||
       strnlen(filepath, FILE_PATH_MAX_SIZE) == FILE_PATH_MAX_SIZE ||
       strnlen(extension, EXTENSION_MAX_SIZE) == EXTENSION_MAX_SIZE) {
        return BOUNDS_CHECK_ERROR;
    }
#endif
    Sample sample = Sample_init_zero;
    strlcpy(sample.metric_id, metric_id, METRIC_ID_MAX_SIZE);
    sample.timestamp = timestamp;
    sample.which_data = Sample_file_tag;
    strlcpy(sample.data.file.filepath, filepath, FILE_PATH_MAX_SIZE);
    strlcpy(sample.data.file.extension, extension, EXTENSION_MAX_SIZE);
    return send_sample(socket_fd, sample);
}