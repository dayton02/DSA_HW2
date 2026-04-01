/*  bench.cpp
 *
 *  Benchmark for the APSC algorithm.
 *  Links directly against apsc.o
 *
 *  For each CSV in experiments/custom_inputs/ it:
 *    1. Parses the polygon(s) (same logic as main.cpp, side-effect-free).
 *    2. Calls apscPolygon() at 50 % of input vertex count.
 *    3. Records wall-clock time (std::chrono) and peak virtual memory
 *       (/proc/self/status VmPeak, available under Linux / WSL).
 *    4. Additionally sweeps target vertex counts on A_ellipse_n01000.csv to
 *       produce data for the "areal displacement vs. target" plot.
 *    5. Writes all results to experiments/results.json.
 *
 */

#include "../apsc.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs  = std::filesystem;
using   Clock = std::chrono::high_resolution_clock;

// Constants
static const std::string INPUT_DIR   = "experiments/custom_inputs";
static const std::string RESULT_FILE = "experiments/results.json";
static const double  TARGET_FRAC     = 0.50;   // 50 % reduction for main bench
static const int     MIN_TARGET      = 4;       // APSC requires at least 4 vertices

// Memory measurement (/proc/self/status)
// Returns VmPeak in KB, or -1 if unavailable
static long readVmPeakKB()
{
    std::ifstream f("/proc/self/status");
    if (!f.is_open()) return -1;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmPeak:", 0) == 0) {
            long kb = -1;
            sscanf(line.c_str(), "VmPeak: %ld kB", &kb);
            return kb;
        }
    }
    return -1;
}

// CSV parser
static void parseCsv(const std::string& path, std::map<int, burd>& rings)
{
    rings.clear();
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "  [bench] Cannot open: " << path << '\n';
        return;
    }
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (!std::isdigit((unsigned char)line[0])) break;
        std::stringstream ss(line);
        std::string tok;
        try {
            std::getline(ss, tok, ','); int ring_id   = std::stoi(tok);
            std::getline(ss, tok, ','); int vertex_id = std::stoi(tok);
            std::getline(ss, tok, ','); double x      = std::stod(tok);
            std::getline(ss, tok, ','); double y      = std::stod(tok);
            rings[ring_id].push_back({ ring_id, vertex_id, x, y });
        } catch (...) { continue; }
    }
}

static int totalVertices(const std::map<int, burd>& rings)
{
    int n = 0;
    for (auto& [id, r] : rings) n += (int)r.size();
    return n;
}

// JSON writer
struct JsonWriter {
    std::ofstream f;
    bool first = true;

    explicit JsonWriter(const std::string& path) : f(path) {
        f << "[\n";
    }
    ~JsonWriter() {
        f << "\n]\n";
    }

    void record(const std::string& section,
                const std::string& file,
                int    n_vert,
                int    target,
                long long elapsed_us,
                long   vmpeak_before_kb,
                long   vmpeak_after_kb,
                double areal_disp)
    {
        if (!first) f << ",\n";
        first = false;
        f << "  {"
          << "\"section\":\""        << section         << "\","
          << "\"file\":\""           << file            << "\","
          << "\"n_vertices\":"       << n_vert          << ","
          << "\"target\":"           << target          << ","
          << "\"elapsed_us\":"       << elapsed_us      << ","
          << "\"vmpeak_before_kb\":" << vmpeak_before_kb<< ","
          << "\"vmpeak_after_kb\":"  << vmpeak_after_kb << ","
          << "\"areal_disp\":"
          << std::scientific << std::setprecision(6) << areal_disp
          << "}";
    }
};

// One benchmark run
struct RunResult {
    long long elapsed_us;
    long      vmpeak_before_kb;
    long      vmpeak_after_kb;
    double    areal_disp;
};

static RunResult runOne(const std::map<int, burd>& ref_rings, int target)
{
    std::map<int, burd> work = ref_rings;   // apscPolygon modifies in-place
    double disp = 0.0;

    long vm_before = readVmPeakKB();
    auto t0 = Clock::now();
    apscPolygon(work, target, true, disp);
    auto t1 = Clock::now();
    long vm_after  = readVmPeakKB();

    long long us = std::chrono::duration_cast<
                       std::chrono::microseconds>(t1 - t0).count();
    return { us, vm_before, vm_after, disp };
}

// Parse output line from child process.:
// RESULT <elapsed_us> <vmpeak_before_kb> <vmpeak_after_kb> <areal_disp>
static bool parseChildResult(const std::string& text, RunResult& out)
{
    std::istringstream iss(text);
    std::string tag;
    if (!(iss >> tag)) return false;
    if (tag != "RESULT") return false;
    if (!(iss >> out.elapsed_us >> out.vmpeak_before_kb
              >> out.vmpeak_after_kb >> out.areal_disp)) {
        return false;
    }
    return true;
}

