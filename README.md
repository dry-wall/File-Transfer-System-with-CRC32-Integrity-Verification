# Reliable File Transfer System with CRC32 Integrity Verification

## Overview
This project implements a **client-server file transfer system** using TCP sockets on Windows.  
It supports:
- Uploading files from client → server  
- Downloading files from server → client  
- **CRC32 integrity verification** to detect corrupted or incomplete transfers  
- Robust handling of partial data transmission  

---

## Features
- **Two-way transfer** – Upload or Download files based on user input  
- **Integrity check using CRC32** – Ensures the file received matches the original  
- **Binary file support** – Works with text and binary files  
- **Robust buffer management** – Handles partial reads/writes over network  

---

## Prerequisites
- **Windows** environment with MinGW or similar compiler (`g++` with Winsock2 support)  
- Basic command-line knowledge  

---

## Compilation
```bash
g++ server.cpp -o server -lws2_32
g++ client.cpp -o client -lws2_32
