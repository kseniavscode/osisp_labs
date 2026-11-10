#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <thread>
#include <filesystem>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace fs = std::filesystem;

const int STUDENT_ID = 123;
const int DEFAULT_PORT = 20000 + STUDENT_ID; 
const int MAX_HEADER_SIZE = 8192;           
const std::string ROOT_DIR = "www";

void LogEvent(const std::string& action, const std::string& details) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::cout << "[" << std::setfill('0') << std::setw(2) << st.wHour << ":"
        << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond << "."
        << std::setw(3) << st.wMilliseconds << "] "
        << std::setw(15) << std::left << action << " | " << details << std::endl;
}

std::string UrlDecode(const std::string& src) {
    std::string ret;
    char ch;
    int i, ii;
    for (i = 0; i < (int)src.length(); i++) {
        if (src[i] == '%') {
            if (i + 2 < (int)src.length()) {
                std::istringstream iss(src.substr(i + 1, 2));
                if (iss >> std::hex >> ii) {
                    ch = static_cast<char>(ii);
                    ret += ch;
                    i += 2;
                }
                else {
                    ret += src[i];
                }
            }
        }
        else if (src[i] == '+') {
            ret += ' ';
        }
        else {
            ret += src[i];
        }
    }
    return ret;
}

bool IsSafePath(const std::string& rawPath, fs::path& outSafePath) {
    std::string decoded = UrlDecode(rawPath);
    if (decoded.find("..") != std::string::npos) return false;

    size_t qPos = decoded.find('?');
    if (qPos != std::string::npos) decoded = decoded.substr(0, qPos);

    if (decoded.empty() || decoded == "/") decoded = "/index.html";
    if (decoded.front() == '/') decoded = decoded.substr(1);

    fs::path fullRoot = fs::absolute(ROOT_DIR);
    fs::path target = fs::absolute(fullRoot / decoded);

    std::string rootStr = fullRoot.lexically_normal().string();
    std::string targetStr = target.lexically_normal().string();

    if (targetStr.rfind(rootStr, 0) != 0) {
        return false; 
    }

    outSafePath = target;
    return true;
}

std::string GetMimeType(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

void SendHttpResponse(SOCKET clientSocket, int statusCode, const std::string& statusText,
    const std::string& contentType, const std::string& body, bool sendBody) {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    resp << "Server: SimpleHttpServer/1.1 (Variant 27)\r\n";
    resp << "Connection: close\r\n";
    resp << "Content-Type: " << contentType << "\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "\r\n";

    std::string headerStr = resp.str();
    send(clientSocket, headerStr.c_str(), (int)headerStr.size(), 0);

    if (sendBody && !body.empty()) {
        send(clientSocket, body.c_str(), (int)body.size(), 0);
    }
}

void HandleClient(SOCKET clientSocket, std::string clientIp, int clientPort) {
    std::vector<char> buffer(MAX_HEADER_SIZE);
    int totalReceived = 0;
    bool headersComplete = false;

    while (totalReceived < MAX_HEADER_SIZE) {
        int bytes = recv(clientSocket, buffer.data() + totalReceived, MAX_HEADER_SIZE - totalReceived, 0);
        if (bytes <= 0) break;
        totalReceived += bytes;

        std::string cur(buffer.data(), totalReceived);
        if (cur.find("\r\n\r\n") != std::string::npos) {
            headersComplete = true;
            break;
        }
    }

    if (!headersComplete) {
        LogEvent("HTTP_ERROR", clientIp + " - 400 Bad Request (Headers too large or incomplete)");
        SendHttpResponse(clientSocket, 400, "Bad Request", "text/plain", "400 Bad Request: Malformed headers\n", true);
        closesocket(clientSocket);
        return;
    }

    std::string requestStr(buffer.data(), totalReceived);
    std::istringstream reqStream(requestStr);
    std::string method, rawUri, protocol;
    reqStream >> method >> rawUri >> protocol;

    LogEvent("HTTP_REQ", clientIp + ":" + std::to_string(clientPort) + " -> " + method + " " + rawUri);

    if (method.empty() || rawUri.empty() || protocol.empty() || protocol.rfind("HTTP/", 0) != 0) {
        SendHttpResponse(clientSocket, 400, "Bad Request", "text/plain", "400 Bad Request\n", true);
        closesocket(clientSocket);
        return;
    }

    if (method != "GET" && method != "HEAD") {
        LogEvent("HTTP_RESP", "405 Method Not Allowed: " + method);
        SendHttpResponse(clientSocket, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed\n", true);
        closesocket(clientSocket);
        return;
    }

    fs::path safeFilePath;
    if (!IsSafePath(rawUri, safeFilePath)) {
        LogEvent("SECURITY_BLOCK", "Directory Traversal attack prevented: " + rawUri);
        SendHttpResponse(clientSocket, 404, "Not Found", "text/plain", "404 Not Found\n", method == "GET");
        closesocket(clientSocket);
        return;
    }

    if (!fs::exists(safeFilePath) || fs::is_directory(safeFilePath)) {
        LogEvent("HTTP_RESP", "404 Not Found: " + safeFilePath.filename().string());
        SendHttpResponse(clientSocket, 404, "Not Found", "text/plain", "404 Not Found\n", method == "GET");
        closesocket(clientSocket);
        return;
    }

    std::ifstream file(safeFilePath, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string mime = GetMimeType(safeFilePath);

    LogEvent("HTTP_RESP", "200 OK (" + std::to_string(body.size()) + " bytes, " + mime + ")");
    SendHttpResponse(clientSocket, 200, "OK", mime, body, method == "GET");

    closesocket(clientSocket);
}

void PrepareEnvironment() {
    if (!fs::exists(ROOT_DIR)) {
        fs::create_directories(ROOT_DIR);
    }
    std::ofstream html(ROOT_DIR + "/index.html");
    html << "<!DOCTYPE html><html><head><title>OS Lab 5</title></head>"
        << "<body><h1>HTTP/1.1 Server Ready (Variant 27)</h1><p>Student ID: 123</p></body></html>";
    html.close();

    std::ofstream txt1(ROOT_DIR + "/test1k.txt");
    txt1 << std::string(1024, 'A');
    txt1.close();

    std::ofstream txt64(ROOT_DIR + "/test64k.txt");
    txt64 << std::string(65536, 'B');
    txt64.close();
}

int main(int argc, char* argv[]) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    int port = DEFAULT_PORT;
    if (argc >= 3 && std::string(argv[1]) == "--port") {
        port = std::stoi(argv[2]);
    }

    PrepareEnvironment();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed on port " << port << ". Code: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "========================================================\n";
    std::cout << "HTTP/1.1 STATIC SERVER | VARIANT=27 | STUDENT_ID=" << STUDENT_ID << "\n";
    std::cout << "Listening on http://127.0.0.1:" << port << " (Root dir: ./" << ROOT_DIR << ")\n";
    std::cout << "Supported methods: GET, HEAD\n";
    std::cout << "========================================================\n\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) break;

        std::string clientIp = inet_ntoa(clientAddr.sin_addr);
        int clientPort = ntohs(clientAddr.sin_port);

        std::thread(HandleClient, clientSocket, clientIp, clientPort).detach();
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}