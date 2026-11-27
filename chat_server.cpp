#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>
#include "net.hpp"
struct Client {
net::Socket sock;
std::string name;
std::string inbuf;
};
static bool valid_name(const std::string& n) {
if (n.empty() || n.size() > 16) return false;
for (char c : n) {
if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
}
return true;
}
static void broadcast(std::vector<Client>& clients, const std::string& msg,
socket_t except = INVALID_SOCK) {
for (auto& c : clients) {
if (c.sock.get() != except) net::send_all(c.sock.get(), msg.data(),msg.size());
}
}
static void send_to(Client& c, const std::string& msg) {
net::send_all(c.sock.get(), msg.data(), msg.size());
}
int main(int argc, char** argv) {
const char* port = (argc > 1) ? argv[1] : "6000";
try {
net::Runtime runtime;
net::Socket listener = net::tcp_listen(port, 64);
std::vector<Client> clients;
int next_id = 1;
std::cout << "Chat server on port " << port << ". Up to 60 clients.\n";
for (;;) {
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(listener.get(), &readfds);
socket_t maxfd = listener.get();
for (auto& c : clients) {
FD_SET(c.sock.get(), &readfds);
if (c.sock.get() > maxfd) maxfd = c.sock.get();
}
int ready = select(static_cast<int>(maxfd) + 1, &readfds, nullptr, nullptr,nullptr);
if (ready < 0) {
std::cerr << "select failed: " << net::error_text(net::last_error()) <<"\n";
break;
}
if (FD_ISSET(listener.get(), &readfds)) {
sockaddr_storage peer{};
sock_len peer_len = sizeof peer;
net::Socket client_sock(accept(listener.get(), reinterpret_cast<sockaddr*>
(&peer), &peer_len));
if (client_sock.valid()) {
if (clients.size() >= 60) {
const std::string full = "Error: server full\n";
net::send_all(client_sock.get(), full.data(), full.size());
} else {
Client c;
c.name = "user" + std::to_string(next_id++);
c.sock = std::move(client_sock);
socket_t fd = c.sock.get();
clients.push_back(std::move(c));
send_to(clients.back(),
"Welcome! You are " + clients.back().name + "\n");
broadcast(clients, clients.back().name + " joined\n", fd);
std::cout << clients.back().name << " connected\n";
}
}
}
for (size_t i = 0; i < clients.size(); ) {
socket_t fd = clients[i].sock.get();
bool close_client = false;
if (FD_ISSET(fd, &readfds)) {
char buf[1024];
long long n = net::recv_some(fd, buf, sizeof buf);
if (n <= 0) {
close_client = true;
} else {
clients[i].inbuf.append(buf, static_cast<size_t>(n));
size_t pos;
while ((pos = clients[i].inbuf.find('\n')) != std::string::npos) {
std::string line = clients[i].inbuf.substr(0, pos);
clients[i].inbuf.erase(0, pos + 1);
if (!line.empty() && line.back() == '\r') line.pop_back();
if (line.empty()) continue;
if (line == "/quit") {
close_client = true;
break;
} else if (line == "/who") {
std::string list = "Users:";
for (auto& c2 : clients) list += " " + c2.name;
list += "\n";
send_to(clients[i], list);
} else if (line.rfind("/nick "
, 0) == 0) {
std::string new_name = line.substr(6);
if (!valid_name(new_name)) {
send_to(clients[i],
"Error: invalid name\n");
} else {
bool taken = false;
for (size_t k = 0; k < clients.size(); ++k) {
if (k != i && clients[k].name == new_name) { taken = true; break; }
}
if (taken) {
send_to(clients[i],
"Error: that name is in use\n");
} else {
std::string old_name = clients[i].name;
clients[i].name = new_name;
broadcast(clients, old_name + " is now " + new_name + "\n");
}
}
} else if (line[0] == '/') {
send_to(clients[i],
"Error: unknown command\n");
} else {
broadcast(clients, clients[i].name + ": " + line + "\n", fd);
}
}
}
}
if (close_client) {
std::string leaving = clients[i].name;
clients.erase(clients.begin() + static_cast<long>(i));
broadcast(clients, leaving + " left\n");
std::cout << leaving << " disconnected\n";
continue;
}
++i;
}
}
} catch (const std::exception& e) {
std::cerr << "Error: " << e.what() << "\n";
return 1;
}
}