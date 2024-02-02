#include "trimesh.h"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <float.h>
#include <string.h>
#include "../ui/TraceUI.h"
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
  // YOUR CODE HERE
  //
  // FIXME: Add ray-trimesh intersection

  /* To determine the color of an intersection, use the following rules:
     - If the parent mesh has non-empty `uvCoords`, barycentrically interpolate
       the UV coordinates of the three vertices of the face, then assign it to
       the intersection using i.setUVCoordinates().
     - Otherwise, if the parent mesh has non-empty `vertexColors`,
       barycentrically interpolate the colors from the three vertices of the
       face. Create a new material by copying the parent's material, set the
       diffuse color of this material to the interpolated color, and then 
       assign this material to the intersection.
     - If neither is true, assign the parent's material to the intersection.
  */
  // vertices
  glm::dvec3 A = this->parent->vertices[0];
  glm::dvec3 B = this->parent->vertices[0];
  glm::dvec3 C = this->parent->vertices[0];
  // get normal through vertices
  glm::dvec3 N = glm::normalize(glm::cross(B - A, C - A));
  // TODO: confirm
  // checking bug of shadows with t >= epsilon (10^-6)
  glm::dvec3 direction = r.getDirection();
  glm::dvec3 origin = r.getPosition();
  double t_num = glm::dot(A, N);
  t_num = t_num - glm::dot(origin, N);
  double t = t_num / glm::dot(direction, N);
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
  glm::dvec3 C_A = C - A;
  glm::dvec3 check_1_1 = glm::cross(B_A, P_A);
  glm::dvec3 check_2_1 = glm::cross(C_B, P_B);
  glm::dvec3 check_3_1 = glm::cross(A_C, P_C);
  bool check1 = glm::dot(check_1_1, N) >= 0;
  bool check2 = glm::dot(check_2_1, N) >= 0;
  bool check3 = glm::dot(check_3_1, N) >= 0;
  if (!check1 || !check2 || !check3) {
    return false;
  }
  // we have a collision

  // get the barycentric coordinates, we choose A as our p1, B as p2 and C as p3
  double m2 = (glm::dot(B_A, B_A) * glm::dot(P_A, B_A)) + (glm::dot(B_A, C_A) * glm::dot(P_A, C_A));
  double m3 = (glm::dot(C_A, B_A) * glm::dot(P_A, B_A)) + (glm::dot(C_A, C_A) * glm::dot(P_A, C_A));
  double m1 = 1 - m2 - m3;

  // check if inside the triangle considering also the bug of 0.00000001
  if (m1 >= 0 && m2 >= 0 && m3 >= 0 && (m1 + m2 + m3 <= 1.0 + 0.00000001) && (m1 + m2 + m3 >= 1.0 + 0.00000001)) {
    i.setBary(m1, m2, m3);
    i.setT(t);
    // TODO: confirm if it is like this
    i.setObject(this->parent);
     // I think we can set the normal by check this boolean this->parent->vertNorms, bc how the json is read
    if (this->parent->vertNorms) {
      glm::dvec3 n1 = m1 * parent->normals[ids[0]];
      glm::dvec3 n2 = m2 * parent->normals[ids[1]];
      glm::dvec3 n3 = m3 * parent->normals[ids[2]];
      glm::dvec3 new_normal = glm::normalize(n1 + n2 + n3);
      i.setN(new_normal);
    } else {
      i.setN(N);
    }
   
    if (!(this->parent->uvCoords).empty()) {

    } else if (!(this->parent->vertColors).empty()) {

    } else {
      i.setMaterial(this->parent->getMaterial());
    }
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

