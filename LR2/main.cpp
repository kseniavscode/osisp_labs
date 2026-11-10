#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

void PrintError(const std::string& operation) {
    DWORD errorCode = GetLastError();
    LPSTR messageBuffer = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL
    );
    std::cerr << "[ERROR] " << operation << " failed. Code: " << errorCode
        << " | Desc: " << (messageBuffer ? messageBuffer : "Unknown") << std::endl;
    if (messageBuffer) LocalFree(messageBuffer);
}

bool CreateTestFile(const std::string& filename, DWORD size) {
    HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    std::vector<char> buffer(65536, 'A');
    DWORD written = 0, total = 0;
    while (total < size) {
        DWORD toWrite = (std::min)(size - total, (DWORD)buffer.size());
        WriteFile(hFile, buffer.data(), toWrite, &written, NULL);
        total += written;
    }
    CloseHandle(hFile);
    return true;
}

struct BenchmarkResult {
    std::string mode;
    DWORD fileSizeMB;
    DWORD blockSizeKB;
    int parallelOps;
    double timeMs;
    double speedMBs;
};

int main() {
    const DWORD FILE_SIZE_MB = 50;
    const DWORD FILE_SIZE_BYTES = FILE_SIZE_MB * 1024 * 1024;
    const DWORD BLOCK_SIZE = 64 * 1024;
    const int TOTAL_CHUNKS = (FILE_SIZE_BYTES / BLOCK_SIZE); 

    std::cout << "=== LAB WORK 2: FILE I/O BENCHMARK (VARIANT 27) ===\n";
    std::cout << "Preparing test files (up to 8 files of " << FILE_SIZE_MB << " MB each)...\n";

    std::string filenames[8];
    for (int i = 0; i < 8; ++i) {
        filenames[i] = "test_file_" + std::to_string(i + 1) + ".bin";
        if (!CreateTestFile(filenames[i], FILE_SIZE_BYTES)) {
            std::cerr << "Failed to create " << filenames[i] << std::endl;
            return 1;
        }
    }
    std::cout << "Test files created successfully.\n\nRunning benchmarks...\n\n";

    std::vector<BenchmarkResult> results;
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    int opsList[] = { 1, 1, 2, 4, 8 };
    bool isAsyncList[] = { false, true, true, true, true };
    std::string modeNames[] = { "Sync", "Async", "Async", "Async", "Async" };

    for (int t = 0; t < 5; ++t) {
        bool asyncMode = isAsyncList[t];
        int numOps = opsList[t];

        std::vector<HANDLE> hFiles(numOps);
        std::vector<HANDLE> hEvents(numOps, NULL);
        std::vector<OVERLAPPED> overlappeds(numOps, { 0 });
        std::vector<std::vector<char>> buffers(numOps, std::vector<char>(BLOCK_SIZE));
        std::vector<ULARGE_INTEGER> offsets(numOps);
        std::vector<int> chunksRead(numOps, 0);

        for (int i = 0; i < numOps; ++i) {
            offsets[i].QuadPart = 0;
            DWORD flags = asyncMode ? FILE_FLAG_OVERLAPPED : 0;
            hFiles[i] = CreateFileA(filenames[i].c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, flags, NULL);

            if (hFiles[i] == INVALID_HANDLE_VALUE) {
                PrintError("CreateFileA");
                return 1;
            }

            if (asyncMode) {
                hEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
                overlappeds[i].hEvent = hEvents[i];
            }
        }

        QueryPerformanceCounter(&start);

        if (!asyncMode) {
            DWORD bytesRead = 0;
            std::vector<char> syncBuf(BLOCK_SIZE);
            while (true) {
                BOOL res = ReadFile(hFiles[0], syncBuf.data(), BLOCK_SIZE, &bytesRead, NULL);
                if (!res || bytesRead == 0) break;
            }
        }
        else {
            for (int i = 0; i < numOps; ++i) {
                overlappeds[i].Offset = offsets[i].LowPart;
                overlappeds[i].OffsetHigh = offsets[i].HighPart;
                DWORD dummy = 0;
                BOOL res = ReadFile(hFiles[i], buffers[i].data(), BLOCK_SIZE, &dummy, &overlappeds[i]);
                if (!res && GetLastError() != ERROR_IO_PENDING) {
                    PrintError("Async ReadFile start");
                }
            }

            int activeCount = numOps;
            std::vector<HANDLE> activeEvents = hEvents; 

            while (activeCount > 0) {
                DWORD waitRes = WaitForMultipleObjects(numOps, activeEvents.data(), FALSE, INFINITE);

                if (waitRes >= WAIT_OBJECT_0 && waitRes < WAIT_OBJECT_0 + numOps) {
                    int idx = waitRes - WAIT_OBJECT_0;
                    DWORD transferred = 0;
                    BOOL getRes = GetOverlappedResult(hFiles[idx], &overlappeds[idx], &transferred, FALSE);

                    chunksRead[idx]++;

                    if (!getRes || transferred == 0 || chunksRead[idx] >= TOTAL_CHUNKS) {
                        activeEvents[idx] = NULL; 
                        activeCount--;
                    }
                    else {
                        offsets[idx].QuadPart += BLOCK_SIZE;
                        ResetEvent(hEvents[idx]);
                        ZeroMemory(&overlappeds[idx], sizeof(OVERLAPPED));
                        overlappeds[idx].hEvent = hEvents[idx];
                        overlappeds[idx].Offset = offsets[idx].LowPart;
                        overlappeds[idx].OffsetHigh = offsets[idx].HighPart;

                        DWORD dummy = 0;
                        BOOL nextRes = ReadFile(hFiles[idx], buffers[idx].data(), BLOCK_SIZE, &dummy, &overlappeds[idx]);
                        if (!nextRes && GetLastError() != ERROR_IO_PENDING) {
                            activeEvents[idx] = NULL;
                            activeCount--;
                        }
                    }
                }
                else {
                    break;
                }
            }
        }

        QueryPerformanceCounter(&end);

        double timeSec = static_cast<double>(end.QuadPart - start.QuadPart) / freq.QuadPart;
        double timeMs = timeSec * 1000.0;
        double totalProcessedMB = static_cast<double>(FILE_SIZE_MB * numOps);
        double speedMBs = timeSec > 0 ? totalProcessedMB / timeSec : 0;

        results.push_back({ modeNames[t], FILE_SIZE_MB, BLOCK_SIZE / 1024, numOps, timeMs, speedMBs });

        for (int i = 0; i < numOps; ++i) {
            CloseHandle(hFiles[i]);
            if (hEvents[i]) CloseHandle(hEvents[i]);
        }
    }

    std::cout << "\n=========================================================================================\n";
    std::cout << "| Work Mode   | File Size    | Block Size  | Parallel Operations   | Time (ms)  | Speed (MB/s) |\n";
    std::cout << "=========================================================================================\n";
    for (const auto& r : results) {
        std::cout << "| " << std::left << std::setw(11) << r.mode
            << " | " << std::setw(12) << (std::to_string(r.fileSizeMB) + " MB")
            << " | " << std::setw(11) << (std::to_string(r.blockSizeKB) + " KB")
            << " | " << std::setw(21) << r.parallelOps
            << " | " << std::setw(10) << std::fixed << std::setprecision(2) << r.timeMs
            << " | " << std::setw(12) << r.speedMBs << " |\n";
    }
    std::cout << "=========================================================================================\n";
    std::cout << "Benchmark completed successfully without errors. Resources released.\n";

    return 0;
}