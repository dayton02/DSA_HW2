#include "apsc.hpp"
#include <algorithm>

// 1) Calculate the signed distance between pt and line AB
// Where is the point relative to A and D
double signedDist(Point pt, Point A, Point D)
{
    double dx = D.x - A.x; //Length X of AD
    double dy = D.y - A.y; //Length Y of AD
    double len = std::hypot(dx,dy); //Length of AD
    if(len<1e-12) return 0.0; 
    return((pt.x - A.x) * dy - (pt.y - A.y) * dx)/len;
}

// 2) Calculate signed area of 3 points using shoelace formula
// Returns 2x the signed area (clockwise positive, counterclockwise negative)
inline double triArea(Point i, Point j, Point k)
{
    return i.x * (j.y - k.y) + j.x * (k.y - i.y) + k.x * (i.y - j.y);
}

// 3) Intersect two lines which are infinitely long
bool lineIntersect(Point p1, Point p2, Point p3, Point p4, Point& out)
{
    double d1x = p2.x - p1.x; //Direction vector of line 1
    double d1y = p2.y - p1.y;
    double d2x = p4.x - p3.x; //Direction vector of line 2
    double d2y = p4.y - p3.y;
    double det = d1x*d2y - d1y *d2x; //Determinant would be 0 if parallel
    if(std::abs(det) < 1e-12)
    {
        return false; //Lines parallel
    }
    if ((p1.x == p3.x && p1.y == p3.y) ||
    (p1.x == p4.x && p1.y == p4.y) ||
    (p2.x == p3.x && p2.y == p3.y) ||
    (p2.x == p4.x && p2.y == p4.y))
    return false;

    double t =((p3.x - p1.x) * d2y - (p3.y-p1.y)*d2x)/det; //parameter
    out = {p1.x+t*d1x, p1.y + t*d1y}; //Intersection point
    return true;
}

// 4) Compute area-preserving line for ABCD, collapses BC to E
// Line equation: ax + by + c = 0, where E is placed to preserve total area
void computeELine(Point A, Point B, Point C, Point D, double& a, double& b, double &c)
{
    a = D.y - A.y;  // normal to AD
    b = A.x - D.x;  // normal to AD
    c = -B.y*A.x + (A.y - C.y) * B.x + (B.y - D.y) * C.x + C.y * D.x;  // derived from area preservation
}

// 5) Convert line coefficients into two points on the line. Can be used to intersect with AB or CD
void eLineTwoPoints(double a, double b, double c, Point& e1, Point& e2)
{
    if(std::abs(b) > 1e-12)
    {
        e1 = {0.0, -c/b};
        e2 = {1.0, -(a+c)/b};
    }
    else{
        e1 = {-c/a, 0.0};
        e2 = {-c/a, 1.0};
    }
}

// 6) Decide where to place E based on the signed distances of B, C, and E↔ to line AD.
// Implements Kronenfeld et al. (2020) placement pseudo-code exactly:
//   if side(B, AD) = side(C, AD):
//       if d(B, AD) > d(C, AD): return intersection(E↔, AB)   [Fig. 4b,c]
//       else:                   return intersection(E↔, CD)
//   else:
//       if side(B, AD) = side(E↔, AD): return intersection(E↔, AB)  [Fig. 4a]
//       else:                           return intersection(E↔, CD)
Point placement(Point A, Point B, Point C, Point D)
{
    double a, b, c;
    computeELine(A, B, C, D, a, b, c);
    Point e1, e2;
    eLineTwoPoints(a, b, c, e1, e2);

    Point E_AB = B;
    bool valid_AB = lineIntersect(A, B, e1, e2, E_AB);
    Point E_CD = C;
    bool valid_CD = lineIntersect(C, D, e1, e2, E_CD);

    if (!valid_AB && !valid_CD) return B;

    double dB = signedDist(B, A, D);
    double dC = signedDist(C, A, D);

    // side(E↔, AD): use e1, a guaranteed point ON E↔, to determine which side of AD
    // the line E↔ lies on. The previous code used E_AB here, but if valid_AB is false,
    // E_AB stays as B, making dELine == dB and the else-branch condition always true —
    // incorrect. e1 is always on E↔ regardless of whether valid_AB holds.
    double dELine = signedDist(e1, A, D);

    auto sign = [](double val) { return (val > 1e-9) ? 1 : ((val < -1e-9) ? -1 : 0); };

    if (sign(dB) == sign(dC)) {
        // B and C on the same side: the one further from AD drives the optimal intersection
        if (std::abs(dB) > std::abs(dC)) return valid_AB ? E_AB : E_CD;
        else                             return valid_CD ? E_CD : E_AB;
    } else {
        // B and C on opposite sides: pick AB if B and E↔ share the same side of AD
        if (sign(dB) == sign(dELine)) return valid_AB ? E_AB : E_CD;
        else                          return valid_CD ? E_CD : E_AB;
    }
}

