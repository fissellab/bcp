#include <cjson/cJSON.h>
#include "sample.h"
#include <stdbool.h>
#include <string.h>
#include "primitive.h"

char *encode_int_sample(const char *metric_id, double timestamp, int value) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "which_primitive", "int");
  cJSON_AddNumberToObject(root, "value", value);
  return encode_sample(metric_id, timestamp, WHICH_DATA_TYPE_PRIMITIVE, root);
}

char *encode_float_sample(const char *metric_id, double timestamp, float value) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "which_primitive", "float");
  cJSON_AddNumberToObject(root, "value", value);
  return encode_sample(metric_id, timestamp, WHICH_DATA_TYPE_PRIMITIVE, root);
}

char *encode_bool_sample(const char *metric_id, double timestamp, bool value) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "which_primitive", "bool");
  cJSON_AddBoolToObject(root, "value", value);
  return encode_sample(metric_id, timestamp, WHICH_DATA_TYPE_PRIMITIVE, root);
}
