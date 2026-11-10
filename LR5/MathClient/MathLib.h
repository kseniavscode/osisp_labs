#pragma once

#ifdef MATHLIB_EXPORTS
#define MATHLIB_API __declspec(dllexport)
#else
#define MATHLIB_API __declspec(dllimport)
#endif

extern "C" {
    MATHLIB_API double MathAdd(double a, double b);
    MATHLIB_API double MathSub(double a, double b);
    MATHLIB_API double MathMul(double a, double b);
    MATHLIB_API double MathAverage(const double* arr, int size);
    MATHLIB_API double MathMax(const double* arr, int size);
}