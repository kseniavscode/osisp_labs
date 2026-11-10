#include <windows.h>
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include "MathLib.h"

#pragma comment(lib, "MathLib_123.lib")

const int STUDENT_ID = 123;
const unsigned int SEED = 1000 + STUDENT_ID * 97; 

std::vector<double> GenerateArray(int size) {
    std::mt19937 rng(SEED);
    std::uniform_real_distribution<double> dist(1.0, 1000.0);
    std::vector<double> arr(size);
    for (int i = 0; i < size; ++i) {
        arr[i] = dist(rng);
    }
    return arr;
}

double BenchmarkArrayOps(const std::vector<double>& arr, int iterations = 10000) {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    double volatile avg = 0.0;
    double volatile maxVal = 0.0;

    for (int i = 0; i < iterations; ++i) {
        avg = MathAverage(arr.data(), (int)arr.size());
        maxVal = MathMax(arr.data(), (int)arr.size());
    }

    QueryPerformanceCounter(&end);
    return static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    std::cout << "====================================================================\n";
    std::cout << "ЛАБОРАТОРНАЯ РАБОТА: ДИНАМИЧЕСКИЕ БИБЛИОТЕКИ (ВАРИАНТ 27)\n";
    std::cout << "Модуль: MathLib_123.dll | Неявное связывание (Implicit Linking)\n";
    std::cout << "STUDENT_ID=" << STUDENT_ID << " | SEED=" << SEED << "\n";
    std::cout << "====================================================================\n\n";

    std::cout << ">>> 1. ПРОВЕРКА БАЗОВЫХ МАТЕМАТИЧЕСКИХ ОПЕРАЦИЙ DLL:\n";
    double a = 45.5, b = 15.0;
    std::cout << "MathAdd(" << a << ", " << b << ") = " << MathAdd(a, b) << " (Ожидается: 60.5)\n";
    std::cout << "MathSub(" << a << ", " << b << ") = " << MathSub(a, b) << " (Ожидается: 30.5)\n";
    std::cout << "MathMul(" << a << ", " << b << ") = " << MathMul(a, b) << " (Ожидается: 682.5)\n\n";

    std::vector<double> testArr = { 10.0, 25.5, 3.2, 98.4, 42.1 };
    std::cout << ">>> 2. ПРОВЕРКА ОПЕРАЦИЙ НАД ТЕСТОВЫМ МАССИВОМ (5 элементов):\n";
    std::cout << "MathAverage() = " << MathAverage(testArr.data(), (int)testArr.size()) << " (Ожидается: 35.84)\n";
    std::cout << "MathMax()     = " << MathMax(testArr.data(), (int)testArr.size()) << " (Ожидается: 98.4)\n\n";

    std::cout << ">>> 3. ПРОВЕДЕНИЕ ИЗМЕРЕНИЙ ДЛЯ ТАБЛИЦЫ РЕЗУЛЬТАТОВ (10 000 итераций):\n";
    std::cout << "Выполняются замеры времени...\n\n";

    int sizes[3] = { 100, 1000, 10000 };
    double results[3][3];

    for (int c = 0; c < 3; ++c) {
        std::vector<double> arr = GenerateArray(sizes[c]);
        for (int run = 0; run < 3; ++run) {
            results[c][run] = BenchmarkArrayOps(arr);
        }
    }

    std::cout << "========================== ТАБЛИЦА 2 ==========================\n";
    std::cout << "| Конфигурация        | Запуск 1, мс | Запуск 2, мс | Запуск 3, мс | Среднее, мс |\n";
    std::cout << "--------------------------------------------------------------------\n";

    std::string configNames[3] = {
        "А (Массив 100 эл.)   ",
        "Б (Массив 1000 эл.)  ",
        "В (Массив 10000 эл.) "
    };

    for (int c = 0; c < 3; ++c) {
        double avg = (results[c][0] + results[c][1] + results[c][2]) / 3.0;
        std::cout << "| " << configNames[c] << " | "
            << std::setw(12) << std::fixed << std::setprecision(2) << results[c][0] << " | "
            << std::setw(12) << results[c][1] << " | "
            << std::setw(12) << results[c][2] << " | "
            << std::setw(11) << avg << " |\n";
    }
    std::cout << "====================================================================\n\n";

    std::cout << "Программа завершила работу успешно. Нажмите Enter...";
    std::cin.get();
    return 0;
}