#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "apsc.hpp"

/*
Things I need to do
1. Read CSV file and obtain values
2. Calculate area
3. Visualization
4. Implement algo
*/

// struct VertexData{
//     int ring_id;
//     int vertex_id;
//     double x;
//     double y;
// };
// #define burd std::vector<VertexData>

burd supervertex;

//Do rings
std::map<int, burd> rings;
std::map<int, burd> simplifiedRings;
std::map<int, burd> answer;

std::string formatCoord(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    std::string text = stream.str();

    while (!text.empty() && text.back() == '0')
    {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.')
    {
        text.pop_back();
    }
    if (text == "-0")
    {
        text = "0";
    }

    return text;
}

//Calculate Area
double calculateArea(burd input)
{
    //Init stuff
    int n = int(input.size());
    double area = 0.f;
    //int j = n -1;
    for(int i = 0;i<n;i++)
    {
        int j = (i+1) % n;
        //Sheolace
        area += input[i].x * input[j].y - input[j].x * input[i].y;
        //area+= (input[j].x + input[i].x) * (input[j].y - input[i].y);
    }
    return std::abs(area)/2.0;
}
//Print everything!!!
void printSuper(burd input)
{
    for(VertexData& x: input)
    {
        std::cout<< x.ring_id<<","
        <<x.vertex_id<<","
        <<x.x<<","
        <<x.y<<"\n";
    }
}



double calculateAllAreas(std::map<int, burd> input)
{
    double total_area = 0.0;
    for(std::pair<const int, burd> x: input)
    {
        total_area += calculateArea(x.second);
    }
    //std::cout << "The area for everything is: " << std::fixed << std::setprecision(3)<< total_area << std::endl;
    return total_area;
}

void printToFile(std::ofstream& file, std::map<int, burd> input)
    {
            //file
            //process input to rings then rings to burd

        file<<"ring_id,vertex_id,x,y\n";
        for(auto& x: input)
        {
            for(auto& y: x.second)
            {
                file << std::fixed << std::setprecision(3) << y.ring_id<<","
                <<y.vertex_id<<","
                <<formatCoord(y.x)<<","
                <<formatCoord(y.y)<<"\n";
            }
        }
        //calculateAllAreas(input);

        double input_area = calculateAllAreas(rings);
        double simplified_area = calculateAllAreas(simplifiedRings);
        file << "Total signed area in input: " << std::scientific << std::setprecision(6) << input_area << "\n";
        file << "Total signed area in output: " << std::scientific << std::setprecision(6) << simplified_area << "\n";
        file << "Total areal displacement: " << std::scientific << std::setprecision(6) << std::abs(input_area - simplified_area) << "\n";
    }

//Input CSV file and read
void exportSVG(const std::string& outpath, std::map<int,burd> input)
{
    double minX=1e9, minY=1e9, maxX=-1e9, maxY=-1e9;
    for (auto& [id, verts] : input)
        for (auto& v : verts) {
            minX = std::min(minX, v.x); minY = std::min(minY, v.y);
            maxX = std::max(maxX, v.x); maxY = std::max(maxY, v.y);
        }

    double pad = 50.0;
    double W = (maxX - minX) + pad * 2;
    double H = (maxY - minY) + pad * 2;

    // Scale up small datasets so they aren't tiny in the SVG
    double dataExtent = std::max(maxX - minX, maxY - minY);
    double scale = (dataExtent < 100.0) ? (500.0 / dataExtent) : 1.0;
    W *= scale;
    H *= scale;
    pad *= scale;

    // tx/ty: shift world origin into SVG space
    // SVG Y is flipped, so we translate to the bottom-left of the data area
    double tx = pad - minX * scale;
    double ty = H - pad + minY * scale;  // bottom of data in SVG coords

    std::ofstream out(outpath);
    out << std::fixed << std::setprecision(2);
    out << "<svg xmlns='http://www.w3.org/2000/svg' "
        << "width='" << W << "' height='" << H << "' "
        << "style='background:#0d0f14'>\n";

    // Flip Y axis: translate origin, then scale(1,-1) to invert Y
    out << "<g transform='translate(" << tx << "," << ty << ") scale(" << scale << ",-" << scale << ")'>\n";

    const std::vector<std::string> colors = {"#4af0c4", "#f06a4a", "#a06af0"};

    // Compound even-odd fill (outer blob minus holes)
    out << "<path fill-rule='evenodd' fill='rgba(74,240,196,0.10)' stroke='none' d='";
    for (auto& [id, verts] : input) {
        out << "M " << verts[0].x << " " << verts[0].y;
        for (size_t i = 1; i < verts.size(); ++i)
            out << " L " << verts[i].x << " " << verts[i].y;
        out << " Z ";
    }
    out << "'/>\n";

    // Per-ring outlines, dots, and labels
    // Note: stroke-width and r are divided by scale so they appear at a
    // constant pixel size despite the parent transform scaling them up
    double sw  = 2.0  / scale;   // stroke width in world units
    double vr  = 4.0  / scale;   // vertex dot radius
    double fsz = 10.0 / scale;   // font size

    for (auto& [id, verts] : input) {
        std::string color = colors[std::min(id, (int)colors.size()-1)];

        // Closed polygon outline
        out << "<polygon fill='none' stroke='" << color << "' "
            << "stroke-width='" << sw << "' stroke-linejoin='round' points='";
        for (auto& v : verts)
            out << v.x << "," << v.y << " ";
        out << "'/>\n";

        // Vertex dots and labels
        for (auto& v : verts) {
            // Dot
            out << "<circle cx='" << v.x << "' cy='" << v.y << "' r='" << vr << "' "
                << "fill='#0d0f14' stroke='" << color << "' stroke-width='" << sw << "'/>\n";

            // Label: un-flip around the label anchor so text reads normally
            // Parent has scale(s,-s), so we apply scale(1/s, -1/s) to cancel it,
            // then translate to the desired screen offset from the vertex
            double lx = v.x + vr + 1.0 / scale;
            double ly = v.y + vr;
            out << "<text "
                << "transform='translate(" << lx << "," << ly << ") scale(" << (1.0/scale) << ",-" << (1.0/scale) << ")' "
                << "font-size='"<<fsz*scale<< "' fill='" << color << "' font-family='monospace' "
                << "dominant-baseline='auto'>"
                << "v" << v.vertex_id << " (" << v.x << "," << v.y << ")"
                << "</text>\n";
        }
    }

    out << "</g>\n</svg>\n";
    std::cout << "SVG written to " << outpath << "\n";
}

void ReadFileAndSaveToVector(std::string input, std::map<int, burd>& vect) {
    std::ifstream file(input);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open " << input << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // Skip CSV header

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // CHECK: If the line starts with 'T' (for "Total"), stop reading coordinates!
        if (line[0] == 'T' || !std::isdigit(line[0])) {
            break; 
        }

        std::stringstream ss(line);
        std::string part;

        try {
            std::getline(ss, part, ',');
            int ring_id = std::stoi(part);
            std::getline(ss, part, ',');
            int vertex_id = std::stoi(part);
            std::getline(ss, part, ',');
            double x = std::stod(part);
            std::getline(ss, part, ',');
            double y = std::stod(part);

            vect[ring_id].push_back({ring_id, vertex_id, x, y});
        } catch (...) {
            continue; // Skip any weird lines
        }
    }
}

