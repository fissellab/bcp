#include "codec.h"
#include "hiredis/hiredis.h"
#include <ctime>

void send_sample_int(redisContext *c, const char *addr, const char * port, const char *metric_id, int value) {
    time_t now = time(NULL);
    char* sample = encode_sample_int(metric_id, now, value);
    reply = redisCommand(context, "SET sample-cache:%s %s", metric_id, sample);
    freeReplyObject(reply);
}

void send_sample_float(redisContext *c, const char *addr, const char *port, const char *metric_id, float value) {
    time_t now = time(NULL);
    char* sample = encode_sample_float(metric_id, now, value);
    reply = redisCommand(context, "SET sample-cache:%s %s", metric_id, sample);
    freeReplyObject(reply);
}

