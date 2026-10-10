#include "Benchmarks.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>   // std::unique_ptr, std::shared_ptr — только для сравнения (ТЗ это разрешает)
#include <string>
#include <vector>

#include "AllocationCounter.hpp"
#include "pointers/MemorySpan.hpp"
#include "pointers/MsPtr.hpp"
#include "pointers/ShrdPtr.hpp"
#include "pointers/UnqPtr.hpp"

using Clock = std::chrono::steady_clock;

// ============================================================================
// Вспомогательные функции
// ============================================================================

// Не даёт оптимизатору выбросить «бесполезные» вычисления.
// Пустая ассемблерная вставка «читает» value и «может изменить любую память» —
// компилятор обязан честно посчитать value и не может склеить соседние итерации.
// Тот же приём использует библиотека Google Benchmark (benchmark::DoNotOptimize).
template <typename T>
void DoNotOptimize(const T& value) {
    asm volatile("" : : "r"(&value) : "memory");
}

double MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// На маленьких N один прогон длится наносекунды — таймер такое не измерит.
// Поэтому повторяем прогон, пока общее число операций не наберётся ~2 млн,
// и делим время на число повторов.
size_t RepeatsFor(size_t count) {
    const size_t kTargetOperations = 2'000'000;
    return count >= kTargetOperations ? 1 : kTargetOperations / count;
}

Measurement MakeMeasurement(const std::string& scenario, const std::string& variant, size_t count,
                            double total_ms, AllocationStats memory, size_t handle_bytes) {
    Measurement result;
    result.scenario = scenario;
    result.variant = variant;
    result.count = count;
    result.total_ms = total_ms;
    result.ns_per_op = total_ms * 1e6 / static_cast<double>(count);
    result.allocations = memory.count;
    result.heap_bytes = memory.bytes;
    result.handle_bytes = handle_bytes;
    return result;
}

// Сырой указатель нужно удалить вручную, умные удаляют объект сами
void Release(int*& ptr) {
    delete ptr;
}

template <typename Handle>
void Release(Handle&) {}

// ============================================================================
// Сценарий create: создать N объектов (каждый — отдельный new) и удалить их
// ============================================================================

template <typename Handle, typename Make>
Measurement MeasureCreate(const std::string& variant, size_t count, Make make) {
    std::vector<Handle> handles;
    handles.reserve(count);  // заранее, чтобы рост vector не попал в замер памяти

    const size_t repeats = RepeatsFor(count);
    AllocationStats memory;
    const Clock::time_point start = Clock::now();
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        if (repeat == 0)
            ResetAllocationStats();
        for (size_t i = 0; i < count; ++i)
            handles.push_back(make(static_cast<int>(i)));
        if (repeat == 0)
            memory = GetAllocationStats();  // всё, что выделено на N объектов
        DoNotOptimize(handles);
        for (Handle& handle : handles)
            Release(handle);
        handles.clear();  // здесь умные указатели удаляют объекты
    }
    const double total_ms = MillisecondsSince(start) / static_cast<double>(repeats);
    return MakeMeasurement("create", variant, count, total_ms, memory, count * sizeof(Handle));
}

// ============================================================================
// Сценарий access: прочитать значения через N указателей (*ptr)
// ============================================================================

template <typename Handle, typename Make>
Measurement MeasureAccess(const std::string& variant, size_t count, Make make) {
    std::vector<Handle> handles;
    handles.reserve(count);
    for (size_t i = 0; i < count; ++i)
        handles.push_back(make(static_cast<int>(i)));

    const size_t repeats = RepeatsFor(count);
    long long sum = 0;
    const Clock::time_point start = Clock::now();
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        for (const Handle& handle : handles)
            sum += *handle;
        DoNotOptimize(sum);
    }
    const double total_ms = MillisecondsSince(start) / static_cast<double>(repeats);

    for (Handle& handle : handles)
        Release(handle);
    return MakeMeasurement("access", variant, count, total_ms, AllocationStats(), count * sizeof(Handle));
}

