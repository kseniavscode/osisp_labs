#include <windows.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

const int DETAIL_LIMIT = 3;   

double MeasureProcessCreation(int count);
double MeasureThreadCreation(int count);

DWORD WINAPI ThreadFunc(LPVOID lpParam) { return 0; }

void PrintError(const string& operation)
{
    DWORD err = GetLastError();
    LPSTR msgBuf = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, err, 0, (LPSTR)&msgBuf, 0, nullptr);
    cerr << "  [ERROR] " << operation
        << " | code = " << err
        << " | " << (msgBuf ? msgBuf : "no description") << endl;
    if (msgBuf) LocalFree(msgBuf);
}

void WaitForAllHandles(const vector<HANDLE>& handles)
{
    const DWORD MAX_WAIT = 64;
    size_t i = 0;
    while (i < handles.size())
    {
        DWORD batch = (DWORD)min((size_t)MAX_WAIT, handles.size() - i);
        WaitForMultipleObjects(batch, handles.data() + i, TRUE, INFINITE);
        i += batch;
    }
}


double MeasureProcessCreation(int count)
{
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    vector<HANDLE> handles;
    handles.reserve(count);

    cout << "  Creating " << count << " process(es)...\n";

    QueryPerformanceCounter(&start);

    for (int i = 0; i < count; i++)
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };

        BOOL ok = CreateProcessA("worker.exe", nullptr,
            nullptr, nullptr, FALSE, 0,
            nullptr, nullptr, &si, &pi);

        if (!ok) { PrintError("CreateProcessA"); continue; }

        if (i < DETAIL_LIMIT)
        {
            cout << "    #" << (i + 1)
                << " | PID = " << pi.dwProcessId
                << " | state = created"
                << " | TID(main) = " << pi.dwThreadId << endl;
        }

        handles.push_back(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    WaitForAllHandles(handles);
    QueryPerformanceCounter(&end);

    if (count <= DETAIL_LIMIT)
    {
        for (int i = 0; i < (int)handles.size(); i++)
        {
            DWORD exitCode = 0;
            GetExitCodeProcess(handles[i], &exitCode);
            cout << "    #" << (i + 1)
                << " | state = terminated"
                << " | exit code = " << exitCode << endl;
        }
    }

    for (HANDLE h : handles) CloseHandle(h);

    return (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000.0;
}

double MeasureThreadCreation(int count)
{
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    vector<HANDLE> handles;
    handles.reserve(count);

    cout << "  Creating " << count << " thread(s)...\n";

    QueryPerformanceCounter(&start);

    for (int i = 0; i < count; i++)
    {
        DWORD tid;
        HANDLE h = CreateThread(nullptr, 0, ThreadFunc, nullptr, 0, &tid);
        if (!h) { PrintError("CreateThread"); continue; }

        if (i < DETAIL_LIMIT)
        {
            cout << "    #" << (i + 1)
                << " | TID = " << tid
                << " | state = created" << endl;
        }

        handles.push_back(h);
    }

    WaitForAllHandles(handles);
    QueryPerformanceCounter(&end);

    if (count <= DETAIL_LIMIT)
    {
        for (int i = 0; i < (int)handles.size(); i++)
        {
            DWORD exitCode = 0;
            GetExitCodeThread(handles[i], &exitCode);
            cout << "    #" << (i + 1)
                << " | state = terminated"
                << " | exit code = " << exitCode << endl;
        }
    }

    for (HANDLE h : handles) CloseHandle(h);

    return (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000.0;
}

double Median(vector<double> v)
{
    sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

struct ExperimentResult
{
    bool isProcess;     
    int  count;         
    double run1;
    double run2;
    double run3;
    double average;
    double median;
    double avgPerOne;   
};

ExperimentResult MeasureAllRuns(bool isProcess, int count, int runs = 3)
{
    ExperimentResult res;
    res.isProcess = isProcess;
    res.count = count;

    vector<double> results;
    double total = 0.0;

    for (int r = 0; r < runs; r++)
    {
        cout << "  [run " << (r + 1) << "/" << runs << "]\n";
        double t = isProcess
            ? MeasureProcessCreation(count)
            : MeasureThreadCreation(count);
        cout << "    -> time = " << fixed << setprecision(3)
            << t << " ms\n";
        results.push_back(t);
        total += t;
    }

    res.run1 = results[0];
    res.run2 = results[1];
    res.run3 = results[2];
    res.average = total / runs;
    res.median = Median(results);
    res.avgPerOne = res.average / count;

    return res;
}

void PrintFinalTable(const vector<ExperimentResult>& results)
{
    cout << "\n";
    cout << "================================================================\n";
    cout << "                     FINAL RESULTS TABLE\n";
    cout << "================================================================\n";
    cout << setw(6) << "Type"
        << setw(7) << "N"
        << setw(10) << "Run 1"
        << setw(10) << "Run 2"
        << setw(10) << "Run 3"
        << setw(11) << "Average"
        << setw(11) << "Median"
        << setw(13) << "Avg per 1" << endl;
    cout << "----------------------------------------------------------------\n";

    for (const auto& r : results)
    {
        cout << setw(6) << (r.isProcess ? "PROC" : "THR")
            << setw(7) << r.count
            << setw(10) << fixed << setprecision(3) << r.run1
            << setw(10) << r.run2
            << setw(10) << r.run3
            << setw(11) << r.average
            << setw(11) << r.median
            << setw(13) << r.avgPerOne << endl;
    }
    cout << "================================================================\n";
}

int main()
{
    cout << "=== Lab 1. Variant 27 ===\n";
    cout << "Process vs Thread creation cost\n";
    cout << "Each experiment is repeated 3 times\n\n";

    const int counts[] = { 1, 10, 100 };

    vector<ExperimentResult> allResults;

    cout << "=========== PROCESSES ===========\n";
    for (int n : counts)
    {
        cout << "\n>>> N = " << n << " <<<\n";
        ExperimentResult r = MeasureAllRuns(true, n, 3);
        allResults.push_back(r);

        cout << "Result for N = " << n << ":\n";
        cout << "  Run 1   = " << r.run1 << " ms\n";
        cout << "  Run 2   = " << r.run2 << " ms\n";
        cout << "  Run 3   = " << r.run3 << " ms\n";
        cout << "  Average = " << r.average << " ms\n";
        cout << "  Median  = " << r.median << " ms\n";
        cout << "  Avg/1   = " << r.avgPerOne << " ms\n\n";
    }

    cout << "\n=========== THREADS ===========\n";
    for (int n : counts)
    {
        cout << "\n>>> N = " << n << " <<<\n";
        ExperimentResult r = MeasureAllRuns(false, n, 3);
        allResults.push_back(r);

        cout << "Result for N = " << n << ":\n";
        cout << "  Run 1   = " << r.run1 << " ms\n";
        cout << "  Run 2   = " << r.run2 << " ms\n";
        cout << "  Run 3   = " << r.run3 << " ms\n";
        cout << "  Average = " << r.average << " ms\n";
        cout << "  Median  = " << r.median << " ms\n";
        cout << "  Avg/1   = " << r.avgPerOne << " ms\n\n";
    }

    PrintFinalTable(allResults);

    cout << "\nDone.\n";
    system("pause");
    return 0;
}