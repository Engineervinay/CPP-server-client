#include <iostream>
#include <fstream>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include "json.hpp"

using json = nlohmann::json;

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 3000
#define BUFFER_SIZE 17

struct Packet {
    std::string symbol;
    char buySell;
    int quantity;
    int price;
    int sequence;
};

int createSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) return -1;
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

void sendRequest(int sock, uint8_t callType, uint8_t resendSeq = 0) {
    uint8_t request[2] = {callType, resendSeq};
    send(sock, request, sizeof(request), 0);
}

Packet parsePacket(const uint8_t* buffer) {
    Packet p;
    p.symbol = std::string(buffer, buffer + 4);
    p.buySell = buffer[4];
    p.quantity = ntohl(*(int*)(buffer + 5));
    p.price = ntohl(*(int*)(buffer + 9));
    p.sequence = ntohl(*(int*)(buffer + 13));
    return p;
}

void saveToJson(const std::vector<Packet>& packets) {
    json output;
    for (const auto& p : packets) {
        output.push_back({
            {"symbol", p.symbol},
            {"buySell", std::string(1, p.buySell)},
            {"quantity", p.quantity},
            {"price", p.price},
            {"sequence", p.sequence}
        });
    }
    std::ofstream file("output.json");
    file << output.dump(4);
}

void requestStream() {
    int sock = createSocket();
    if (sock < 0) return;
    sendRequest(sock, 1);
    std::vector<Packet> packets;
    uint8_t buffer[BUFFER_SIZE];
    while (recv(sock, buffer, BUFFER_SIZE, 0) == BUFFER_SIZE) {
        packets.push_back(parsePacket(buffer));
    }
    close(sock);
    saveToJson(packets);
}

void requestMissingPackets(const std::vector<int>& missingSeq) {
    for (int seq : missingSeq) {
        int sock = createSocket();
        if (sock < 0) continue;
        sendRequest(sock, 2, seq);
        uint8_t buffer[BUFFER_SIZE];
        if (recv(sock, buffer, BUFFER_SIZE, 0) == BUFFER_SIZE) {
            std::vector<Packet> packets = {parsePacket(buffer)};
            saveToJson(packets);
        }
        close(sock);
    }
}

int main() {
    requestStream();
    return 0;
}
