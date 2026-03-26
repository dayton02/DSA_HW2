#include "apsc.hpp"

//#define burd std::vector<VertexData>

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
    //This is kinda the init for the calculation
    double a,b,c;
    computeELine(A, B,C, D, a, b,c); //Get da line coefficients (ax + by + c = 0)
    Point e1, e2;
    eLineTwoPoints(a, b, c, e1, e2); //Two points on E

    //Here is the calculation based on the E line made and the calculation of the new shape
    double distB = signedDist(B, A, D); //side of AD for B
    double distC = signedDist(C, A, D); //side of AD for C
    double distEline = signedDist(e1, A,D); //side of AD for E line
    bool sameAD = (distB* distC > 0.0); //B and C on the same side of AD

    bool useAB;
    if(sameAD)
    {
        useAB = (std::abs(distB) < std::abs(distC)); //Choose AB if B is further
    }
    else{
        useAB = ((distEline* distB) > 0.0); //Else check if its (+)
    }

    Point E;
    if(useAB)
    {
        if(!lineIntersect(A,B,e1,e2, E)) 
        {
            E = B;
        }
    }
    else{
        if(!lineIntersect(C,D, e1, e2, E))
        {
            E = C;
        }
    }
    return E;
}

//Total area displacement between ABCD and AED
double displacementArea(Point A, Point B, Point C, Point D, Point E)
{
    //da left region consists of triangles ABE and BCE
    double L = std::abs(triArea(A,B,E)) + std::abs(triArea(B,C,E));
    //the right region is triangle CED
    double R = std::abs(triArea(C,E,D));
    return L+R; //Add tgt to get total displacement
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
bool topologyCheck(const std::vector<Point>& poly, int iA, int iB, int iC, int iD, Point E)
{
    int n = poly.size();
    Point A = poly[iA], D = poly[iD];
    for(int i =0;i<n;i++)
    {
        int j = (i+1) % n;
        //Skip 3 segments AB, BC and CD that are beign collapsed
        //if(i == iA || i == iB || i == iC || i == iD) continue;
        if(i == iA || i == iB || i == iC) continue;
        if(segmentsIntersect(A,E, poly[i], poly[j])) return false;
        if(segmentsIntersect(E,D, poly[i], poly[j])) return false;
    }
    return true;
}

//Convert burd to vector of points
std::vector<Point> burdToPoints(const burd& ring)
{
    std::vector<Point> pts;
    pts.reserve(ring.size());
    for(auto& v: ring)
    {
        pts.push_back({v.x, v.y});
    }
    return pts;
}

burd pointsToBurd(const std::vector<Point>& pts, int ring_id)
{
    burd result;
    for(int i =0;i<(int)pts.size(); i++)
    {
        result.push_back({ring_id, i, pts[i].x, pts[i].y});
    }
    return result;
}
burd apscRing(const burd& ring, int targetVertices, bool doTopoCheck)
{
    std::vector<Point> poly = burdToPoints(ring);
    int n = poly.size();

    if(n <= targetVertices || n <4) return ring; //Do nothing

    //For each entry
    struct Op{int a, b, c ,d; Point E; double disp;};
    std::map<int ,Op> ops;

    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    int nextId = 0;

    //Register candidate for 4-verte window from i
    auto addOp = [&](int i)
    {
        int a = i,
        b = (i+1) %n,
        c = (i+2) %n,
        d = (i+3) %n;
        Point E = placement(poly[a], poly[b], poly[c], poly[d]);
        double dis =displacementArea(poly[a], poly[b], poly[c], poly[d], E);
        int id = nextId++;
        ops[id] = {a,b,c,d,E,dis};
        pq.push({dis,id});
    };
    for(int i =0;i<n;i++) addOp(i);

    //Iterate through everything in the queue then see if can make smol
    while((int)poly.size() > targetVertices && !pq.empty())
    {
        auto[disp,id] = pq.top(); pq.pop();

        //If op already invalidated
        if(ops.find(id) == ops.end()) continue;
        Op op = ops[id];
        ops.erase(id);

        if(op.a >= (int)poly.size() || op.b >= (int)poly.size()|| op.c >= (int)poly.size() || op.d >= (int)poly.size()) continue;
        if(doTopoCheck&& !topologyCheck(poly, op.a, op.b, op.c ,op.d, op.E)){
            continue;
        }
        //Collapse
        poly[op.b] = op.E;
        poly.erase(poly.begin() + op.c);
        n = poly.size();

        // std::vector<int> toErase;
        // for(auto& [oid, o] : ops)
        // {
        //     if(o.b == op.b || o.c == op.b || o.b == op.c || o.c == op.c)
        //     {
        //         toErase.push_back(oid);
        //     }
        // }
        // for(int oid: toErase)
        // {
        //     ops.erase(oid);
        // }
        // for(int offset = -2; offset <= 0; offset++)
        // {
        //     int start = ((op.b + offset) % n +n) % n;
        //     addOp(start);
        // }
        ops.clear();
        while (!pq.empty()) pq.pop();

        n = poly.size();
        for(int i = 0; i < n; i++) {
            addOp(i);
        }
    }
    return pointsToBurd(poly, ring.empty() ?0 : ring[0].ring_id);
}