// Run a single benchmark in a fresh child process to isolate VmPeak.
// (POSIX only)
static bool runOneIsolated(const std::string& selfPath,
                           const std::string& csvPath,
                           int target,
                           RunResult& out)
{
#ifdef _WIN32
    // Windows: popen/pclose not available; use non-isolated fallback
    return false;
#else
    std::string cmd = "\"" + selfPath + "\" --single \"" + csvPath +
                      "\" " + std::to_string(target);

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;

    std::string line;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        line = buffer;
    }

    int rc = pclose(pipe);
    if (rc != 0) return false;
    return parseChildResult(line, out);
#endif
}
//  Analysis: Curve fitting and results summary


struct Record {
    std::string section;
    std::string file;
    int n_vertices;
    int target;
    long long elapsed_us;
    long vmpeak_before_kb;
    long vmpeak_after_kb;
    double areal_disp;
};

// Minimal JSON parser for results.json
static std::vector<Record> parseResultsJson(const std::string& filepath) {
    std::vector<Record> records;
    std::ifstream f(filepath);
    if (!f.is_open()) return records;
    
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    
    size_t pos = 0;
    while ((pos = content.find("{", pos)) != std::string::npos) {
        size_t end = content.find("}", pos);
        if (end == std::string::npos) break;
        
        std::string obj = content.substr(pos, end - pos + 1);
        Record r = {};
        
        auto extract = [&obj](const char* key) -> std::string {
            size_t p = obj.find(key);
            if (p == std::string::npos) return "";
            p = obj.find(":", p);
            if (p == std::string::npos) return "";
            p = obj.find_first_not_of(" :\"", p);
            size_t q = obj.find_first_of(",}\"", p);
            return obj.substr(p, q - p);
        };
        
        r.section = extract("\"section\"");
        r.file = extract("\"file\"");
        r.n_vertices = std::stoi(extract("\"n_vertices\""));
        r.target = std::stoi(extract("\"target\""));
        r.elapsed_us = std::stoll(extract("\"elapsed_us\""));
        r.vmpeak_before_kb = std::stol(extract("\"vmpeak_before_kb\""));
        r.vmpeak_after_kb = std::stol(extract("\"vmpeak_after_kb\""));
        r.areal_disp = std::stod(extract("\"areal_disp\""));
        
        records.push_back(r);
        pos = end + 1;
    }
    return records;
}

// Least-squares fit for power-law c·n^k on log-log scale
static void fitPowerLaw(const std::vector<double>& ns,
                        const std::vector<double>& ys,
                        double& c, double& k, double& r2) {
    c = k = r2 = 0.0;
    if (ns.size() < 3) return;
    
    std::vector<double> log_ns, log_ys;
    for (size_t i = 0; i < ns.size(); i++) {
        if (ns[i] > 0 && ys[i] > 0) {
            log_ns.push_back(std::log(ns[i]));
            log_ys.push_back(std::log(ys[i]));
        }
    }
    if (log_ns.size() < 3) return;
    
    double mean_x = 0, mean_y = 0;
    for (auto x : log_ns) mean_x += x;
    for (auto y : log_ys) mean_y += y;
    mean_x /= log_ns.size();
    mean_y /= log_ys.size();
    
    double cov_xy = 0, var_x = 0;
    for (size_t i = 0; i < log_ns.size(); i++) {
        double dx = log_ns[i] - mean_x;
        double dy = log_ys[i] - mean_y;
        cov_xy += dx * dy;
        var_x += dx * dx;
    }
    
    if (var_x < 1e-12) return;
    k = cov_xy / var_x;
    double log_c = mean_y - k * mean_x;
    c = std::exp(log_c);
    
    // R² calculation
    double ss_res = 0, ss_tot = 0;
    for (size_t i = 0; i < log_ns.size(); i++) {
        double pred = k * log_ns[i] + log_c;
        ss_res += (log_ys[i] - pred) * (log_ys[i] - pred);
        ss_tot += (log_ys[i] - mean_y) * (log_ys[i] - mean_y);
    }
    if (ss_tot > 0) r2 = 1.0 - ss_res / ss_tot;
}