// ============================================================================
// Сценарий copy: N раз скопировать указатель и уничтожить копию.
// Для ShrdPtr / shared_ptr это +1 и -1 к счётчику ссылок на каждой итерации.
// ============================================================================

template <typename Handle>
Measurement MeasureCopy(const std::string& variant, size_t count, const Handle& original) {
    const size_t repeats = RepeatsFor(count);
    ResetAllocationStats();
    const Clock::time_point start = Clock::now();
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        for (size_t i = 0; i < count; ++i) {
            Handle copy = original;
            DoNotOptimize(copy);
        }
    }
    const double total_ms = MillisecondsSince(start) / static_cast<double>(repeats);
    return MakeMeasurement("copy", variant, count, total_ms, GetAllocationStats(), sizeof(Handle));
}

// ============================================================================
// Сценарий traverse: пройти массив из N int и сложить элементы.
// Сырой массив против MemorySpan + MsPtr (каждый шаг проверяет границы).
// ============================================================================

Measurement MeasureRawArray(size_t count) {
    ResetAllocationStats();
    int* array = new int[count]();
    const AllocationStats memory = GetAllocationStats();
    for (size_t i = 0; i < count; ++i)
        array[i] = static_cast<int>(i % 100);

    const size_t repeats = RepeatsFor(count);
    long long sum = 0;
    const Clock::time_point start = Clock::now();
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        for (int* ptr = array; ptr != array + count; ++ptr)
            sum += *ptr;
        DoNotOptimize(sum);
    }
    const double total_ms = MillisecondsSince(start) / static_cast<double>(repeats);

    delete[] array;
    return MakeMeasurement("traverse", "raw", count, total_ms, memory, sizeof(int*));
}

// use_index = false: for (p = Begin(); p != End(); ++p) sum += *p;
// use_index = true:  sum += begin[i];
Measurement MeasureMsPtr(size_t count, bool use_index) {
    ResetAllocationStats();
    MemorySpan<int> span(count);
    const AllocationStats memory = GetAllocationStats();
    for (size_t i = 0; i < count; ++i)
        *span.Locate(i) = static_cast<int>(i % 100);

    const size_t repeats = RepeatsFor(count);
    long long sum = 0;
    const MsPtr<int> begin = span.Begin();
    const MsPtr<int> end = span.End();
    const Clock::time_point start = Clock::now();
    for (size_t repeat = 0; repeat < repeats; ++repeat) {
        if (use_index) {
            for (size_t i = 0; i < count; ++i)
                sum += begin[static_cast<ptrdiff_t>(i)];
        } else {
            for (MsPtr<int> ptr = begin; ptr != end; ++ptr)
                sum += *ptr;
        }
        DoNotOptimize(sum);
    }
    const double total_ms = MillisecondsSince(start) / static_cast<double>(repeats);

    const std::string variant = use_index ? "MsPtr[i]" : "MsPtr ++";
    return MakeMeasurement("traverse", variant, count, total_ms, memory, sizeof(MsPtr<int>));
}

// ============================================================================
// Запуск всех сценариев
// ============================================================================

BenchmarkConfig FullConfig() {
    BenchmarkConfig config;
    config.sizes = {10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000};
    config.large_sizes = {10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000, 100'000'000};
    return config;
}

