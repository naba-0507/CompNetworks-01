#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include "net.hpp"
static std::mutex g_log_mutex;
static std::atomic<int> g_active{0};
static void serve_client(net::Socket client) {
++g_active;
{
std::lock_guard<std::mutex> lock(g_log_mutex);
std::cout << "Active clients: " << g_active << "\n";
}
char buf[1024];
for (;;) {
long long n = net::recv_some(client.get(), buf, sizeof buf);
if (n <= 0) break;
if (!net::send_all(client.get(), buf, static_cast<size_t>(n))) break;
}
--g_active;
}
int main(int argc, char** argv) {
const char* port = (argc > 1) ? argv[1] : "5000";
try {
net::Runtime runtime;
net::Socket listener = net::tcp_listen(port);
std::cout << "Threaded echo server on port " << port << "\n";
for (;;) {
net::Socket client(accept(listener.get(), nullptr, nullptr));
if (!client.valid()) continue;
std::thread(serve_client, std::move(client)).detach();
}
} catch (const std::exception& e) {
std::cerr << "Error: " << e.what() << "\n";
return 1;
}
}