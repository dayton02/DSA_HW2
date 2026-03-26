#include <cmath>
#include <vector>
#include <map>
#include <queue>

struct VertexData{
    int ring_id;
    int vertex_id;
    double x;
    double y;
};
#define burd std::vector<VertexData>

struct Point{double x,y;};

//Calculate the signed distance between pt and line AB
//Where is the point relative to A and D
double signedDist(Point pt, Point A, Point D);
//Calculate area fo 3 points using shoelace method
double triArea(Point i, Point j, Point k);
bool lineIntersect(Point p1, Point p2, Point p3, Point p4, Point& out);
void eLineTwoPoints(double a, double b, double c, Point& e1, Point& e2);
Point placement(Point A, Point B, Point C, Point D);
double displacementArea(Point A, Point B, Point C, Point D, Point E);
bool segmentsIntersect(Point p1, Point p2, Point p3, Point p4);
bool topologyCheck(const std::vector<Point>& poly, int iA, int iB, int iC, int iD, Point E);
std::vector<Point> burdToPoints(const burd& ring);
burd pointsToBurd(const std::vector<Point>& pts, int ring_id);
burd apscRing(const burd& ring, int targetVertices, bool doTopoCheck);