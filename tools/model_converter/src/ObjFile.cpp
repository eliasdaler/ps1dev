#include "ObjFile.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
bool isBlankLine(const std::string& str)
{
    for (const auto& c : str) {
        if (!std::isspace(c)) {
            return false;
        }
    }
    return true;
}

float stringToFloat(const std::string& str)
{
    float res;
    std::from_chars(str.data(), str.data() + str.size(), res);
    return res;
}

std::vector<std::string> splitString(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream ss(s);
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void rtrim(std::string& s)
{
    s.erase(
        std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
            .base(),
        s.end());
}

ObjFace parseFaceString(const std::string& line, int linenum)
{
    const auto ts = splitString(line, ' '); // line = f attr1/attr2/attr3 ...

    ObjFace face{};
    face.numVertices = static_cast<int>(ts.size() - 1);
    if (face.numVertices > 4) {
        std::cout << face.numVertices << "-gon on line " << linenum << ", skipping..." << std::endl;
        return face;
    }

    assert(face.numVertices == 3 || face.numVertices == 4);
    for (std::size_t i = 1; i < ts.size(); ++i) {
        const auto ts2 = splitString(ts[i], '/'); // ts[i] = attr1/attr2/attr3
        if (face.numAttrs == 0) {
            face.numAttrs = ts2.size();
        } else if (ts2.size() != face.numAttrs) {
            std::cout << "Line: " << linenum
                      << ", skipping face: inconsistent number of attributes. First seen "
                      << face.numAttrs << ", but now got " << ts.size() << std::endl;
            return {};
        }
        assert(face.numAttrs != 0);
        for (int attr = 0; attr < face.numAttrs; ++attr) {
            face.indices[i - 1][attr] = std::stoi(ts2[attr]);
            assert(face.indices[i - 1][attr] >= 0);
        }
    }

    return face;
}
}

ObjModel parseObjFile(const std::filesystem::path& path)
{
    std::ifstream objfile(path);
    if (!objfile.good()) {
        std::cerr << "Failed to open obj file " << path << std::endl;
        std::exit(1);
    }

    std::string line;

    bool firstSubmesh = true;

    ObjModel model;
    ObjMesh mesh;

    // TODO: make optional
    bool mergeSubmeshes = true;

    int linenum = 0;
    while (std::getline(objfile, line)) {
        ++linenum;
        if (line.starts_with("#")) {
            continue;
        }
        if (line.empty() || isBlankLine(line)) {
            continue;
        }
        if (line.starts_with("mtllib")) {
            continue; // TODO: mtl
        }

        if (line.starts_with("o ")) {
            const auto ts = splitString(line, ' ');
            if (!firstSubmesh && !mergeSubmeshes) {
                model.meshes.push_back(std::move(mesh));
            } else {
                firstSubmesh = false;
            }
            mesh.name = ts[1];
            rtrim(mesh.name);
        } else if (line.starts_with("v ")) {
            const auto ts = splitString(line, ' ');
            model.vertices.push_back(Vec3<float>{
                .x = stringToFloat(ts[1]),
                .y = stringToFloat(ts[2]),
                .z = stringToFloat(ts[3]),
            });
        } else if (line.starts_with("vt ")) {
            const auto ts = splitString(line, ' ');
            model.uvs.push_back(Vec2<float>{
                .x = stringToFloat(ts[1]),
                .y = stringToFloat(ts[2]),
            });
        } else if (line.starts_with("vn ")) {
            const auto ts = splitString(line, ' ');
            model.normals.push_back(Vec3<float>{
                .x = stringToFloat(ts[1]),
                .y = stringToFloat(ts[2]),
                .z = stringToFloat(ts[3]),
            });
        } else if (line.starts_with("f ")) {
            ObjFace face = parseFaceString(line, linenum);
            if (face.numVertices != 0 && face.numVertices <= 4) {
                mesh.faces.push_back(std::move(face));
            }
        }
    }

    if (!mesh.name.empty()) {
        if (mergeSubmeshes) {
            mesh.name = "merged";
        }
        model.meshes.push_back(std::move(mesh));
    }

    for (const auto& mesh : model.meshes) {
        std::cout << "Submesh: " << mesh.name << ", num faces: " << mesh.faces.size() << std::endl;
    }

    return model;
}
