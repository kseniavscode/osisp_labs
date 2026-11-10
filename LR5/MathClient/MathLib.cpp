#define MATHLIB_EXPORTS
#include "MathLib.h"
#include <windows.h>
#include <algorithm>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

extern "C" {
    MATHLIB_API double MathAdd(double a, double b) {
        return a + b;
    }

    MATHLIB_API double MathSub(double a, double b) {
        return a - b;
    }

    MATHLIB_API double MathMul(double a, double b) {
        return a * b;
    }

    MATHLIB_API double MathAverage(const double* arr, int size) {
        if (!arr || size <= 0) return 0.0;
        double sum = 0.0;
        for (int i = 0; i < size; ++i) {
            sum += arr[i];
        }
        return sum / size;
    }

    MATHLIB_API double MathMax(const double* arr, int size) {
        if (!arr || size <= 0) return 0.0;
        double maxVal = arr[0];
        for (int i = 1; i < size; ++i) {
            if (arr[i] > maxVal) {
                maxVal = arr[i];
            }
        }
        return maxVal;
    }
}