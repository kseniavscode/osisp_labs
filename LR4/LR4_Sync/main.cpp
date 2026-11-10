#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <random>
#include <atomic>

const int VARIANT_ID = 27;
const int STUDENT_ID = 123;
const unsigned int SEED = 1000 + STUDENT_ID * 97; 
const int BASE_OPS_PER_PRODUCER = 500;            
const int INDIVIDUAL_OPS_PER_PRODUCER = 500 + STUDENT_ID * 5; 

const int NUM_PRODUCERS = 2;
const int NUM_CONSUMERS = 2;
const int MAX_QUEUE_CAPACITY = 128; 


struct QueueItem {
    int producerId;
    int itemId;
    int payload;
};
struct SharedQueue {
    int capacity;
    int head;
    int tail; 
    int count; 
    QueueItem buffer[MAX_QUEUE_CAPACITY];
};

HANDLE hMapFile = NULL;
SharedQueue* pQueue = NULL;
HANDLE hMutex = NULL;
HANDLE hSemEmpty = NULL; 
HANDLE hSemFull = NULL; 
HANDLE hConsoleMutex = NULL;

std::atomic<int> g_totalProduced{ 0 };
std::atomic<int> g_totalConsumed{ 0 };
std::atomic<int> g_invariantViolations{ 0 };
std::atomic<long long> g_totalWaitTimeMs{ 0 }; 
bool g_verboseLog = false;

std::chrono::high_resolution_clock::time_point g_startTime;

void LogEvent(const std::string& participant, DWORD tid, const std::string& state, const std::string& info = "") {
    if (!g_verboseLog) return;

    WaitForSingleObject(hConsoleMutex, INFINITE);
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime).count();

    std::cout << "[" << std::setfill('0') << std::setw(6) << elapsedMs << " ms] "
        << participant << " | TID=" << std::setw(5) << tid
        << " | STATE=" << std::setw(9) << std::left << state;
    if (!info.empty()) {
        std::cout << " | " << info;
    }
    std::cout << std::endl;
    ReleaseMutex(hConsoleMutex);
}

DWORD WINAPI ProducerThread(LPVOID lpParam) {
    int pId = *reinterpret_cast<int*>(lpParam);
    DWORD tid = GetCurrentThreadId();
    int ops = (pQueue->capacity == 4 || pQueue->capacity == 16) ? BASE_OPS_PER_PRODUCER : INDIVIDUAL_OPS_PER_PRODUCER;

    std::mt19937 rng(SEED + pId * 100);
    std::uniform_int_distribution<int> dist(1, 100);

    for (int i = 0; i < ops; ++i) {
        LogEvent("PRODUCER " + std::to_string(pId), tid, "WAITING", "Waiting for EMPTY slot");

        auto t1 = std::chrono::high_resolution_clock::now();
        WaitForSingleObject(hSemEmpty, INFINITE);
        WaitForSingleObject(hMutex, INFINITE);
        auto t2 = std::chrono::high_resolution_clock::now();
        g_totalWaitTimeMs += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        LogEvent("PRODUCER " + std::to_string(pId), tid, "PRODUCING", "Acquired lock. Adding item #" + std::to_string(i));

        if (pQueue->count >= pQueue->capacity) {
            g_invariantViolations++;
        }

        pQueue->buffer[pQueue->tail] = { pId, i, dist(rng) };
        pQueue->tail = (pQueue->tail + 1) % pQueue->capacity;
        pQueue->count++;
        g_totalProduced++;

        ReleaseMutex(hMutex);
        ReleaseSemaphore(hSemFull, 1, NULL);

        LogEvent("PRODUCER " + std::to_string(pId), tid, "RELEASED", "Item added. Slot signaled");
    }

    LogEvent("PRODUCER " + std::to_string(pId), tid, "FINISHED");
    return 0;
}

DWORD WINAPI ConsumerThread(LPVOID lpParam) {
    int cId = *reinterpret_cast<int*>(lpParam);
    DWORD tid = GetCurrentThreadId();
    int ops = (pQueue->capacity == 4 || pQueue->capacity == 16) ? BASE_OPS_PER_PRODUCER : INDIVIDUAL_OPS_PER_PRODUCER;

    for (int i = 0; i < ops; ++i) {
        LogEvent("CONSUMER " + std::to_string(cId), tid, "WAITING", "Waiting for FULL slot");

        auto t1 = std::chrono::high_resolution_clock::now();
        WaitForSingleObject(hSemFull, INFINITE);
        WaitForSingleObject(hMutex, INFINITE);
        auto t2 = std::chrono::high_resolution_clock::now();
        g_totalWaitTimeMs += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        LogEvent("CONSUMER " + std::to_string(cId), tid, "CONSUMING", "Acquired lock. Extracting item");

        if (pQueue->count <= 0) {
            g_invariantViolations++;
        }

        QueueItem item = pQueue->buffer[pQueue->head];
        pQueue->head = (pQueue->head + 1) % pQueue->capacity;
        pQueue->count--;
        g_totalConsumed++;

        ReleaseMutex(hMutex);
        ReleaseSemaphore(hSemEmpty, 1, NULL);

        LogEvent("CONSUMER " + std::to_string(cId), tid, "RELEASED",
            "Consumed P" + std::to_string(item.producerId) + "-Item#" + std::to_string(item.itemId));
    }

    LogEvent("CONSUMER " + std::to_string(cId), tid, "FINISHED");
    return 0;
}