// Helper: like segmentsIntersect but also returns the intersection point X.
// Returns true only for strictly interior crossings (t,u ∈ (0,1)), matching
// segmentsIntersect's behaviour so shared endpoints are never flagged.
bool segmentsIntersectWithPoint(Point p1, Point p2, Point p3, Point p4, Point& X)
{
    auto cross2d = [](Point a, Point b) { return a.x * b.y - a.y * b.x; };
    auto sub     = [](Point a, Point b) -> Point { return {a.x - b.x, a.y - b.y}; };
    Point r   = sub(p2, p1);
    Point s   = sub(p4, p3);
    double det = cross2d(r, s);
    if (std::abs(det) < 1e-12) return false;
    double t = cross2d(sub(p3, p1), s) / det;
    double u = cross2d(sub(p3, p1), r) / det;
    if (t > 0.0 && t < 1.0 && u > 0.0 && u < 1.0) {
        X = {p1.x + t * r.x, p1.y + t * r.y};
        return true;
    }
    return false;
}

// Compute the areal displacement between the original path A→B→C→D and the
// simplified path A→E→D 
double displacementArea(Point A, Point B, Point C, Point D, Point E)
{
    Point X;

    // Case 1 – E placed on line CD: closed polygon A→B→C→D→E crosses on BC∩EA
    if (segmentsIntersectWithPoint(B, C, E, A, X)) {
        // Lobe containing A and B: triangle A–B–X
        double lobe1 = std::abs(triArea(A, B, X));
        // Lobe containing C, D, E: quadrilateral X–C–D–E
        double lobe2 = std::abs(triArea(X, C, D) + triArea(X, D, E));
        return (lobe1 + lobe2) * 0.5;
    }

    // Case 2 – E placed on line AB: closed polygon A→B→C→D→E crosses on BC∩DE
    if (segmentsIntersectWithPoint(B, C, D, E, X)) {
        // Lobe containing C and D: triangle X–C–D
        double lobe1 = std::abs(triArea(X, C, D));
        // Lobe containing E, A, B: quadrilateral X–E–A–B
        double lobe2 = std::abs(triArea(X, E, A) + triArea(X, A, B));
        return (lobe1 + lobe2) * 0.5;
    }

    // Fallback: degenerate case (e.g. collinear vertices, E≈B).
    // Area is already preserved so net displacement is genuinely ~0.
    return 0.0;
}

// GLOBAL TOPOLOGY CHECK: Checks every active segment across ALL rings
bool topologyCheckGlobal(const std::vector<Node>& nodes, int iA, int iB, int iC, int iD, Point E) {
    Point A = nodes[iA].p, D = nodes[iD].p;
    
    for (int curr = 0; curr < (int)nodes.size(); curr++) {
        if (!nodes[curr].active) continue;
        int next_idx = nodes[curr].next;
        
        // Skip the specific sequence being collapsed
        if (curr == iA || curr == iB || curr == iC) continue;
        
        if (segmentsIntersect(A, E, nodes[curr].p, nodes[next_idx].p)) return false;
        if (segmentsIntersect(E, D, nodes[curr].p, nodes[next_idx].p)) return false;
    }
    return true;
}

