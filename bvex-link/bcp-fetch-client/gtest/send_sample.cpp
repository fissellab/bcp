#include "../test_common/decode_sample.h"
#include <arpa/inet.h>
#include <connected_udp_socket.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <send_sample.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Helper function to create a test UDP server
static int create_test_server(const char* port)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1) {
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(port));

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) ==
       -1) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Helper function to receive data from the server
static ssize_t receive_data(int server_fd, char* buffer, size_t buffer_size)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    return recvfrom(server_fd, buffer, buffer_size, 0,
                    (struct sockaddr*)&client_addr, &addr_len);
}

class SendSampleTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create test server
        server_fd = create_test_server("8080");
        ASSERT_NE(server_fd, -1);

        // Create client socket
        client_fd = connected_udp_socket("localhost", "8080");
        ASSERT_NE(client_fd, -1);
    }

    void TearDown() override
    {
        if(server_fd != -1) {
            close(server_fd);
        }
        if(client_fd != -1) {
            close(client_fd);
        }
    }

    int server_fd = -1;
    int client_fd = -1;
};

TEST_F(SendSampleTest, SendInt32Sample)
{
    const char* metric_id = "test_int32";
    float timestamp = 1234.5f;
    int32_t value = 42;

    send_status_t status =
        send_sample_int32(client_fd, metric_id, timestamp, value);
    EXPECT_EQ(status, SEND_STATUS_OK);

    char buffer[1024];
    ssize_t received = receive_data(server_fd, buffer, sizeof(buffer));
    EXPECT_GT(received, 0);

    Sample* decoded_sample = decode_sample((uint8_t*)buffer, received);
    ASSERT_NE(decoded_sample, nullptr);
    EXPECT_EQ(decoded_sample->which_data, Sample_primitive_tag);
    EXPECT_EQ(decoded_sample->data.primitive.which_value,
              primitive_Primitive_int_val_tag);
    EXPECT_EQ(decoded_sample->data.primitive.value.int_val, value);
    EXPECT_EQ(decoded_sample->timestamp, timestamp);
    EXPECT_STREQ(decoded_sample->metric_id, metric_id);
    free(decoded_sample);
}

TEST_F(SendSampleTest, SendInt64Sample)
{
    const char* metric_id = "test_int64";
    float timestamp = 1234.5f;
    int64_t value = 1234567890;

    send_status_t status =
        send_sample_int64(client_fd, metric_id, timestamp, value);
    EXPECT_EQ(status, SEND_STATUS_OK);

    char buffer[1024];
    ssize_t received = receive_data(server_fd, buffer, sizeof(buffer));
    EXPECT_GT(received, 0);

    Sample* decoded_sample = decode_sample((uint8_t*)buffer, received);
    ASSERT_NE(decoded_sample, nullptr);
    EXPECT_EQ(decoded_sample->which_data, Sample_primitive_tag);
    EXPECT_EQ(decoded_sample->data.primitive.which_value,
              primitive_Primitive_long_val_tag);
    EXPECT_EQ(decoded_sample->data.primitive.value.long_val, value);
    EXPECT_EQ(decoded_sample->timestamp, timestamp);
    EXPECT_STREQ(decoded_sample->metric_id, metric_id);
    free(decoded_sample);
}

TEST_F(SendSampleTest, SendFloatSample)
{
    const char* metric_id = "test_float";
    float timestamp = 1234.5f;
    float value = 3.14159f;

    send_status_t status =
        send_sample_float(client_fd, metric_id, timestamp, value);
    EXPECT_EQ(status, SEND_STATUS_OK);

    char buffer[1024];
    ssize_t received = receive_data(server_fd, buffer, sizeof(buffer));
    EXPECT_GT(received, 0);

    Sample* decoded_sample = decode_sample((uint8_t*)buffer, received);
    ASSERT_NE(decoded_sample, nullptr);
    EXPECT_EQ(decoded_sample->which_data, Sample_primitive_tag);
    EXPECT_EQ(decoded_sample->data.primitive.which_value,
              primitive_Primitive_float_val_tag);
    EXPECT_FLOAT_EQ(decoded_sample->data.primitive.value.float_val, value);
    EXPECT_EQ(decoded_sample->timestamp, timestamp);
    EXPECT_STREQ(decoded_sample->metric_id, metric_id);
    free(decoded_sample);
}

TEST_F(SendSampleTest, SendDoubleSample)
{
    const char* metric_id = "test_double";
    float timestamp = 1234.5f;
    double value = 2.718281828459045;

    send_status_t status =
        send_sample_double(client_fd, metric_id, timestamp, value);
    EXPECT_EQ(status, SEND_STATUS_OK);

    char buffer[1024];
    ssize_t received = receive_data(server_fd, buffer, sizeof(buffer));
    EXPECT_GT(received, 0);

    Sample* decoded_sample = decode_sample((uint8_t*)buffer, received);
    ASSERT_NE(decoded_sample, nullptr);
    EXPECT_EQ(decoded_sample->which_data, Sample_primitive_tag);
    EXPECT_EQ(decoded_sample->data.primitive.which_value,
              primitive_Primitive_double_val_tag);
    EXPECT_DOUBLE_EQ(decoded_sample->data.primitive.value.double_val, value);
    EXPECT_EQ(decoded_sample->timestamp, timestamp);
    EXPECT_STREQ(decoded_sample->metric_id, metric_id);
    free(decoded_sample);
}