bool InitIPC(int capacity) {
    hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedQueue), "Local\\OS_Lab4_SharedQueue");
    if (!hMapFile) return false;

    pQueue = reinterpret_cast<SharedQueue*>(MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedQueue)));
    if (!pQueue) return false;

    pQueue->capacity = capacity;
    pQueue->head = 0;
    pQueue->tail = 0;
    pQueue->count = 0;

    hMutex = CreateMutexA(NULL, FALSE, "Local\\OS_Lab4_Mutex");
    hSemEmpty = CreateSemaphoreA(NULL, capacity, capacity, "Local\\OS_Lab4_SemEmpty");
    hSemFull = CreateSemaphoreA(NULL, 0, capacity, "Local\\OS_Lab4_SemFull");
    hConsoleMutex = CreateMutexA(NULL, FALSE, NULL);

    return (hMutex && hSemEmpty && hSemFull && hConsoleMutex);
}

void CleanupIPC() {
    if (pQueue) UnmapViewOfFile(pQueue);
    if (hMapFile) CloseHandle(hMapFile);
    if (hMutex) CloseHandle(hMutex);
    if (hSemEmpty) CloseHandle(hSemEmpty);
    if (hSemFull) CloseHandle(hSemFull);
    if (hConsoleMutex) CloseHandle(hConsoleMutex);
}

double RunSimulation(int capacity, bool verbose) {
    g_verboseLog = verbose;
    g_totalProduced = 0;
    g_totalConsumed = 0;
    g_invariantViolations = 0;
    g_totalWaitTimeMs = 0;

    InitIPC(capacity);

    g_startTime = std::chrono::high_resolution_clock::now();

    HANDLE threads[NUM_PRODUCERS + NUM_CONSUMERS];
    int pIds[NUM_PRODUCERS] = { 1, 2 };
    int cIds[NUM_CONSUMERS] = { 1, 2 };

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        threads[i] = CreateThread(NULL, 0, ProducerThread, &pIds[i], 0, NULL);
    }
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        threads[NUM_PRODUCERS + i] = CreateThread(NULL, 0, ConsumerThread, &cIds[i], 0, NULL);
    }

    WaitForMultipleObjects(NUM_PRODUCERS + NUM_CONSUMERS, threads, TRUE, 30000);

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - g_startTime).count();

    for (int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; ++i) {
        CloseHandle(threads[i]);
    }

    CleanupIPC();
    return elapsedMs;
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    std::cout << "===============================================================\n";
    std::cout << "ЛАБОРАТОРНАЯ РАБОТА №4: СИНХРОНИЗАЦИЯ ПОТОКОВ (ВАРИАНТ 27)\n";
    std::cout << "Модель: Кольцевая очередь (Shared Memory, Semaphore, Mutex)\n";
    std::cout << "Параметры: 2 Producers, 2 Consumers | STUDENT_ID=" << STUDENT_ID << "\n";
    std::cout << "===============================================================\n\n";

    std::cout << ">>> 1. ДЕМОНСТРАЦИОННЫЙ ЗАПУСК С ПОДРОБНЫМ ЖУРНАЛОМ (Capacity=16)\n";
    std::cout << "Нажмите Enter для старта демо-режима...";
    std::cin.get();

    double demoTime = RunSimulation(16, true);
    std::cout << "\n[ИТОГ ДЕМО]: Произведено: " << g_totalProduced
        << " | Потреблено: " << g_totalConsumed
        << " | Нарушений инвариантов: " << g_invariantViolations
        << " | Время: " << demoTime << " мс\n\n";

    std::cout << ">>> 2. ПРОВЕДЕНИЕ ЭКСПЕРИМЕНТОВ ДЛЯ ТАБЛИЦЫ 2\n";
    std::cout << "Выполняются замеры без вывода логов в консоль...\n\n";

    double results[3][3]; 

    std::cout << "Тестирование Конфигурации А (Емкость 4)... ";
    for (int i = 0; i < 3; ++i) results[0][i] = RunSimulation(4, false);
    std::cout << "Готово!\n";

    std::cout << "Тестирование Конфигурации Б (Емкость 16)... ";
    for (int i = 0; i < 3; ++i) results[1][i] = RunSimulation(16, false);
    std::cout << "Готово!\n";

    std::cout << "Тестирование Конфигурации В (Индивидуальная нагрузка)... ";
    for (int i = 0; i < 3; ++i) results[2][i] = RunSimulation(16, false);
    std::cout << "Готово!\n\n";

    std::cout << "========================== ТАБЛИЦА 2 ==========================\n";
    std::cout << "| Конфигурация        | Запуск 1, мс | Запуск 2, мс | Запуск 3, мс | Среднее время, мс |\n";
    std::cout << "------------------------------------------------------------------------------------\n";

    std::string configNames[3] = {
        "Конфигурация А (C=4) ",
        "Конфигурация Б (C=16)",
        "Конфигурация В (Indiv)"
    };

    for (int c = 0; c < 3; ++c) {
        double avg = (results[c][0] + results[c][1] + results[c][2]) / 3.0;
        std::cout << "| " << configNames[c] << " | "
            << std::setw(12) << std::fixed << std::setprecision(2) << results[c][0] << " | "
            << std::setw(12) << results[c][1] << " | "
            << std::setw(12) << results[c][2] << " | "
            << std::setw(17) << avg << " |\n";
    }
    std::cout << "====================================================================================\n\n";

    std::cout << "Программа завершила работу. Все объекты IPC успешно закрыты.\n";
    return 0;
}