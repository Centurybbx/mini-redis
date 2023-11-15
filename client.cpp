#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>

int main() {
    int client_socket;
    sockaddr_in server_address;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9966);
    server_address.sin_addr.s_addr = INADDR_ANY;

    connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address));
    std::cout << "Type message(type /exit to quit):\n";
    
    std::string msg = "Hello World!";
    while (true) {
        std::getline(std::cin, msg);
        if (msg == "/exit") {
            std::cout << "Exiting...\n";
            break;
        }

        send(client_socket, msg.c_str(), msg.size(), 0);


        char buf[1024] = {0};
        read(client_socket, buf, sizeof(buf));
        std::cout << "Server replied: " << buf << std::endl;
    }

    close(client_socket);

    return 0;
}