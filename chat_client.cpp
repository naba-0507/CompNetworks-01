#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include "net.hpp"
static std::atomic<bool> running(true);
static void receiver_thread(socket_t sock) {
char buf[1024];
for (;;) {
long long n = net::recv_some(sock, buf, sizeof buf);
if (n <= 0) {
std::cout << "\nDisconnected from server.\n";
running = false;
break;
}
std::cout.write(buf, static_cast<std::streamsize>(n));
std::cout.flush();
}
}
int main(int argc, char** argv) {
const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
const char* port = (argc > 2) ? argv[2] : "6000";
try {
net::Runtime runtime;
net::Socket s = net::tcp_connect(host, port);
std::cout << "Connected to chat server. Type /quit to leave.\n";
std::thread receiver(receiver_thread, s.get());
receiver.detach();
std::string line;
while (running && std::getline(std::cin, line)) {
line += "\n";
if (!net::send_all(s.get(), line.data(), line.size())) break;
if (line.substr(0, 5) == "/quit") break;
}
} catch (const std::exception& e) {
std::cerr << "Error: " << e.what() << "\n";
return 1;
}
return 0;
}