// Analyze results and print summary
static void analyzeResults(const std::string& resultsFile) {
    auto records = parseResultsJson(resultsFile);
    if (records.empty()) {
        printf("\n[analysis] No records found in %s\n", resultsFile.c_str());
        return;
    }
    
    // Separate benchmark and sweep records
    std::vector<Record> benchmarks, sweeps;
    for (auto& r : records) {
        if (r.section == "benchmark") benchmarks.push_back(r);
        else if (r.section == "sweep") sweeps.push_back(r);
    }
    
    // Group benchmarks by family
    std::map<char, std::vector<Record>> families;
    for (auto& r : benchmarks) {
        char family = r.file[0];
        families[family].push_back(r);
    }
    
    // Family descriptors
    std::map<char, std::pair<std::string, std::string>> family_info = {
        { 'A', { "Smooth Ellipse", "Algorithmic complexity baseline (n=50..10000)" } },
        { 'B', { "Many Holes", "Ring topology stress test" } },
        { 'C', { "Narrow Gap", "Degeneracy & gap detection" } },
        { 'D', { "Spiky/Near-Degen", "Zero-cost collapse handling" } },
        { 'E', { "Fractal Boundary", "High-detail, self-similar features" } },
        { 'F', { "Mixed Multi-Ring", "Complex realistic topology" } }
    };
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  EXPERIMENTAL RESULTS ANALYSIS\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    // Summary table
    printf("\nFamily Benchmarks (50%% target reduction):\n");
    printf("%-3s %-18s %-6s %-10s %-10s %-12s\n",
           "ID", "Type", "N", "Avg Time", "Avg Mem", "Max Displ");
    printf("%s\n", std::string(70, '-').c_str());
    
    for (char fam : {'A', 'B', 'C', 'D', 'E', 'F'}) {
        if (!families.count(fam)) continue;
        
        auto& recs = families[fam];
        double total_time = 0, total_mem = 0, max_disp = 0;
        for (auto& r : recs) {
            total_time += r.elapsed_us / 1000.0;
            total_mem += r.vmpeak_after_kb;
            max_disp = std::max(max_disp, r.areal_disp);
        }
        double avg_time = total_time / recs.size();
        double avg_mem = total_mem / recs.size();
        
        printf("%c   %-18s %6zu %9.2f ms %8ld KB %12.3e\n",
               fam, family_info[fam].first.c_str(), recs.size(), 
               avg_time, (long)avg_mem, max_disp);
    }
    
    // Dataset properties
    printf("\nDataset Properties:\n");
    for (char fam : {'A', 'B', 'C', 'D', 'E', 'F'}) {
        if (!family_info.count(fam)) continue;
        printf("  [%c] %-15s  %s\n", fam, family_info[fam].first.c_str(),
               family_info[fam].second.c_str());
    }
    
    // A-series scaling
    printf("\nScaling Analysis (A-series):\n");
    if (families.count('A')) {
        auto& a_recs = families['A'];
        std::sort(a_recs.begin(), a_recs.end(),
                  [](const Record& x, const Record& y) { return x.n_vertices < y.n_vertices; });
        
        std::vector<double> ns, times_ms, mems_kb;
        for (auto& r : a_recs) {
            ns.push_back(r.n_vertices);
            times_ms.push_back(r.elapsed_us / 1000.0);
            mems_kb.push_back(r.vmpeak_after_kb);
        }
        
        double c_t, k_t, r2_t;
        fitPowerLaw(ns, times_ms, c_t, k_t, r2_t);
        printf("  Time:    T(n) ≈ %.3e·n^%.4f  (R²=%.4f)\n", c_t, k_t, r2_t);
        
        double c_m, k_m, r2_m;
        fitPowerLaw(ns, mems_kb, c_m, k_m, r2_m);
        printf("  Memory:  M(n) ≈ %.3e·n^%.4f  (R²=%.4f)\n", c_m, k_m, r2_m);
    }
    
    // Displacement sweep
    if (!sweeps.empty()) {
        printf("\nDisplacement Sweep (A_ellipse_n01000):\n");
        
        double min_disp = sweeps[0].areal_disp, max_disp = sweeps[0].areal_disp;
        int min_target = sweeps[0].target, max_target = sweeps[0].target;
        for (auto& s : sweeps) {
            if (s.areal_disp < min_disp) {
                min_disp = s.areal_disp;
                min_target = s.target;
            }
            if (s.areal_disp > max_disp) {
                max_disp = s.areal_disp;
                max_target = s.target;
            }
        }
        printf("  Min: %.3e (target=%d), Max: %.3e (target=%d), Ratio: %.2f×\n",
               min_disp, min_target, max_disp, max_target, max_disp / min_disp);
    }
    
    printf("\n");
}

