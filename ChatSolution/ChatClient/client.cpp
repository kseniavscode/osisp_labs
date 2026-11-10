#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <iomanip>
#include <limits>
#include <chrono>
#include <vector>

#define PIPE_NAME L"\\\\.\\pipe\\PersonalChatPipe"

const int VARIANT_ID = 27;

enum MessageType {
    MSG_HELLO = 1,
    MSG_HELLO_OK = 2,
    MSG_CHAT = 3,
    MSG_ERROR = 4,
    MSG_STOP = 5,
    MSG_PERF = 6
};

struct ChatMessage {
    uint32_t version;
    uint32_t messageType;
    uint32_t sourcePid;
    uint32_t requestId;
    uint32_t payloadSize;
    char sender[32];
    char target[32];
    char text[4096];
};

HANDLE hPipe = INVALID_HANDLE_VALUE;
bool isRunning = true;
bool isBenchmarking = false; 

void LogEvent(const std::string& action, DWORD pid, uint32_t reqId, uint32_t type, const std::string& extra) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::cout << "[" << std::setfill('0') << std::setw(2) << st.wHour << ":"
        << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond << "."
        << std::setw(3) << st.wMilliseconds << "] "
        << "PID=" << pid << " | " << action
        << " | reqId=" << reqId << " | type=" << type
        << " | " << extra << std::endl;
}

void ReceiveMessagesThread() {
    while (isRunning) {
        if (!isBenchmarking) { 
            DWORD bytesAvail = 0;
            if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail >= sizeof(ChatMessage)) {
                ChatMessage msg;
                DWORD bytesRead = 0;
                if (ReadFile(hPipe, &msg, sizeof(ChatMessage), &bytesRead, NULL) && bytesRead > 0) {
                    if (msg.messageType == MSG_CHAT) {
                        std::cout << "\n\n========================================";
                        std::cout << "\n[ВХОДЯЩЕЕ СООБЩЕНИЕ ОТ: " << msg.sender << "]";
                        std::cout << "\nТекст: " << msg.text;
                        std::cout << "\n========================================\n";
                        std::cout << "Введите получателя (или 'STOP', 'PERF'): " << std::flush;
                    }
                    else if (msg.messageType == MSG_ERROR) {
                        std::cout << "\n\n[ОШИБКА ОТ СЕРВЕРА]: " << msg.text << "\n";
                        std::cout << "Введите получателя (или 'STOP', 'PERF'): " << std::flush;
                    }
                }
            }
        }
        Sleep(20);
    }
}

void RunBenchmark() {
    isBenchmarking = true;
    const int count = 1000;
    ChatMessage testMsg = {};
    testMsg.version = 1;
    testMsg.messageType = MSG_PERF;
    testMsg.sourcePid = GetCurrentProcessId();

    std::cout << "\n>>> Запуск замера производительности на 1000 сообщений...\n";

    testMsg.payloadSize = 64;
    memset(testMsg.text, 'A', 64);
    testMsg.text[64] = '\0';

    ChatMessage recvMsg;
    DWORD rw = 0;

    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < count; ++i) {
        WriteFile(hPipe, &testMsg, sizeof(ChatMessage), &rw, NULL);
        ReadFile(hPipe, &recvMsg, sizeof(ChatMessage), &rw, NULL);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    double time1 = std::chrono::duration<double, std::milli>(end1 - start1).count();
    double speed1 = count / (time1 / 1000.0);

    testMsg.payloadSize = 4096;
    memset(testMsg.text, 'B', 4095);
    testMsg.text[4095] = '\0';

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < count; ++i) {
        WriteFile(hPipe, &testMsg, sizeof(ChatMessage), &rw, NULL);
        ReadFile(hPipe, &recvMsg, sizeof(ChatMessage), &rw, NULL);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    double time2 = std::chrono::duration<double, std::milli>(end2 - start2).count();
    double speed2 = count / (time2 / 1000.0);

    std::cout << "\n================== ТАБЛИЦА 2 (ЭКСПЕРИМЕНТ) ==================\n";
    std::cout << "| Тест | Размер сообщения | Количество | Время (мс) | Сообщений/с |\n";
    std::cout << "-------------------------------------------------------------\n";
    std::cout << "| 1    | 64 байта         | 1000       | "
        << std::setw(10) << std::fixed << std::setprecision(2) << time1 << " | "
        << std::setw(11) << (int)speed1 << " |\n";
    std::cout << "| 2    | >= 4 КБ          | 1000       | "
        << std::setw(10) << std::fixed << std::setprecision(2) << time2 << " | "
        << std::setw(11) << (int)speed2 << " |\n";
    std::cout << "=============================================================\n\n";

    isBenchmarking = false;
}

