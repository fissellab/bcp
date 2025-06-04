#include "cjson/cJSON.h"

enum which_data_type {
  WHICH_DATA_TYPE_PRIMITIVE,
  WHICH_DATA_TYPE_FILE,
};

char *encode_sample(const char *metric_id, double timestamp,
                    enum which_data_type which, cJSON *data) {
  // Create the root object
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }

  // Create metadata object
  cJSON *metadata = cJSON_CreateObject();
  if (!metadata) {
    cJSON_Delete(root);
    return NULL;
  }

  // Add metadata fields
  cJSON_AddStringToObject(metadata, "metric_id", metric_id);
  cJSON_AddNumberToObject(metadata, "timestamp", timestamp);

  // Add which_data_type based on enum
  const char *which_str =
      (which == WHICH_DATA_TYPE_PRIMITIVE) ? "primitive" : "file";
  cJSON_AddStringToObject(metadata, "which_data_type", which_str);

  // Add metadata to root
  cJSON_AddItemToObject(root, "metadata", metadata);

  // Add data to root
  cJSON_AddItemToObject(root, "data", data);

  // Convert to string
  char *json_str = cJSON_PrintUnformatted(root);

  // Clean up
  cJSON_Delete(root);

  return json_str;
}
