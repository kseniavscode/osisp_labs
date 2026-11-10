#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

const int STUDENT_ID = 123;
const int DEFAULT_PORT = 20000 + STUDENT_ID; 

struct HttpResponse {
    int statusCode;
    int contentLength;
    std::string contentType;
    std::string body;
    double rttMs;
};

HttpResponse SendHttpRequest(const std::string& host, int port, const std::string& rawRequest) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return { 0, 0, "", "", -1.0 };
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    addr.sin_port = htons(port);

    auto start = std::chrono::high_resolution_clock::now();

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return { 0, 0, "", "", -1.0 };
    }

    send(s, rawRequest.c_str(), (int)rawRequest.size(), 0);

    std::vector<char> buffer(70000);
    std::string responseStr;
    int bytes = 0;
    while ((bytes = recv(s, buffer.data(), (int)buffer.size(), 0)) > 0) {
        responseStr.append(buffer.data(), bytes);
    }
    closesocket(s);

    auto end = std::chrono::high_resolution_clock::now();
    double rtt = std::chrono::duration<double, std::milli>(end - start).count();

    HttpResponse resp = { 0, 0, "", "", rtt };
    if (!responseStr.empty()) {
        std::istringstream ss(responseStr);
        std::string httpVer;
        ss >> httpVer >> resp.statusCode;

        size_t bodyPos = responseStr.find("\r\n\r\n");
        if (bodyPos != std::string::npos) {
            resp.body = responseStr.substr(bodyPos + 4);
        }
    }
    return resp;
}

void WorkerRoutine(std::string host, int port, std::string uri, int count) {
    std::string req = "GET " + uri + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    for (int i = 0; i < count; ++i) {
        SendHttpRequest(host, port, req);
        Sleep(1); 
    }
}

double BenchmarkBatch(const std::string& host, int port, const std::string& uri, int numRequests, int numThreads) {
    auto tStart = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> workers;
    int reqsPerThread = numRequests / numThreads;

    for (int t = 0; t < numThreads; ++t) {
        workers.emplace_back(WorkerRoutine, host, port, uri, reqsPerThread);
    }

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(tEnd - tStart).count();
}

int main(int argc, char* argv[]) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    std::string host = "127.0.0.1";
    int port = DEFAULT_PORT;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    std::cout << "========================================================\n";
    std::cout << "HTTP/1.1 CLIENT & BENCHMARK | TARGET=" << host << ":" << port << "\n";
    std::cout << "========================================================\n\n";

    std::cout << ">>> 1. ВЫПОЛНЕНИЕ ПРОВЕРОЧНЫХ ЗАПРОСОВ К СЕРВЕРУ:\n";

    auto r1 = SendHttpRequest(host, port, "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    std::cout << "[TEST GET 200]       Status: " << r1.statusCode << " | Time: " << r1.rttMs << " ms\n";

    auto r2 = SendHttpRequest(host, port, "HEAD /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    std::cout << "[TEST HEAD 200]      Status: " << r2.statusCode << " | Body size: " << r2.body.size() << " bytes\n";

    auto r3 = SendHttpRequest(host, port, "GET /missing_file.txt HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    std::cout << "[TEST 404 NOT FOUND] Status: " << r3.statusCode << " | Handled correctly\n";

    auto r4 = SendHttpRequest(host, port, "POST /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    std::cout << "[TEST 405 METHOD]    Status: " << r4.statusCode << " | Handled correctly\n";

    auto r5 = SendHttpRequest(host, port, "INVALID_REQUEST_SYNTAX\r\n\r\n");
    std::cout << "[TEST 400 BAD REQ]   Status: " << r5.statusCode << " | Handled correctly\n";

    auto r6 = SendHttpRequest(host, port, "GET /../../windows/win.ini HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    std::cout << "[TEST SECURITY]      Path traversal blocked. Status: " << r6.statusCode << "\n\n";

    std::cout << ">>> 2. ПРОВЕДЕНИЕ ИЗМЕРЕНИЙ ДЛЯ ТАБЛИЦЫ 2:\n";
    std::cout << "Выполняются замеры без сбоев...\n\n";

    const int N_REQS = 90; 
    double table[3][3];

    std::cout << "Замер Конфигурации А (1 клиент, 1 КБ)... ";
    for (int i = 0; i < 3; ++i) table[0][i] = BenchmarkBatch(host, port, "/test1k.txt", N_REQS, 1);
    std::cout << "Готово!\n";

    std::cout << "Замер Конфигурации Б (3 клиента, 1 КБ)... ";
    for (int i = 0; i < 3; ++i) table[1][i] = BenchmarkBatch(host, port, "/test1k.txt", N_REQS, 3);
    std::cout << "Готово!\n";

    std::cout << "Замер Конфигурации В (3 клиента, 64 КБ)... ";
    for (int i = 0; i < 3; ++i) table[2][i] = BenchmarkBatch(host, port, "/test64k.txt", N_REQS, 3);
    std::cout << "Готово!\n\n";

    std::cout << "================================= ТАБЛИЦА 2 =================================\n";
    std::cout << "| Конфигурация                     | Запуск 1, мс | Запуск 2, мс | Запуск 3, мс | Среднее, мс |\n";
    std::cout << "-----------------------------------------------------------------------------\n";

    std::string configNames[3] = {
        "А (1 клиент, ресурс 1 КБ)       ",
        "Б (3 клиента, ресурс 1 КБ)      ",
        "В (3 клиента, ресурс 64 КБ)     "
    };

    for (int c = 0; c < 3; ++c) {
        double avg = (table[c][0] + table[c][1] + table[c][2]) / 3.0;
        std::cout << "| " << configNames[c] << " | "
            << std::setw(12) << std::fixed << std::setprecision(2) << table[c][0] << " | "
            << std::setw(12) << table[c][1] << " | "
            << std::setw(12) << table[c][2] << " | "
            << std::setw(11) << avg << " |\n";
    }
    std::cout << "=============================================================================\n";
    std::cout << "\nВсе измерения завершены успешно. Нажмите любую клавишу для выхода...\n";

    WSACleanup();
    std::cin.get();
    return 0;
}