int main(int argc, char* argv[]) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    if (argc < 3) {
        std::cout << "Использование: client.exe <ИмяКлиента> <RUN_ID>\n";
        return 1;
    }

    std::string clientName = argv[1];
    int runId = std::stoi(argv[2]);
    DWORD pid = GetCurrentProcessId();

    std::cout << "CLIENT " << clientName
        << " | PID=" << pid
        << " | VARIANT=" << VARIANT_ID
        << " | RUN_ID=" << runId << "\n";

    while (true) {
        hPipe = CreateFileW(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL,
            OPEN_EXISTING,
            0, NULL
        );
        if (hPipe != INVALID_HANDLE_VALUE) break;

        if (GetLastError() != ERROR_PIPE_BUSY) {
            std::cerr << "[ERROR] Сервер недоступен. Код: " << GetLastError() << "\n";
            return 1;
        }
        WaitNamedPipeW(PIPE_NAME, 500);
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);

    ChatMessage helloMsg = {};
    helloMsg.version = 1;
    helloMsg.messageType = MSG_HELLO;
    helloMsg.sourcePid = pid;
    helloMsg.requestId = 1;
    strcpy_s(helloMsg.sender, clientName.c_str());

    DWORD bytesWritten = 0, bytesRead = 0;
    WriteFile(hPipe, &helloMsg, sizeof(ChatMessage), &bytesWritten, NULL);
    LogEvent("SEND", pid, 1, MSG_HELLO, "Регистрация: " + clientName);

    ChatMessage resp = {};
    ReadFile(hPipe, &resp, sizeof(ChatMessage), &bytesRead, NULL);
    std::cout << "[CLIENT] Успешно зарегистрирован в чате!\n";
    std::cout << "Подсказка: введите 'PERF' для замера скорости на 1000 сообщений.\n\n";

    std::thread rxThread(ReceiveMessagesThread);
    rxThread.detach();

    uint32_t reqId = 2;
    std::string target, text;

    while (true) {
        std::cout << "Введите получателя (или 'STOP', 'PERF'): ";
        std::cin >> target;

        if (target == "STOP") {
            isRunning = false;
            ChatMessage stopMsg = {};
            stopMsg.version = 1;
            stopMsg.messageType = MSG_STOP;
            stopMsg.sourcePid = pid;
            stopMsg.requestId = reqId;
            strcpy_s(stopMsg.sender, clientName.c_str());

            WriteFile(hPipe, &stopMsg, sizeof(ChatMessage), &bytesWritten, NULL);
            break;
        }

        if (target == "PERF") {
            RunBenchmark();
            continue;
        }

        std::cout << "Введите текст сообщения: ";
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        std::getline(std::cin, text);

        ChatMessage chatMsg = {};
        chatMsg.version = 1;
        chatMsg.messageType = MSG_CHAT;
        chatMsg.sourcePid = pid;
        chatMsg.requestId = reqId;
        chatMsg.payloadSize = (uint32_t)text.length();
        strcpy_s(chatMsg.sender, clientName.c_str());
        strcpy_s(chatMsg.target, target.c_str());
        strcpy_s(chatMsg.text, text.c_str());

        BOOL sent = WriteFile(hPipe, &chatMsg, sizeof(ChatMessage), &bytesWritten, NULL);
        if (sent) {
            LogEvent("SEND", pid, reqId, MSG_CHAT, "Кому=" + target + " Текст=" + text);
            std::cout << "[УСПЕХ] Отправлено серверу.\n\n";
        }
        else {
            std::cerr << "[ОШИБКА] Код: " << GetLastError() << "\n\n";
            break;
        }

        reqId++;
    }

    CloseHandle(hPipe);
    return 0;
}