TEST_F(SendSampleTest, SendBoolSample)
{
    const char* metric_id = "test_bool";
    float timestamp = 1234.5f;
    bool value = true;

    send_status_t status =
        send_sample_bool(client_fd, metric_id, timestamp, value);
    EXPECT_EQ(status, SEND_STATUS_OK);

    char buffer[1024];
    ssize_t received = receive_data(server_fd, buffer, sizeof(buffer));
    EXPECT_GT(received, 0);

    Sample* decoded_sample = decode_sample((uint8_t*)buffer, received);
    ASSERT_NE(decoded_sample, nullptr);
    EXPECT_EQ(decoded_sample->which_data, Sample_primitive_tag);
    EXPECT_EQ(decoded_sample->data.primitive.which_value,
              primitive_Primitive_bool_val_tag);
    EXPECT_EQ(decoded_sample->data.primitive.value.bool_val, value);
    EXPECT_EQ(decoded_sample->timestamp, timestamp);
    EXPECT_STREQ(decoded_sample->metric_id, metric_id);
    free(decoded_sample);
}

TEST_F(SendSampleTest, SendStringSample)
{
    const char* metric_id = "test_string";
    float timestamp = 1234.5f;
    const char* value = "Hello, World!";

    send_status_t status =
        send_sample_string(client_fd, metric_id, timestamp, value);
    EXPECT_EQ(status, SEND_STATUS_OK);

    char buffer[1024];
    ssize_t received = receive_data(server_fd, buffer, sizeof(buffer));
    EXPECT_GT(received, 0);

    Sample* decoded_sample = decode_sample((uint8_t*)buffer, received);
    ASSERT_NE(decoded_sample, nullptr);
    EXPECT_EQ(decoded_sample->which_data, Sample_primitive_tag);
    EXPECT_EQ(decoded_sample->data.primitive.which_value,
              primitive_Primitive_string_val_tag);
    EXPECT_STREQ(decoded_sample->data.primitive.value.string_val, value);
    EXPECT_EQ(decoded_sample->timestamp, timestamp);
    EXPECT_STREQ(decoded_sample->metric_id, metric_id);
    free(decoded_sample);
}

TEST_F(SendSampleTest, SendFileSample)
{
    const char* metric_id = "test_file";
    float timestamp = 1234.5f;
    const char* filepath = "test.txt";
    const char* extension = "txt";

    send_status_t status =
        send_sample_file(client_fd, metric_id, timestamp, filepath, extension);
    EXPECT_EQ(status, SEND_STATUS_OK);

    char buffer[1024];
    ssize_t received = receive_data(server_fd, buffer, sizeof(buffer));
    EXPECT_GT(received, 0);

    Sample* decoded_sample = decode_sample((uint8_t*)buffer, received);
    ASSERT_NE(decoded_sample, nullptr);
    EXPECT_EQ(decoded_sample->which_data, Sample_file_tag);
    EXPECT_STREQ(decoded_sample->data.file.filepath, filepath);
    EXPECT_STREQ(decoded_sample->data.file.extension, extension);
    EXPECT_EQ(decoded_sample->timestamp, timestamp);
    EXPECT_STREQ(decoded_sample->metric_id, metric_id);
    free(decoded_sample);

    // Clean up test file
    unlink(filepath);
}

#ifdef BCP_FETCH_BOUNDS_CHECKING
TEST_F(SendSampleTest, InvalidMetricId)
{
    // Create a metric_id that exceeds the maximum size
    char long_metric_id[METRIC_ID_MAX_SIZE + 1];
    memset(long_metric_id, 'a', sizeof(long_metric_id) - 1);
    long_metric_id[sizeof(long_metric_id) - 1] = '\0';

    send_status_t status =
        send_sample_int32(client_fd, long_metric_id, 1234.5f, 42);
    EXPECT_EQ(status, BOUNDS_CHECK_ERROR);
}
#endif

TEST_F(SendSampleTest, InvalidSocket)
{
    send_status_t status = send_sample_int32(-1, "test", 1234.5f, 42);
    EXPECT_EQ(status, SEND_STATUS_SEND_ERROR);
}

TEST_F(SendSampleTest, NullMetricId)
{
    send_status_t status = send_sample_int32(client_fd, nullptr, 1234.5f, 42);
    EXPECT_EQ(status, SEND_STATUS_ENCODING_ERROR);
}
