#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")
using namespace std;
const char* SERVER_IP = "127.0.0.1";
const int PORT = 54000;
const int BUFFER_SIZE = 4096;

uint32_t crc32(const char* data, size_t length, uint32_t prev = 0) {
    uint32_t crc = ~prev;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void downloadFile(SOCKET sock) {
    uint64_t fileSize = 0;
    int sizeBytesReceived = 0;
    while (sizeBytesReceived < sizeof(fileSize)) {
        int result = recv(sock, reinterpret_cast<char*>(&fileSize) + sizeBytesReceived, sizeof(fileSize) - sizeBytesReceived, 0);
        if (result <= 0) {
            cerr << "[Client] Failed to receive file size.\n";
            return;
        }
        sizeBytesReceived += result;
    }

    cout << "[Client] Expecting file size: " << fileSize << " bytes\n";

    ofstream outFile("received.txt", ios::binary);
    char buffer[BUFFER_SIZE];
    uint32_t crc = 0;
    uint64_t totalReceived = 0;

    while (totalReceived < fileSize) {
        uint64_t remainingBytes = fileSize - totalReceived;
        int bytesToRead = static_cast<int>(min((uint64_t)sizeof(buffer), remainingBytes));
        
        int bytesRead = recv(sock, buffer, bytesToRead, 0);

        if (bytesRead <= 0) {
            cerr << "[Client] Connection lost during download.\n";
            break;
        }
        outFile.write(buffer, bytesRead);
        crc = crc32(buffer, bytesRead, crc);
        totalReceived += bytesRead;
    }

    if (totalReceived == fileSize) {
        uint32_t receivedCRC = 0;
        int crcBytesReceived = 0;
        while (crcBytesReceived < sizeof(receivedCRC)) {
             int result = recv(sock, reinterpret_cast<char*>(&receivedCRC) + crcBytesReceived, sizeof(receivedCRC) - crcBytesReceived, 0);
             if (result <= 0) {
                cerr << "[Client] Failed to receive CRC.\n";
                break;
             }
             crcBytesReceived += result;
        }

        if (crcBytesReceived == sizeof(receivedCRC)) {
            cout << "[Client] Transfer complete. Received CRC: "
                      << receivedCRC << " | Computed CRC: " << crc << "\n";

            if (receivedCRC == crc) {
                cout << "[Client] Integrity verified!\n";
            } else {
                cout << "[Client] Integrity check failed!\n";
            }
        }
    } else {
        cerr << "[Client] Download incomplete. Received " << totalReceived << " of " << fileSize << " bytes.\n";
    }

    outFile.close();
}

void uploadFile(SOCKET sock) {
    ifstream file("upload.txt", ios::binary);
    if (!file.is_open()) {
        cerr << "[Client] No file 'upload.txt' found to upload.\n";
        return;
    }

    file.seekg(0, ios::end);
    uint64_t fileSize = file.tellg();
    file.seekg(0, ios::beg);

    send(sock, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);

    char buffer[BUFFER_SIZE];
    uint32_t crc = 0;

    while (file) {
        file.read(buffer, sizeof(buffer));
        streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            crc = crc32(buffer, bytesRead, crc);
            send(sock, buffer, static_cast<int>(bytesRead), 0);
        }
    }

    uint32_t serverCRC;
    recv(sock, reinterpret_cast<char*>(&serverCRC), sizeof(serverCRC), 0);

    cout << "[Client] Upload complete. Sent CRC: " << crc
              << " | Server CRC: " << serverCRC << "\n";

    if (serverCRC == crc) {
        cout << "[Client] Upload integrity verified!\n";
    } else {
        cout << "[Client] Upload integrity mismatch!\n";
    }

    file.close();
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[Client] Connection failed.\n";
        return 1;
    }

    cout << "Enter mode (D = Download, U = Upload): ";
    char mode;
    cin >> mode;
    send(sock, &mode, 1, 0);

    if (mode == 'D') {
        downloadFile(sock);
    } else if (mode == 'U') {
        uploadFile(sock);
    } else {
        cout << "[Client] Invalid mode.\n";
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
