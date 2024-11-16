#include "socket.h"

int main(int argc, char **argv) {
  return make_async_connected_send_socket("localhost", 3000);
}