//Segment intersection test for topology check 
//Checks if the finite segments intersect and uses parametic form and cross products
bool segmentsIntersect(Point p1, Point p2, Point p3, Point p4)
{
    auto cross2d = [](Point a, Point b){return a.x * b.y - a.y* b.x;};
    auto sub = [](Point a, Point b) -> Point{return{a.x - b.x, a.y - b.y};};
    Point r = sub(p2,p1);
    Point s = sub(p4,p3);
    double det = cross2d(r,s);
    if(std::abs(det) < 1e-12){
        return false;
    }
    double t = cross2d(sub(p3,p1), s)/ det;
    double u = cross2d(sub(p3,p1), r)/det;
    return (t > 0.0 && t < 1.0 && u > 0.0 && u < 1.0);
}

// Topology check for a single collapse operation, checks if the new point E causes any intersections with existing segments across all rings
bool topologyCheck(const std::vector<Node>& nodes, int iA, int iB, int iC, int iD, Point E)
{
    Point A = nodes[iA].p, D = nodes[iD].p;
    
    int start = iA; // GUARANTEED to be active
    int curr = start;
    do {
        int next_idx = nodes[curr].next;
        if (curr == iA || curr == iB || curr == iC) {
            curr = next_idx;
            continue;
        }
        
        if (segmentsIntersect(A, E, nodes[curr].p, nodes[next_idx].p)) return false;
        if (segmentsIntersect(E, D, nodes[curr].p, nodes[next_idx].p)) return false;
        
        curr = next_idx;
    } while (curr != start);
    
    return true;
}

// Converters for Doubly Linked List
std::vector<Node> burdToNodes(const burd& ring)
{
    int n = ring.size();
    std::vector<Node> nodes(n);
    for (int i = 0; i < n; i++) {
        nodes[i] = {{ring[i].x, ring[i].y}, 
                    (i - 1 + n) % n, 
                    (i + 1) % n, 
                    true, 
                    ring[i].vertex_id,
                    0,
                    ring[i].ring_id
                }; 
    }
    return nodes;
}

// Convert from the global node list back to the output burd format, extracting only the active nodes for each ring
burd nodesToBurd(const std::vector<Node>& nodes, int ring_id)
{
    burd result;
    
    // Find the first active node to start
    int start = 0;
    while (!nodes[start].active && start < (int)nodes.size()) start++;
    
    if (start >= (int)nodes.size()) return result; // Empty ring

    int curr = start;
    int new_vertex_id = 0;
    do {
        result.push_back({ring_id, new_vertex_id++, nodes[curr].p.x, nodes[curr].p.y});
        curr = nodes[curr].next;
    } while (curr != start);
    
    return result;
}

