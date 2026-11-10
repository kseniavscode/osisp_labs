#include <windows.h>
#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <iomanip>

#define PIPE_NAME L"\\\\.\\pipe\\PersonalChatPipe"

const int VARIANT_ID = 27;
const int STUDENT_ID = 123;

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

std::map<std::string, HANDLE> clientsMap;
CRITICAL_SECTION csClients;

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

void HandleClient(HANDLE hPipe) {
    std::string clientName = "";
    bool isConnected = true;

    while (isConnected) {
        DWORD bytesAvail = 0;
        if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail >= sizeof(ChatMessage)) {
            ChatMessage msg;
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(hPipe, &msg, sizeof(ChatMessage), &bytesRead, NULL);
            if (!ok || bytesRead == 0) {
                break;
            }

            if (msg.messageType == MSG_PERF) {
                DWORD bytesWritten = 0;
                WriteFile(hPipe, &msg, sizeof(ChatMessage), &bytesWritten, NULL);
                continue;
            }

            LogEvent("RECEIVE", msg.sourcePid, msg.requestId, msg.messageType,
                std::string("From=") + msg.sender + " Target=" + msg.target);

            if (msg.messageType == MSG_HELLO) {
                clientName = msg.sender;
                EnterCriticalSection(&csClients);
                clientsMap[clientName] = hPipe;
                LeaveCriticalSection(&csClients);

                std::cout << "[SERVER] Клиент зарегистрирован: " << clientName << std::endl;

                ChatMessage resp = {};
                resp.version = 1;
                resp.messageType = MSG_HELLO_OK;
                resp.sourcePid = GetCurrentProcessId();
                resp.requestId = msg.requestId;
                strcpy_s(resp.sender, "SERVER");

                DWORD bytesWritten;
                WriteFile(hPipe, &resp, sizeof(ChatMessage), &bytesWritten, NULL);
            }

            else if (msg.messageType == MSG_CHAT) {
                std::string targetUser = msg.target;
                std::cout << "[CHAT ROUTE] От [" << msg.sender << "] к [" << targetUser << "]: " << msg.text << std::endl;

                EnterCriticalSection(&csClients);
                if (clientsMap.find(targetUser) != clientsMap.end()) {
                    HANDLE hTargetPipe = clientsMap[targetUser];
                    DWORD bytesWritten;
                    WriteFile(hTargetPipe, &msg, sizeof(ChatMessage), &bytesWritten, NULL);
                    std::cout << "[SERVER] Доставлено пользователю " << targetUser << std::endl;
                    LogEvent("SEND", GetCurrentProcessId(), msg.requestId, MSG_CHAT, "Forwarded to " + targetUser);
                }
                else {
                    ChatMessage errResp = {};
                    errResp.version = 1;
                    errResp.messageType = MSG_ERROR;
                    errResp.sourcePid = GetCurrentProcessId();
                    errResp.requestId = msg.requestId;
                    strcpy_s(errResp.sender, "SERVER");
                    std::string errText = "Пользователь '" + targetUser + "' не найден!";
                    strcpy_s(errResp.text, errText.c_str());

                    DWORD bytesWritten;
                    WriteFile(hPipe, &errResp, sizeof(ChatMessage), &bytesWritten, NULL);
                    std::cout << "[SERVER WARNING] Адресат не найден: " << targetUser << std::endl;
                }
                LeaveCriticalSection(&csClients);
            }
            else if (msg.messageType == MSG_STOP) {
                std::cout << "[SERVER] Клиент " << clientName << " отключился.\n";
                break;
            }
        }
        else {
            Sleep(10);
        }
    }

    if (!clientName.empty()) {
        EnterCriticalSection(&csClients);
        clientsMap.erase(clientName);
        LeaveCriticalSection(&csClients);
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}

int main(int argc, char* argv[]) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    int runId = (argc > 1) ? std::stoi(argv[1]) : 1;

    std::cout << "SERVER | PID=" << GetCurrentProcessId()
        << " | VARIANT=" << VARIANT_ID
        << " | STUDENT_ID=" << STUDENT_ID
        << " | RUN_ID=" << runId << "\n";
    std::cout << "Сервер ожидает клиентов...\n\n";

    InitializeCriticalSection(&csClients);

    while (true) {
        HANDLE hPipe = CreateNamedPipeW(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            sizeof(ChatMessage) * 8,
            sizeof(ChatMessage) * 8,
            0, NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "[CRITICAL] CreateNamedPipe failed.\n";
            break;
        }

        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            std::thread(HandleClient, hPipe).detach();
        }
        else {
            CloseHandle(hPipe);
        }
    }

    DeleteCriticalSection(&csClients);
    return 0;
}