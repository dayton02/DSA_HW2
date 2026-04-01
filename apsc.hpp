#pragma once
#include <cmath>
#include <vector>
#include <map>
#include <queue>

// Data structures for vertices, nodes, and operations
struct VertexData{
    int ring_id;
    int vertex_id;
    double x;
    double y;
};

// A unified node structure for all rings, with links and metadata for the simplification process
using burd = std::vector<VertexData>;

// Node structure for the global linked list representation of all vertices across rings
struct Point{double x,y;};
struct Node {
    Point p;
    int prev;
    int next;
    bool active; // Set to false when collapsed
    int vertex_id; // Keep track for final output
    int version;  // Tracks coordinate changes
    int ring_id;  // Which ring this node belongs to
};

// Math & Geometry Functions
double signedDist(Point pt, Point A, Point D);
double triArea(Point i, Point j, Point k);
bool lineIntersect(Point p1, Point p2, Point p3, Point p4, Point& out);
void eLineTwoPoints(double a, double b, double c, Point& e1, Point& e2);
Point placement(Point A, Point B, Point C, Point D);
double displacementArea(Point A, Point B, Point C, Point D, Point E);
bool segmentsIntersect(Point p1, Point p2, Point p3, Point p4);

// ALGORITHM FUNCTIONS
bool topologyCheck(const std::vector<Node>& nodes, int iA, int iB, int iC, int iD, Point E);
std::vector<Node> burdToNodes(const burd& ring);
burd nodesToBurd(const std::vector<Node>& nodes, int ring_id);
void apscPolygon(std::map<int, burd>& rings, int targetVertices, bool doTopoCheck, double& outDisplacement);