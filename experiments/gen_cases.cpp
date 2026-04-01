/*  gen_cases.cpp
 *
 *  Generates synthetic polygon CSV test datasets for the APSC experimental
 *  evaluation.  No external dependencies — uses only <cmath>, <fstream>,
 *  <filesystem>, and standard C++17.
 *
 *  Output directory: experiments/custom_inputs/
 *
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// Output directory
static const std::string OUT_DIR = "experiments/custom_inputs";

// Basic 2-D point
struct Pt { double x, y; };

// Write a set of rings (ring 0 = outer boundary, rings 1+ = holes) to a CSV.
static void writeCsv(const std::string& filename,
                     const std::vector<std::vector<Pt>>& rings)
{
    std::string path = OUT_DIR + "/" + filename;
    std::ofstream f(path);
    f << "ring_id,vertex_id,x,y\n";
    size_t total = 0;
    for (int rid = 0; rid < (int)rings.size(); ++rid) {
        for (int vid = 0; vid < (int)rings[rid].size(); ++vid) {
            f << rid << "," << vid << ","
              << rings[rid][vid].x << "," << rings[rid][vid].y << "\n";
        }
        total += rings[rid].size();
    }
    printf("  Written %-42s  (%zu verts, %zu rings)\n",
           filename.c_str(), total, rings.size());
}

// Generators for different test cases

// Ellipse (counter-clockwise, centred at origin)
static std::vector<Pt> makeEllipse(int n, double rx = 1000.0, double ry = 600.0)
{
    std::vector<Pt> pts(n);
    for (int i = 0; i < n; ++i) {
        double a = 2.0 * M_PI * i / n;
        pts[i] = { rx * std::cos(a), ry * std::sin(a) };
    }
    return pts;
}

// Regular polygon (CCW by default, or CW if reverse=true)
static std::vector<Pt> makeRegular(double cx, double cy, double r,
                                   int sides, bool reverse_cw = false,
                                   double offset = 0.0)
{
    std::vector<Pt> pts(sides);
    for (int i = 0; i < sides; ++i) {
        double a = offset + 2.0 * M_PI * i / sides;
        pts[i] = { cx + r * std::cos(a), cy + r * std::sin(a) };
    }
    if (reverse_cw) std::reverse(pts.begin(), pts.end());
    return pts;
}

// Tiny deterministic LCG
struct LCG {
    uint64_t s;
    LCG(uint64_t seed) : s(seed) {}
    // Returns a uniform double in [lo, hi)
    double next(double lo, double hi) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        double t = (double)(s >> 33) / (double)(1ULL << 31);
        return lo + t * (hi - lo);
    }
};

// Spiky polygon: base circle + random outward spikes
// long collinear runs (cost ≈ 0) between sharp spikes (high cost)
// many zero-cost operations flood the priority queue
static std::vector<Pt> makeSpiky(int n, double base_r = 500.0,
                                  double spike_amp = 200.0,
                                  double spike_frac = 0.10,
                                  uint64_t seed = 42)
{
    LCG rng(seed);
    int n_spikes = std::max(3, (int)(n * spike_frac));

    // Space spikes evenly around the ring
    std::vector<bool> is_spike(n, false);
    for (int i = 0; i < n_spikes; ++i) {
        int pos = (int)(i * (double)n / n_spikes);
        is_spike[std::min(pos, n - 1)] = true;
    }

    std::vector<Pt> pts(n);
    for (int i = 0; i < n; ++i) {
        double a = 2.0 * M_PI * i / n;
        double r = base_r + (is_spike[i] ? spike_amp : 0.0);
        pts[i] = { r * std::cos(a), r * std::sin(a) };
    }
    return pts;
}

// Fractal / coastline ring (midpoint displacement on initial square)
// self-similar rough boundary; displacement values span many orders
// of magnitude, stressing heap ordering precision.
static std::vector<Pt> makeFractal(int depth, double radius = 500.0,
                                    double roughness = 0.4, uint64_t seed = 7)
{
    LCG rng(seed);

    // Start with 4 points on the unit circle (square vertices)
    std::vector<Pt> pts = {
        { std::cos(0.0),          std::sin(0.0)          },
        { std::cos(M_PI / 2.0),   std::sin(M_PI / 2.0)   },
        { std::cos(M_PI),         std::sin(M_PI)          },
        { std::cos(3.0 * M_PI / 2.0), std::sin(3.0 * M_PI / 2.0) }
    };

    double scale = roughness;
    for (int d = 0; d < depth; ++d) {
        std::vector<Pt> next;
        int m = (int)pts.size();
        for (int i = 0; i < m; ++i) {
            Pt p = pts[i];
            Pt q = pts[(i + 1) % m];
            double mx = (p.x + q.x) * 0.5;
            double my = (p.y + q.y) * 0.5;
            // Inward normal of pq (perpendicular to segment)
            double nx = -(q.y - p.y);
            double ny =  (q.x - p.x);
            double len = std::hypot(nx, ny);
            if (len > 1e-12) { nx /= len; ny /= len; }
            double disp = rng.next(-scale, scale);
            next.push_back(p);
            next.push_back({ mx + disp * nx, my + disp * ny });
        }
        pts = next;
        scale *= 0.6;   // reduce roughness each level
    }

    // Scale to world coordinates
    for (auto& p : pts) { p.x *= radius; p.y *= radius; }
    return pts;
}

// Narrow-gap: outer CCW circle + inner CW circle (concentric)
// tiny gap between outer ring and hole
// topology checker must block collapses that would jump across the gap
static std::vector<std::vector<Pt>> makeNarrowGap(int n,
                                                    double outer_r = 500.0,
                                                    double gap     = 2.0)
{
    double inner_r = std::max(outer_r * 0.1, outer_r - gap);
    std::vector<Pt> outer(n), inner(n);
    for (int i = 0; i < n; ++i) {
        double a = 2.0 * M_PI * i / n;
        outer[i] = { outer_r * std::cos(a), outer_r * std::sin(a) };
        inner[i] = { inner_r * std::cos(a), inner_r * std::sin(a) };
    }
    std::reverse(inner.begin(), inner.end()); 
    return { outer, inner };
}

// Grid of hexagonal holes inside a square outer ring
// many independent interior rings
// ringActiveCount weighting applies per ring and the
// global topology check must scan O(total_n) segments every iteration
static std::vector<std::vector<Pt>> makeGridHoles(int n_holes)
{
    // Outer square (4 vertices, CCW)
    std::vector<Pt> outer = { {-800,-800}, {800,-800}, {800,800}, {-800,800} };
    std::vector<std::vector<Pt>> rings = { outer };

    int cols = (int)std::ceil(std::sqrt((double)n_holes));
    int rows = (n_holes + cols - 1) / cols;
    double xs = 1400.0 / cols;
    double ys = 1400.0 / rows;
    double hr = std::min(xs, ys) * 0.28;   // hole radius

    int count = 0;
    for (int r = 0; r < rows && count < n_holes; ++r) {
        for (int c = 0; c < cols && count < n_holes; ++c) {
            double cx = -700.0 + c * xs + xs * 0.5;
            double cy = -700.0 + r * ys + ys * 0.5;
            rings.push_back(makeRegular(cx, cy, hr, 10, true));
            ++count;
        }
    }
    return rings;
}

// Mixed: large ellipse outer + multiple medium hexagonal holes
// cross-ring topology checks between rings of very different sizes
static std::vector<std::vector<Pt>> makeMixed(int outer_n,
                                               int n_holes   = 6,
                                               int hole_sides = 12,
                                               double hole_r  = 200.0)
{
    auto outer = makeEllipse(outer_n, 2000.0, 1500.0);
    std::vector<std::vector<Pt>> rings = { outer };
    for (int h = 0; h < n_holes; ++h) {
        double a  = 2.0 * M_PI * h / n_holes;
        double cx = 1000.0 * std::cos(a);
        double cy =  750.0 * std::sin(a);
        rings.push_back(makeRegular(cx, cy, hole_r, hole_sides,true));
    }
    return rings;
}

int main()
{
    fs::create_directories(OUT_DIR);
    printf("Generating test CSVs into %s/\n\n", OUT_DIR.c_str());

    // A: Smooth ellipse

    // isolate algorithmic complexity with no holes and smooth geometry
    // Sizes span three orders of magnitude so both O(n²) and O(n log n) regimes
    // are visible
    printf("A: Smooth ellipse (vertex-count scaling)\n");
    for (int n : { 50, 100, 200, 500, 1000, 2000, 5000, 10000 }) {
        char name[64]; snprintf(name, sizeof(name), "A_ellipse_n%05d.csv", n);
        writeCsv(name, { makeEllipse(n) });
    }

    // B: Many holes

    // ringActiveCount per-ring penalty and multi-ring topology checks
    printf("\nB: Many holes (hole-count scaling)\n");
    for (int h : { 2, 4, 8, 16, 32 }) {
        char name[64]; snprintf(name, sizeof(name), "B_holes_h%02d.csv", h);
        writeCsv(name, makeGridHoles(h));
    }

    // C: Narrow gap

    // near-topology violations; high collapse-rejection rate
    printf("\nC: Narrow gap (size axis, gap=2)\n");
    for (int n : { 20, 40, 80, 160, 320 }) {
        char name[64]; snprintf(name, sizeof(name), "C_gap_size_n%04d.csv", n);
        writeCsv(name, makeNarrowGap(n, 500.0, 2.0));
    }

    // C2: Narrow gap

    // Gap varies from extremely narrow (0.5) to comfortable (50)
    printf("\nC2: Narrow gap (gap-size axis, n=80)\n");
    const std::vector<std::pair<int, double>> gaps = {
        {5, 0.5}, {10, 1.0}, {50, 5.0}, {200, 20.0}, {500, 50.0}
    };
    for (auto [g10, gap] : gaps) {
        char name[64]; snprintf(name, sizeof(name), "C_gap_g%04d.csv", g10);
        writeCsv(name, makeNarrowGap(80, 500.0, gap));
    }

    // D: Spiky / near-degenerate

    // many zero-cost collapses (collinear runs) mixed with a few high-cost spikes
    printf("\nD: Spiky / near-degenerate\n");
    for (int n : { 60, 120, 250, 500, 1000 }) {
        char name[64]; snprintf(name, sizeof(name), "D_spiky_n%04d.csv", n);
        writeCsv(name, { makeSpiky(n) });
    }

    // E: Fractal boundary

    // displacement costs spanning orders of magnitude
    printf("\nE: Fractal / coastline-like boundary\n");
    for (int depth : { 4, 5, 6, 7, 8 }) { 
        auto ring = makeFractal(depth);
        char name[64];
        snprintf(name, sizeof(name), "E_fractal_n%05d.csv", (int)ring.size());
        writeCsv(name, { ring });
    }

    // F: Mixed multi-ring

    // large outer ring + several medium holes; outer ring size varies
    printf("\nF: Mixed multi-ring (outer-n scaling)\n");
    for (int n : { 200, 500, 1000, 2000 }) {
        char name[64]; snprintf(name, sizeof(name), "F_mixed_n%04d.csv", n);
        writeCsv(name, makeMixed(n));
    }

    printf("\nAll done.  %s/ is ready.\n", OUT_DIR.c_str());
    return 0;
}