int main(int argc, char** argv)
{
    // Child: run one dataset/target and print one machine-readable line.
    if (argc == 4 && std::string(argv[1]) == "--single") {
        std::string csvPath = argv[2];
        int target = std::stoi(argv[3]);

        std::map<int, burd> rings;
        parseCsv(csvPath, rings);
        if (totalVertices(rings) == 0) return 2;

        RunResult r = runOne(rings, target);
        std::cout << "RESULT "
                  << r.elapsed_us << " "
                  << r.vmpeak_before_kb << " "
                  << r.vmpeak_after_kb << " "
                  << std::scientific << std::setprecision(6) << r.areal_disp
                  << "\n";
        return 0;
    }

    // Collect and sort CSV files by name
    std::vector<std::string> files;
    if (!fs::exists(INPUT_DIR)) {
        std::cerr << "ERROR: " << INPUT_DIR << " does not exist.\n"
                  << "Run experiments/gen_cases first.\n";
        return 1;
    }
    for (auto& e : fs::directory_iterator(INPUT_DIR)) {
        if (e.path().extension() == ".csv")
            files.push_back(e.path().filename().string());
    }
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        std::cerr << "No CSV files found in " << INPUT_DIR << ".\n";
        return 1;
    }

    JsonWriter jw(RESULT_FILE);

    printf("\n%-46s %6s %6s %10s %10s %16s\n",
           "file", "n", "target", "time(ms)", "vmΔ(KB)", "areal_disp");
    printf("%s\n", std::string(100, '-').c_str());

    // Primary benchmark: every CSV at 50 % target
    for (const auto& fname : files) {
        std::string path = INPUT_DIR + "/" + fname;
        std::map<int, burd> rings;
        parseCsv(path, rings);
        int n = totalVertices(rings);
        if (n == 0) { printf("  (empty) %s\n", fname.c_str()); continue; }
        int target = std::max(MIN_TARGET, (int)(n * TARGET_FRAC));

        RunResult r;
        bool ok = runOneIsolated(argv[0], path, target, r);
        if (!ok) {
            // Fallback keeps benchmarking usable even if popen fails.
            r = runOne(rings, target);
        }
        long vm_delta = std::max(0L, r.vmpeak_after_kb - r.vmpeak_before_kb);

        printf("%-46s %6d %6d %10.1f %10ld %16.4e\n",
               fname.c_str(), n, target,
               r.elapsed_us / 1000.0, vm_delta, r.areal_disp);

        jw.record("benchmark", fname, n, target,
                  r.elapsed_us, r.vmpeak_before_kb, r.vmpeak_after_kb,
                  r.areal_disp);
    }

    // Displacement sweep: A_ellipse_n01000.csv at 5 % .. 95 % of n
    const std::string sweepPath = INPUT_DIR + "/A_ellipse_n01000.csv";
    if (fs::exists(sweepPath)) {
        printf("\n── Displacement sweep on A_ellipse_n01000.csv ──────────────\n");
        printf("%-46s %6s %6s %10s %10s %16s\n",
               "file", "n", "target", "time(ms)", "vmΔ(KB)", "areal_disp");
        printf("%s\n", std::string(100, '-').c_str());

        std::map<int, burd> ref;
        parseCsv(sweepPath, ref);
        int n = totalVertices(ref);

        // Targets: 5, 10, 15, … 95 % of n
        std::vector<int> targets;
        for (int pct = 5; pct <= 95; pct += 5)
            targets.push_back(std::max(MIN_TARGET, (int)(n * pct / 100.0)));
        // Remove duplicates
        std::sort(targets.begin(), targets.end());
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

        for (int t : targets) {
            RunResult r;
            bool ok = runOneIsolated(argv[0], sweepPath, t, r);
            if (!ok) {
                r = runOne(ref, t);
            }
            long vm_delta = std::max(0L, r.vmpeak_after_kb - r.vmpeak_before_kb);

            printf("%-46s %6d %6d %10.1f %10ld %16.4e\n",
                   "A_ellipse_n01000.csv", n, t,
                   r.elapsed_us / 1000.0, vm_delta, r.areal_disp);

            jw.record("sweep", "A_ellipse_n01000.csv", n, t,
                      r.elapsed_us, r.vmpeak_before_kb, r.vmpeak_after_kb,
                      r.areal_disp);
        }
    } else {
        printf("\n[bench] Note: %s not found; skipping sweep.\n",
               sweepPath.c_str());
    }

    printf("\nResults written to %s\n", RESULT_FILE.c_str());
    
    // Analyze and print summary
    analyzeResults(RESULT_FILE);
    
    return 0;
}