void simplify(std::string input, int vertices)
{
    std::cout<<"Filepath is "<<input<<std::endl;
    std::cout<<"Number of vertices is "<<vertices<<std::endl;

    //Read the CSV file
    std::fstream file(input);
    if(!file.is_open())
    {
        std::cout<<"Unable to open file: "<<input<<std::endl;
    }
    ReadFileAndSaveToVector(input, rings); 
    //Before output
    exportSVG("input.svg", rings);
    //Simplify algo
    //calculateAllAreas(rings);

    int totalVerts = 0;
    for(auto& [id,r]: rings) totalVerts += (int)r.size();

    for(auto& [id,r]: rings)
    {
        int target =std::max(4, (int)std::round(vertices* (double)r.size()/totalVerts));
        std::cout<<"Ring "<<id<< ": "<< r.size()<< " -> " <<target <<" vertices\n";
        simplifiedRings[id] = apscRing(r, target, false);
    }

    

    
    //rings = simplifiedRings;
    //After everything done
    exportSVG("output.svg", simplifiedRings);
    std::ofstream ofile("output.txt");
    if(ofile.is_open())
    {
        printToFile(ofile, simplifiedRings);
    }
    //calculateAllAreas(simplifiedRings);


}

int main(int argc, char* argv[])
{
    //Take in CSV file
    if(argc < 3) 
    {
    std::cout<<"Usage: ./simplify <inputfile.csv> <target_vertices>\n";
    } 
    //Argv[0] is the ./simplify
    //Argv[1] is the input file & Argv[2] is the vertices
    simplify(argv[1], std::stoi(argv[2]));
    ReadFileAndSaveToVector(argv[3], answer);
    exportSVG("Answer.svg", answer);
}