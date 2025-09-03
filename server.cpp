#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")
using namespace std;
#define PORT 54000
#define BUFFER_SIZE 1024

uint32_t crc32(const char* data, size_t length, uint32_t prev = 0) {
    uint32_t crc = ~prev;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int)(crc & 1)));
        }
    }
    return ~crc;
}

void handleDownload(SOCKET clientSocket) {
    ifstream file("testfile.txt", ios::binary);
    if (!file) {
        cerr << "[Server] Error: Could not open file for download.\n";
        return;
    }

    file.seekg(0, ios::end);
    uint64_t fileSize = file.tellg();
    file.seekg(0, ios::beg);

    uint32_t crc = 0;
    {
        vector<char> fileBuffer(fileSize);
        file.read(fileBuffer.data(), fileSize);
        crc = crc32(fileBuffer.data(), fileSize, 0);

        file.clear();
        file.seekg(0, ios::beg);
    }

    send(clientSocket, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);

    char buffer[BUFFER_SIZE];
    while (file) {
        file.read(buffer, sizeof(buffer));
        streamsize bytesRead = file.gcount();
        if (bytesRead > 0)
            send(clientSocket, buffer, static_cast<int>(bytesRead), 0);
    }

    send(clientSocket, reinterpret_cast<char*>(&crc), sizeof(crc), 0);
    cout << "[Server] File sent. CRC: " << crc << "\n";
}



void handleUpload(SOCKET clientSocket) {
    ofstream file("uploaded_from_client.txt", ios::binary);
    if (!file) {
        cerr << "[Server] Error: Could not open file for upload.\n";
        return;
    }

    uint64_t fileSize = 0;
    recv(clientSocket, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);

    char buffer[BUFFER_SIZE];
    uint32_t crc = 0;
    uint64_t totalReceived = 0;

    while (totalReceived < fileSize) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) break;
        file.write(buffer, bytesRead);
        crc = crc32(buffer, bytesRead, crc);
        totalReceived += bytesRead;
    }

    // Send CRC back to client
    send(clientSocket, reinterpret_cast<char*>(&crc), sizeof(crc), 0);
    cout << "[Server] Upload complete. CRC: " << crc << "\n";
}

void handleClient(SOCKET clientSocket) {
    char mode;
    int r = recv(clientSocket, &mode, 1, 0);
    if (r <= 0) {
        closesocket(clientSocket);
        return;
    }

    if (mode == 'D') {
        handleDownload(clientSocket);
    } else if (mode == 'U') {
        handleUpload(clientSocket);
    } else {
        cerr << "[Server] Unknown mode received: " << mode << "\n";
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);

    cout << "[Server] Waiting for clients...\n";

    vector<thread> threads;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) break;
        cout << "[Server] Client connected.\n";
        threads.emplace_back(handleClient, clientSocket);
    }

    for (auto& t : threads) t.join();

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
