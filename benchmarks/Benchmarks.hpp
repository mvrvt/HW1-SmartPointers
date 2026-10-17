#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

// Один замер: один сценарий, один вариант указателя, одно количество объектов
struct Measurement {
    std::string scenario;     // create, access, copy, traverse
    std::string variant;      // raw, UnqPtr, std::unique_ptr, ...
    size_t count = 0;         // N - сколько объектов / операций
    double total_ms = 0;      // время одного прогона на N объектах, мс
    double ns_per_op = 0;     // то же, в пересчёте на один объект, нс
    size_t allocations = 0;   // сколько раз выделялась память за прогон
    size_t heap_bytes = 0;    // сколько байт выделено в куче за прогон
    size_t handle_bytes = 0;  // сколько занимают сами указатели (N * sizeof)
};

struct BenchmarkConfig {
    std::vector<size_t> sizes;        // для create и access (каждый объект - отдельный new)
    std::vector<size_t> large_sizes;  // для copy и traverse (до 10^8: память почти не нужна)
};

BenchmarkConfig FullConfig();   // 10 ... 10^7 и до 10^8
BenchmarkConfig QuickConfig();  // 10 ... 10^5 - за пару секунд, для демонстрации

// Запускает все сценарии; ход работы пишет в progress
std::vector<Measurement> RunAllBenchmarks(const BenchmarkConfig& config, std::ostream& progress);

// Таблицы времени и памяти в консоль
void PrintTables(const std::vector<Measurement>& results, std::ostream& out);

// CSV-файл для построения графиков (benchmarks/plot.py)
bool WriteCsv(const std::vector<Measurement>& results, const std::string& path);
