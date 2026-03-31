#include "apsc.hpp"
#include <algorithm>

//#define burd std::vector<VertexData>

double getPolygonArea(Point P1, Point P2, Point P3, Point P4) {
    auto intersect = [](Point p1, Point p2, Point p3, Point p4, Point& X) {
        double det = (p2.x - p1.x)*(p4.y - p3.y) - (p2.y - p1.y)*(p4.x - p3.x);
        if (std::abs(det) < 1e-12) return false;
        
        double t = ((p3.x - p1.x)*(p4.y - p3.y) - (p3.y - p1.y)*(p4.x - p3.x)) / det;
        double u = ((p3.x - p1.x)*(p2.y - p1.y) - (p3.y - p1.y)*(p2.x - p1.x)) / det;
        
        // STRICT BOUNDS: The lines must cross exactly within the segments
        if (t >= -1e-8 && t <= 1.0 + 1e-8 && u >= -1e-8 && u <= 1.0 + 1e-8) {
            X = {p1.x + t*(p2.x - p1.x), p1.y + t*(p2.y - p1.y)};
            return true;
        }
        return false;
    };

    Point X;
    // Case 1: P1-P2 crosses P3-P4
    if (intersect(P1, P2, P3, P4, X)) {
        return std::abs(triArea(P1, P4, X)) + std::abs(triArea(P2, P3, X));
    }
    // Case 2: P2-P3 crosses P4-P1
    if (intersect(P2, P3, P4, P1, X)) {
        return std::abs(triArea(P1, P2, X)) + std::abs(triArea(P3, P4, X));
    }
    // Case 3: Simple polygon (no crossing)
    return std::abs(triArea(P1, P2, P3) + triArea(P1, P3, P4));
}

//1) Calculate the signed distance between pt and line AB
//Where is the point relative to A and D
double signedDist(Point pt, Point A, Point D)
{
    double dx = D.x - A.x; //Length X of AD
    double dy = D.y - A.y; //Length Y of AD
    double len = std::hypot(dx,dy); //Length of AD
    if(len<1e-12) return 0.0; 
    return((pt.x - A.x) * dy - (pt.y - A.y) * dx)/len;
}
//2) Calculate area fo 3 points using shoelace method
double triArea(Point i, Point j, Point k)
{
    return 0.5* ((j.x-i.x) * (k.y-i.y) - (k.x-i.x)*(j.y-i.y));
}
//3) Intersect two lines which are infinitely long
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
    double t =((p3.x - p1.x) * d2y - (p3.y-p1.y)*d2x)/det; //parameter
    out = {p1.x+t*d1x, p1.y + t*d1y}; //Intersection point
    return true;
}
//4) Compute area-preserving line for ABCD, collapses BC to E
void computeELine(Point A, Point B, Point C, Point D, double& a, double& b, double &c)
{
 a = D.y - A.y; //X variable of normal AD
 b = A.x - D.x; //Y variable of normal AD
 c = -B.y*A.x + (A.y - C.y) * B.x + (B.y - D.y) * C.x + C.y * D.x; 
}

//5) Convert line coefficients into two points on the line. Can be used to intersect with AB or CD
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

//6) Decides on whether to collapse BC into AD or CD depending on geometry
Point placement(Point A, Point B, Point C, Point D)
{
    double a, b, c;
    computeELine(A, B, C, D, a, b, c); // Get the line coefficients (ax + by + c = 0)
    Point e1, e2;
    eLineTwoPoints(a, b, c, e1, e2); // Two points on E

    // Candidate 1: Place E on the line extending AB
    Point E_AB = B;
    bool valid_AB = lineIntersect(A, B, e1, e2, E_AB);
    double disp_AB = valid_AB ? getPolygonArea(E_AB, B, C, D) : std::numeric_limits<double>::max();

    // Candidate 2: Place E on the line extending CD
    Point E_CD = C;
    bool valid_CD = lineIntersect(C, D, e1, e2, E_CD);
    double disp_CD = valid_CD ? getPolygonArea(A, B, C, E_CD) : std::numeric_limits<double>::max();

    // Candidate 3: Place E on the line extending BC
    Point E_BC = B;
    bool valid_BC = lineIntersect(B, C, e1, e2, E_BC);
    // When E is on BC, the symmetric difference is exactly the sum of the two triangles ABE and CDE
    double disp_BC = valid_BC ? (std::abs(triArea(A, B, E_BC)) + std::abs(triArea(C, D, E_BC))) : std::numeric_limits<double>::max();

    // Fallback if all line extensions are completely parallel to the E-line
    if (!valid_AB && !valid_CD && !valid_BC) {
        return B;
    }

    // Return the optimal placement that directly minimizes the areal displacement
    if (disp_AB <= disp_CD && disp_AB <= disp_BC) return E_AB;
    if (disp_CD <= disp_AB && disp_CD <= disp_BC) return E_CD;
    return E_BC;
}

