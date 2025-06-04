#pragma once

#include <stdbool.h>

char *encode_int_sample(const char *metric_id, double timestamp, int value);
char *encode_float_sample(const char *metric_id, double timestamp, float value);
char *encode_bool_sample(const char *metric_id, double timestamp, bool value);