BenchmarkConfig QuickConfig() {
    BenchmarkConfig config;
    config.sizes = {10, 100, 1'000, 10'000, 100'000};
    config.large_sizes = {10, 100, 1'000, 10'000, 100'000};
    return config;
}

std::vector<Measurement> RunAllBenchmarks(const BenchmarkConfig& config, std::ostream& progress) {
    std::vector<Measurement> results;

    for (size_t count : config.sizes) {
        progress << "  create, N = " << count << '\n';
        results.push_back(MeasureCreate<int*>("raw", count, [](int value) { return new int(value); }));
        results.push_back(MeasureCreate<UnqPtr<int>>("UnqPtr", count,
                                                     [](int value) { return MakeUnq<int>(value); }));
        results.push_back(MeasureCreate<std::unique_ptr<int>>("std::unique_ptr", count,
                                                              [](int value) { return std::make_unique<int>(value); }));
        results.push_back(MeasureCreate<ShrdPtr<int>>("ShrdPtr", count,
                                                      [](int value) { return MakeShrd<int>(value); }));
        results.push_back(MeasureCreate<std::shared_ptr<int>>("std::shared_ptr", count,
                                                              [](int value) { return std::shared_ptr<int>(new int(value)); }));
        results.push_back(MeasureCreate<std::shared_ptr<int>>("std::make_shared", count,
                                                              [](int value) { return std::make_shared<int>(value); }));
    }

    for (size_t count : config.sizes) {
        progress << "  access, N = " << count << '\n';
        results.push_back(MeasureAccess<int*>("raw", count, [](int value) { return new int(value); }));
        results.push_back(MeasureAccess<UnqPtr<int>>("UnqPtr", count,
                                                     [](int value) { return MakeUnq<int>(value); }));
        results.push_back(MeasureAccess<std::unique_ptr<int>>("std::unique_ptr", count,
                                                              [](int value) { return std::make_unique<int>(value); }));
        results.push_back(MeasureAccess<ShrdPtr<int>>("ShrdPtr", count,
                                                      [](int value) { return MakeShrd<int>(value); }));
        results.push_back(MeasureAccess<std::shared_ptr<int>>("std::shared_ptr", count,
                                                              [](int value) { return std::make_shared<int>(value); }));
    }

    int raw_value = 42;
    const ShrdPtr<int> shrd_original = MakeShrd<int>(42);
    const std::shared_ptr<int> std_original = std::make_shared<int>(42);
    for (size_t count : config.large_sizes) {
        progress << "  copy, N = " << count << '\n';
        results.push_back(MeasureCopy<int*>("raw", count, &raw_value));
        results.push_back(MeasureCopy<ShrdPtr<int>>("ShrdPtr", count, shrd_original));
        results.push_back(MeasureCopy<std::shared_ptr<int>>("std::shared_ptr", count, std_original));
    }

    for (size_t count : config.large_sizes) {
        progress << "  traverse, N = " << count << '\n';
        results.push_back(MeasureRawArray(count));
        results.push_back(MeasureMsPtr(count, false));
        results.push_back(MeasureMsPtr(count, true));
    }
    return results;
}

// ============================================================================
// Вывод результатов
// ============================================================================

// Варианты сценария в том порядке, в каком они впервые встретились
std::vector<std::string> VariantsOf(const std::vector<Measurement>& results, const std::string& scenario) {
    std::vector<std::string> variants;
    for (const Measurement& m : results) {
        if (m.scenario != scenario)
            continue;
        bool known = false;
        for (const std::string& v : variants)
            known = known || v == m.variant;
        if (!known)
            variants.push_back(m.variant);
    }
    return variants;
}

std::vector<size_t> CountsOf(const std::vector<Measurement>& results, const std::string& scenario) {
    std::vector<size_t> counts;
    for (const Measurement& m : results) {
        if (m.scenario == scenario && (counts.empty() || counts.back() != m.count))
            counts.push_back(m.count);
    }
    return counts;
}

const Measurement* Find(const std::vector<Measurement>& results, const std::string& scenario,
                        const std::string& variant, size_t count) {
    for (const Measurement& m : results) {
        if (m.scenario == scenario && m.variant == variant && m.count == count)
            return &m;
    }
    return nullptr;
}

void PrintTimeTable(const std::vector<Measurement>& results, const std::string& scenario,
                    const std::string& title, std::ostream& out) {
    const std::vector<std::string> variants = VariantsOf(results, scenario);
    if (variants.empty())
        return;

    out << "\n" << title << "\n";
    out << "  (последняя строка — время одного прогона на самом большом N, в миллисекундах)\n";
    out << std::setw(12) << "N";
    for (const std::string& variant : variants)
        out << std::setw(19) << variant;
    out << "\n";

    out << std::fixed << std::setprecision(2);
    for (size_t count : CountsOf(results, scenario)) {
        out << std::setw(12) << count;
        for (const std::string& variant : variants) {
            const Measurement* m = Find(results, scenario, variant, count);
            out << std::setw(19) << (m != nullptr ? m->ns_per_op : 0.0);
        }
        out << "\n";
    }

    // Общее время на самом большом N — чтобы было видно и абсолютные цифры
    const size_t largest = CountsOf(results, scenario).back();
    out << std::setw(12) << "ms, max N";
    for (const std::string& variant : variants) {
        const Measurement* m = Find(results, scenario, variant, largest);
        out << std::setw(19) << (m != nullptr ? m->total_ms : 0.0);
    }
    out << "\n";
}

void PrintMemoryTable(const std::vector<Measurement>& results, const std::string& scenario,
                      const std::string& title, std::ostream& out) {
    const std::vector<std::string> variants = VariantsOf(results, scenario);
    if (variants.empty())
        return;
    const size_t largest = CountsOf(results, scenario).back();

    out << "\n" << title << " (N = " << largest << ")\n";
    // Заголовки столбцов латиницей: setw считает байты, а русская буква — это 2 байта
    out << "  allocs/obj — выделений памяти на объект, heap — байт в куче на объект,\n"
        << "  handle — байт на сам указатель, total — всего байт на объект, MB — всего на N объектов\n";
    out << std::setw(18) << "variant" << std::setw(12) << "allocs/obj" << std::setw(12) << "heap"
        << std::setw(12) << "handle" << std::setw(12) << "total" << std::setw(12) << "MB" << "\n";

    for (const std::string& variant : variants) {
        const Measurement* m = Find(results, scenario, variant, largest);
        if (m == nullptr)
            continue;
        const double n = static_cast<double>(m->count);
        const double total = static_cast<double>(m->heap_bytes + m->handle_bytes);
        out << std::setw(18) << variant << std::setprecision(2)
            << std::setw(12) << static_cast<double>(m->allocations) / n
            << std::setw(12) << static_cast<double>(m->heap_bytes) / n
            << std::setw(12) << static_cast<double>(m->handle_bytes) / n
            << std::setw(12) << total / n
            << std::setw(12) << total / (1024.0 * 1024.0) << "\n";
    }
}

void PrintTables(const std::vector<Measurement>& results, std::ostream& out) {
    out << "\n================ ВРЕМЯ (наносекунд на один объект / одну операцию) ================\n";
    PrintTimeTable(results, "create", "create — создание и удаление N объектов", out);
    PrintTimeTable(results, "access", "access — чтение *ptr через N указателей", out);
    PrintTimeTable(results, "copy", "copy — копирование указателя N раз (счётчик ссылок +1/-1)", out);
    PrintTimeTable(results, "traverse", "traverse — обход массива из N int", out);

    out << "\n================ ПАМЯТЬ ================\n";
    PrintMemoryTable(results, "create", "create — N отдельных объектов int", out);
    PrintMemoryTable(results, "traverse", "traverse — массив из N int", out);
}

bool WriteCsv(const std::vector<Measurement>& results, const std::string& path) {
    std::ofstream file(path);
    if (!file)
        return false;
    file << "scenario,variant,count,total_ms,ns_per_op,allocations,heap_bytes,handle_bytes\n";
    file << std::setprecision(6);
    for (const Measurement& m : results) {
        file << m.scenario << ',' << m.variant << ',' << m.count << ',' << m.total_ms << ','
             << m.ns_per_op << ',' << m.allocations << ',' << m.heap_bytes << ',' << m.handle_bytes << '\n';
    }
    return static_cast<bool>(file);
}