// Calculates the sum of the two lobes of a self-intersecting "bowtie" quadrilateral
double getBowtieArea(Point P1, Point P2, Point P3, Point P4) {
    auto intersect = [](Point p1, Point p2, Point p3, Point p4, Point& X, double& t, double& u) {
        double det = (p2.x - p1.x)*(p4.y - p3.y) - (p2.y - p1.y)*(p4.x - p3.x);
        if (std::abs(det) < 1e-12) return false;
        t = ((p3.x - p1.x)*(p4.y - p3.y) - (p3.y - p1.y)*(p4.x - p3.x)) / det;
        u = ((p3.x - p1.x)*(p2.y - p1.y) - (p3.y - p1.y)*(p2.x - p1.x)) / det;
        X = {p1.x + t*(p2.x - p1.x), p1.y + t*(p2.y - p1.y)};
        return true;
    };

    Point X1, X2; 
    double t1, u1, t2, u2;
    bool cross1 = intersect(P1, P2, P3, P4, X1, t1, u1);
    bool cross2 = intersect(P2, P3, P4, P1, X2, t2, u2);
    
    // Score based on how close the intersection is to the center of the segments
    // This perfectly handles floating-point noise on extreme scales
    double score1 = cross1 ? std::max(std::abs(t1 - 0.5), std::abs(u1 - 0.5)) : 1e9;
    double score2 = cross2 ? std::max(std::abs(t2 - 0.5), std::abs(u2 - 0.5)) : 1e9;

    if (score1 < score2 && score1 < 2.0) { 
        // Pair 1 crosses (True Bowtie)
        return std::abs(triArea(X1, P2, P3)) + std::abs(triArea(X1, P4, P1));
    } else if (score2 < 2.0) { 
        // Pair 2 crosses (True Bowtie)
        return std::abs(triArea(X2, P3, P4)) + std::abs(triArea(X2, P1, P2));
    }
    
    // Fallback for extreme floating point degenerate cases (acts as simple polygon)
    return std::abs(triArea(P1, P2, P3) + triArea(P1, P3, P4));
}


// THE FIXED DISPLACEMENT FUNCTION
double displacementArea(Point A, Point B, Point C, Point D, Point E) {
    double area_AB = std::abs(triArea(A, B, E));
    double area_CD = std::abs(triArea(C, D, E));
    double area_BC = std::abs(triArea(B, C, E));

    // Determine which line E was placed on by finding the degenerate (zero-area) triangle.
    // Using comparisons safely bypasses floating-point noise.
    if (area_BC <= area_AB && area_BC <= area_CD) {
        // E is perfectly aligned with BC
        return area_AB + area_CD;
    } else if (area_AB <= area_CD) {
        // E is perfectly aligned with AB
        return getPolygonArea(E, B, C, D);
    } else {
        // E is perfectly aligned with CD
        return getPolygonArea(A, B, C, E);
    }
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
    return (t > 1e-9 && t < 1- 1e-9 && u > 1e-9 && u <1-1e-9);
}

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

    if (activeCount <= targetVertices) return;

    struct Op { int a, b, c, d; Point E; double disp; int vA, vB, vC, vD; };
    std::map<int, Op> ops;

    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    int nextId = 0;

    auto addOp = [&](int a) {
        int b = nodes[a].next;
        int c = nodes[b].next;
        int d = nodes[c].next;
        
        // Protect holes: Never let a ring drop below 4 vertices!
        if (ringActiveCount[nodes[a].ring_id] <= 4) return;
        if (a == b || a == c || a == d) return; 
        
        Point E = placement(nodes[a].p, nodes[b].p, nodes[c].p, nodes[d].p);
        double dis = displacementArea(nodes[a].p, nodes[b].p, nodes[c].p, nodes[d].p, E);
        
        int id = nextId++;
        ops[id] = {a, b, c, d, E, dis, nodes[a].version, nodes[b].version, nodes[c].version, nodes[d].version};
        pq.push({dis, id});
    };

    for (int i = 0; i < (int)nodes.size(); i++) addOp(i);

    // 2. Globally collapse the absolute best candidates
    while (activeCount > targetVertices && !pq.empty()) {
        auto[disp, id] = pq.top(); 
        pq.pop();

        if (ops.find(id) == ops.end()) continue;
        Op op = ops[id];
        ops.erase(id); 

        if (!nodes[op.a].active || !nodes[op.b].active || !nodes[op.c].active || !nodes[op.d].active) continue;
        if (nodes[op.a].next != op.b || nodes[op.b].next != op.c || nodes[op.c].next != op.d) continue;
        if (nodes[op.a].version != op.vA || nodes[op.b].version != op.vB || 
            nodes[op.c].version != op.vC || nodes[op.d].version != op.vD) continue;

        if (doTopoCheck && !topologyCheckGlobal(nodes, op.a, op.b, op.c, op.d, op.E)) continue;

        nodes[op.b].p = op.E;         
        nodes[op.b].version++;        
        nodes[op.c].active = false;   

        nodes[op.b].next = op.d;
        nodes[op.d].prev = op.b;

        activeCount--;
        ringActiveCount[nodes[op.b].ring_id]--;
        outDisplacement += op.disp; 

        int curr = nodes[nodes[nodes[op.b].prev].prev].prev;
        for (int i = 0; i < 4; i++) {
            addOp(curr);
            curr = nodes[curr].next;
        }
    }

    // 3. Reconstruct the separated rings
    rings.clear();
    std::map<int, int> ring_starts;
    for(int i = 0; i < (int)nodes.size(); i++) {
        if (nodes[i].active && ring_starts.find(nodes[i].ring_id) == ring_starts.end()) {
            ring_starts[nodes[i].ring_id] = i;
        }
    }
    
    for (auto const& [ring_id, start_idx] : ring_starts) {
        int curr = start_idx;
        int v_id = 0;
        do {
            rings[ring_id].push_back({ring_id, v_id++, nodes[curr].p.x, nodes[curr].p.y});
            curr = nodes[curr].next;
        } while (curr != start_idx);
    }
}