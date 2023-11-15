#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
  int server_socket , client_socket;
  sockaddr_in server_address, client_address;

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(9966);
  server_address.sin_addr.s_addr = INADDR_ANY;

  // TODO: error handling 
  bind(server_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address));
  listen(server_socket, 5);

  std::cout << "Server started..." << std::endl;

  socklen_t client_len = sizeof(client_address);
  client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_address), &client_len);

  char buf[1024] = {0};
  while (true) {
    read(client_socket, buf, sizeof(buf));
    std::cout << "Received: " << buf << std::endl;
    
    send(client_socket, buf, strlen(buf), 0);
  }

  close(client_socket);
  close(server_socket);

  return 0;
}
