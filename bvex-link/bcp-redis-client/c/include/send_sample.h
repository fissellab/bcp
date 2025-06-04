#pragma once

#include "hiredis/hiredis.h"

void send_sample_int(redisContext *c, const char *addr, const char *port,
                     const char *metric_id, int value);

void send_sample_float(redisContext *c, const char *addr, const char *port,
                       const char *metric_id, float value);
