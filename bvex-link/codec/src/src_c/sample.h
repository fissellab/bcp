#pragma once

#include "cjson/cJSON.h"

enum which_data_type {
  WHICH_DATA_TYPE_PRIMITIVE,
  WHICH_DATA_TYPE_FILE,
};

/**
 * Encodes sample data into a JSON string
 *
 * @param metric_id The metric identifier
 * @param timestamp The timestamp of the sample
 * @param which The type of data being encoded
 * @param data The JSON data to encode
 * @return A JSON string representation of the sample, or NULL on error
 * @note The returned string must be freed by the caller
 */
char *encode_sample(const char *metric_id, double timestamp,
                    enum which_data_type which, cJSON *data);
