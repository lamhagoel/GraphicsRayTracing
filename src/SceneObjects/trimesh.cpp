#include "trimesh.h"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <float.h>
#include <string.h>
#include "../ui/TraceUI.h"
#include <iostream>
extern TraceUI *traceUI;
extern TraceUI *traceUI;

using namespace std;

Trimesh::~Trimesh() {
  for (auto f : faces)
    delete f;
}

// must add vertices, normals, and materials IN ORDER
void Trimesh::addVertex(const glm::dvec3 &v) { vertices.emplace_back(v); }

void Trimesh::addNormal(const glm::dvec3 &n) { normals.emplace_back(n); }

void Trimesh::addColor(const glm::dvec3 &c) { vertColors.emplace_back(c); }

void Trimesh::addUV(const glm::dvec2 &uv) { uvCoords.emplace_back(uv); }

// Returns false if the vertices a,b,c don't all exist
bool Trimesh::addFace(int a, int b, int c) {
  int vcnt = vertices.size();

  if (a >= vcnt || b >= vcnt || c >= vcnt)
    return false;

  TrimeshFace *newFace = new TrimeshFace(this, a, b, c);
  if (!newFace->degen)
    faces.push_back(newFace);
  else
    delete newFace;

  // Don't add faces to the scene's object list so we can cull by bounding
  // box
  return true;
}

// Check to make sure that if we have per-vertex materials or normals
// they are the right number.
const char *Trimesh::doubleCheck() {
  if (!vertColors.empty() && vertColors.size() != vertices.size())
    return "Bad Trimesh: Wrong number of vertex colors.";
  if (!uvCoords.empty() && uvCoords.size() != vertices.size())
    return "Bad Trimesh: Wrong number of UV coordinates.";
  if (!normals.empty() && normals.size() != vertices.size())
    return "Bad Trimesh: Wrong number of normals.";

  return 0;
}

bool Trimesh::intersectLocal(ray &r, isect &i) const {
  bool have_one = false;
  for (auto face : faces) {
    isect cur;
    if (face->intersectLocal(r, cur)) {
      if (!have_one || (cur.getT() < i.getT())) {
        i = cur;
        have_one = true;
      }
    }
  }
  if (!have_one)
    i.setT(1000.0);
  return have_one;
}

bool TrimeshFace::intersect(ray &r, isect &i) const {
  return intersectLocal(r, i);
}


// Intersect ray r with the triangle abc.  If it hits returns true,
// and put the parameter in t and the barycentric coordinates of the
// intersection in u (alpha) and v (beta).
bool TrimeshFace::intersectLocal(ray &r, isect &i) const {
  glm::dvec3 A = parent->vertices[ids[0]];
  glm::dvec3 B = parent->vertices[ids[1]];
  glm::dvec3 C = parent->vertices[ids[2]];
  // TODO: confirm
  // checking bug of shadows with t >= epsilon (10^-6)
  glm::dvec3 direction = r.getDirection();
  glm::dvec3 origin = r.getPosition();
  double t_num = glm::dot(A, normal);
  t_num = t_num - glm::dot(origin, normal);
  double t = t_num / glm::dot(direction, normal);
  if (t <= 0.00000001) {
    return false; 
  }
  // if P is inside, must be on correct side of lines
  glm::dvec3 P = r.at(t);
  // the following booleans should be true for the point to be inside
  glm::dvec3 B_A = B - A;
  glm::dvec3 C_B = C - B;
  glm::dvec3 A_C = A - C;
  glm::dvec3 P_A = P - A;
  glm::dvec3 P_B = P - B;
  glm::dvec3 P_C = P - C;
  glm::dvec3 C_P = C - P;
  glm::dvec3 C_A = C - A;
  glm::dvec3 B_P = B - P;
  glm::dvec3 A_P = A - P;
  glm::dvec3 check_1_1 = glm::cross(B_A, P_A);
  glm::dvec3 check_2_1 = glm::cross(C_B, P_B);
  glm::dvec3 check_3_1 = glm::cross(A_C, P_C);
  double check1 = glm::dot(normal, check_1_1);
  double check2 = glm::dot(normal, check_2_1);
  double check3 = glm::dot(normal, check_3_1);
  if ((check1 >= 0 && check2 >= 0 && check3 >= 0)) {
    // we have a collision
    i.setObject(this->parent);
    i.setN(normal);
    i.setMaterial(this->parent->getMaterial());
    i.setT(t);
    // get the barycentric coordinates
    double abc = (glm::dot(glm::cross(B_A, C_A), normal));
    double pbc = (glm::dot(glm::cross(B_P, C_P), normal));
    double pca = (glm::dot(glm::cross(C_P, A_C), normal));
    double m1 = (pbc / abc);
    double m2 = (pca / abc);
    double m3 = 1.0 - m1 - m2;

    i.setBary(m1, m2, m3);
    i.setUVCoordinates(glm::dvec2(m1, m2));

    // TODO: Phong interpolation, confirm if we need to set the normals like this
      // I think we can set the normal by check this boolean this->parent->vertNorms, bc how the json is read
    if (parent->vertNorms) {
      glm::dvec3 n1 = m1 * parent->normals[ids[0]];
      glm::dvec3 n2 = m2 * parent->normals[ids[1]];
      glm::dvec3 n3 = m3 * parent->normals[ids[2]];
      glm::dvec3 new_normal = glm::normalize(n1 + n2 + n3);
      i.setN(new_normal);
    } else {
      i.setN(normal);
    }

    // TODO: I think is milestone 2
    //  - If the parent mesh has non-empty `uvCoords`, barycentrically interpolate
    //      the UV coordinates of the three vertices of the face, then assign it to
    //      the intersection using i.setUVCoordinates().
    // if (!(this->parent->uvCoords).empty()) {

    // } else {
      
    // }

    // - Otherwise, if the parent mesh has non-empty `vertexColors`,
    //    barycentrically interpolate the colors from the three vertices of the
    //    face. Create a new material by copying the parent's material, set the
    //    diffuse color of this material to the interpolated color, and then 
    //    assign this material to the intersection.
    if (!parent->vertColors.empty()) {
      glm::dvec3 c1 = m1 * parent->vertColors[ids[0]];
      glm::dvec3 c2 = m2 * parent->vertColors[ids[1]];
      glm::dvec3 c3 = m3 * parent->vertColors[ids[2]];
      glm::dvec3 new_color = glm::normalize(c1 + c2 + c3);
      // confirm if & before m (&m) or not
      // Material m = this->parent->getMaterial();
      Material m = parent->getMaterial();
      m.setDiffuse(new_color);
      i.setMaterial(m);
    } else {
      i.setMaterial(parent->getMaterial());
    }
    return true;
  }
  return false;
}

// Once all the verts and faces are loaded, per vertex normals can be
// generated by averaging the normals of the neighboring faces.
void Trimesh::generateNormals() {
  int cnt = vertices.size();
  normals.resize(cnt);
  std::vector<int> numFaces(cnt, 0);

  for (auto face : faces) {
    glm::dvec3 faceNormal = face->getNormal();

    for (int i = 0; i < 3; ++i) {
      normals[(*face)[i]] += faceNormal;
      ++numFaces[(*face)[i]];
    }
  }

  for (int i = 0; i < cnt; ++i) {
    if (numFaces[i])
      normals[i] /= numFaces[i];
  }

  vertNorms = true;
}