// Main simplification function: takes the input rings, target vertex count, and outputs the simplified rings while calculating total areal displacement
void apscPolygon(std::map<int, burd>& rings, int targetVertices, bool doTopoCheck, double& outDisplacement) {
    std::vector<Node> nodes;
    std::map<int, int> ringActiveCount;
    int activeCount = 0;
    
    // 1. Unify all rings into a single Global Node Array
    for (auto& [ring_id, ring] : rings) {
        int n = ring.size();
        int start_idx = nodes.size();
        ringActiveCount[ring_id] = n;
        
        for (int i = 0; i < n; i++) {
            nodes.push_back({
                {ring[i].x, ring[i].y},
                start_idx + (i - 1 + n) % n,
                start_idx + (i + 1) % n,
                true,
                ring[i].vertex_id,
                0,
                ring_id
            });
            activeCount++;
        }
    }

    // If the total active vertices across all rings is already at or below the target, we can skip simplification
    if (activeCount <= targetVertices) return;

    // 2. Compute initial collapse costs for every vertex and add to priority queue
    struct Op { int a, b, c, d; Point E; double disp; int vA, vB, vC, vD; };
    std::map<int, Op> ops;

    // Lambda to add or update an operation for a given vertex index
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    int nextId = 0;

    auto addOp = [&](int a) {
        int b = nodes[a].next;
        int c = nodes[b].next;
        int d = nodes[c].next;
        
        // A collapse removes one vertex; keep every ring at ≥ 3 vertices (a triangle
        // is the smallest valid simple polygon).  With ≤3 active vertices the pattern
        // ABCD degenerates (d == a), which the a==d guard below also catches, but the
        // explicit count check avoids unnecessary work.  The old guard of ≤4 wrongly
        // prevented collapsing quadrilaterals to triangles.
        if (ringActiveCount[nodes[a].ring_id] <= 3) return;
        if (a == b || a == c || a == d) return; 
        
        Point E = placement(nodes[a].p, nodes[b].p, nodes[c].p, nodes[d].p);
        double dis = displacementArea(nodes[a].p, nodes[b].p, nodes[c].p, nodes[d].p, E);
        
        int id = nextId++;
        ops[id] = {a, b, c, d, E, dis, nodes[a].version, nodes[b].version, nodes[c].version, nodes[d].version};
        // Use raw displacement as the priority key, exactly as Kronenfeld et al. (2020)
        // describe: "the item with min d in worklist".  The previous code divided by
        // ringActiveCount, which is not part of the paper's algorithm and biased the
        // ordering toward smaller rings.
        pq.push({dis, id});
    };

    // Initialize the priority queue with all vertices
    for (int i = 0; i < (int)nodes.size(); i++) addOp(i);

    // Iteratively collapse the best vertex until we reach the target vertex count
    while (activeCount > targetVertices && !pq.empty()) {
        // Get the operation with the lowest weighted displacement
        auto[disp, id] = pq.top(); 
        pq.pop();

        // If the operation was invalidated by a previous collapse, skip it
        if (ops.find(id) == ops.end()) continue;
        Op op = ops[id];
        ops.erase(id); 

        // Check if the operation is still valid (vertices haven't been modified since insertion)
        if (!nodes[op.a].active || !nodes[op.b].active || !nodes[op.c].active || !nodes[op.d].active) continue;
        if (nodes[op.a].next != op.b || nodes[op.b].next != op.c || nodes[op.c].next != op.d) continue;

        // Recompute E and displacement using current geometry
        Point E = placement(nodes[op.a].p, nodes[op.b].p, nodes[op.c].p, nodes[op.d].p);
        double displ = displacementArea(nodes[op.a].p, nodes[op.b].p, nodes[op.c].p, nodes[op.d].p, E);

        // If displacement changed, reinsert and skip
        if (std::abs(displ - op.disp) > 1e-12) {
            addOp(op.a);
            continue;
        }

        // If topology check is enabled, verify that collapsing this vertex won't cause intersections
        if (doTopoCheck && !topologyCheckGlobal(nodes, op.a, op.b, op.c, op.d, op.E)) continue;

        // Perform the collapse: move B to E, remove C, and update links
        nodes[op.b].p = op.E;         
        nodes[op.b].version++;        
        nodes[op.c].active = false;   
        nodes[op.b].next = op.d;
        nodes[op.d].prev = op.b;

        // Update counts and displacement
        activeCount--;
        ringActiveCount[nodes[op.b].ring_id]--;
        outDisplacement += op.disp; 

        // Add new operations for the affected vertices (A, B, D)
        int curr = nodes[nodes[nodes[op.b].prev].prev].prev;
        for (int i = 0; i < 4; i++) {
            addOp(curr);
            curr = nodes[curr].next;
        }
    }

    // 3. Convert back to output format, extracting only active nodes for each ring
    rings.clear();
    std::map<int, int> ring_starts;
    for(int i = 0; i < (int)nodes.size(); i++) {
        if (nodes[i].active && ring_starts.find(nodes[i].ring_id) == ring_starts.end()) {
            ring_starts[nodes[i].ring_id] = i;
        }
    }
    
    // Iterate through each ring and extract the active nodes in order to reconstruct the simplified rings
    for (auto const& [ring_id, start_idx] : ring_starts) {
        int curr = start_idx;
        int v_id = 0;
        do {
            rings[ring_id].push_back({ring_id, v_id++, nodes[curr].p.x, nodes[curr].p.y});
            curr = nodes[curr].next;
        } while (curr != start_idx);
    }
}