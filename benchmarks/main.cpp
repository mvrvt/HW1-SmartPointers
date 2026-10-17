#include <filesystem>
#include <iostream>
#include <string>

#include "Benchmarks.hpp"

// Запуск (из корня проекта):
//   ./build/benchmarks                 - полный замер, N до 10^7 и 10^8 (около минуты)
//   ./build/benchmarks --quick         - быстрый, N до 10^5 (пара секунд)
//   ./build/benchmarks --out папка     - куда сохранить CSV (по умолчанию benchmark_results)
// Графики: python3 benchmarks/plot.py benchmark_results/results.csv
int main(int argc, char* argv[]) {
    bool quick = false;
    std::string output_dir = "benchmark_results";
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--quick") {
            quick = true;
        } else if (argument == "--out" && i + 1 < argc) {
            output_dir = argv[++i];
        } else {
            std::cerr << "Неизвестный аргумент: " << argument << "\n"
                      << "Использование: benchmarks [--quick] [--out папка]\n";
            return 1;
        }
    }

#ifndef __OPTIMIZE__
    std::cout << "ВНИМАНИЕ: бенчмарки собраны без оптимизации - цифры не показательны.\n";
#endif

    std::cout << "Нагрузочное тестирование умных указателей ("
              << (quick ? "быстрый режим" : "полный режим") << ")\n";
    const BenchmarkConfig config = quick ? QuickConfig() : FullConfig();
    const std::vector<Measurement> results = RunAllBenchmarks(config, std::cout);

    PrintTables(results, std::cout);

    std::filesystem::create_directories(output_dir);
    const std::string csv_path = output_dir + "/results.csv";
    if (WriteCsv(results, csv_path)) {
        std::cout << "\nРезультаты сохранены в " << csv_path << "\n"
                  << "Графики: python3 benchmarks/plot.py " << csv_path << "\n";
    } else {
        std::cerr << "\nНе удалось записать " << csv_path << "\n";
        return 1;
    }
    return 0;
}
