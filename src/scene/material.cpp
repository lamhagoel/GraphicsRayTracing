#include "material.h"
#include "../ui/TraceUI.h"
#include "light.h"
#include "ray.h"
extern TraceUI *traceUI;

#include "../fileio/images.h"
#include <glm/gtx/io.hpp>
#include <iostream>
#include <algorithm>
#include <iostream>
using namespace std;
extern bool debugMode;

Material::~Material() {}

// Basic linear algebra operations are provided by the OpenGL Mathematics (glm) library

// Apply the phong model to this point on the surface of the object, returning
// the color of that point.
// YOUR CODE HERE

  // For now, this method just returns the diffuse color of the object.
  // This gives a single matte color for every distinct surface in the
  // scene, and that's it.  Simple, but enough to get you started.
  // (It's also inconsistent with the phong model...)

  // Your mission is to fill in this method with the rest of the phong
  // shading model, including the contributions of all the light sources.
  // You will need to call both distanceAttenuation() and
  // shadowAttenuation()
  // somewhere in your code in order to compute shadows and light falloff.
  //	if( debugMode )
  //		std::cout << "Debugging Phong code..." << std::endl;

  // When you're iterating through the lights,
  // you'll want to use code that looks something
  // like this:
  //
  // for ( const auto& pLight : scene->getAllLights() )
  // {
  //              // pLight has type Light*
  // 		.
  // 		.
  // 		.
  // }
glm::dvec3 Material::shade(Scene *scene, const ray &r, const isect &i) const {
  // first two terms before the summation
  // ke + ka*ia
  glm::dvec3 result =  ke(i) + ka(i)*(scene->ambient());

  // summation
  for ( const auto& pLight : scene->getAllLights() )
  { 
    // Difusse term
    // i.getT() gives the point on plane 
    // r.at(): it is the intersection between the ray and the plane it returns p + (t * d)
    // where p is the position and d is the direction and t the point on plane
    // getDirection(P) normalized(position of light - vector)
    // glm::normalize(position - P)
    glm::dvec3 Lambertian = pLight->getDirection(r.at(i.getT()));
    // dot product berween L and N, i.getN() returns the normal of the intersection point
    // scalar
    double Lambertian_N = abs(glm::dot(Lambertian, i.getN()));
    glm::dvec3 IDiffuse = pLight->getColor() * (kd(i)*Lambertian_N);
    // specular coefficient term double shininess(const isect &i) const
    // reflection angle: r = 2(l ⋅n)n − l
    // ks = ks(i)
    // glm::reflect	(	genType const & 	I, genType const & 	N )	
    // For the incident vector I and surface orientation N, returns the reflection direction : 
    // result = I - 2.0 * dot(N, I) * N.
    // changed:
    // glm::dvec3 R = glm::normalize(glm::reflect(r.getDirection(), i.getN()));
    glm::dvec3 R = 2 * glm::dot(i.getN(), Lambertian) * i.getN() - Lambertian;
    // scene->getCamera().getEye() gives the location of the camera eye
    // changed:
    // glm::dvec3 V = glm::normalize(scene->getCamera().getEye() - r.at(i.getT()));
    glm::dvec3 V = -r.getDirection();
    // changed
    // double V_dot_R = pow(max(0.0, glm::dot(R, V)), shininess(i));
    double V_dot_R = pow(max(0.0, glm::dot(R, V)), this->shininess(i));

    glm::dvec3 ISpecular = pLight->getColor() * ks(i)*V_dot_R;
    ray rShadow(r.at(i.getT()), pLight->getDirection(r.at(i.getT())), glm::dvec3(1,1,1), ray::SHADOW);	
    // TODO: include light attenuation
    // glm::dvec3 I_attenuation = pLight->distanceAttenuation(r.at(i.getT()))*pLight->shadowAttenuation(r, r.at(i.getT()));
    glm::dvec3 I_attenuation = min(1.0, pLight->distanceAttenuation(r.at(i.getT())))*pLight->shadowAttenuation(rShadow, r.at(i.getT()));
    result += I_attenuation * (IDiffuse + ISpecular);
  }

  return result;
}

TextureMap::TextureMap(string filename) {
  data = readImage(filename.c_str(), width, height);
  if (data.empty()) {
    width = 0;
    height = 0;
    string error("Unable to load texture map '");
    error.append(filename);
    error.append("'.");
    throw TextureMapException(error);
  }
}

glm::dvec3 TextureMap::getMappedValue(const glm::dvec2 &coord) const {
  // YOUR CODE HERE
  //
  // In order to add texture mapping support to the
  // raytracer, you need to implement this function.
  // What this function should do is convert from
  // parametric space which is the unit square
  // [0, 1] x [0, 1] in 2-space to bitmap coordinates,
  // and use these to perform bilinear interpolation
  // of the values.

  return glm::dvec3(1, 1, 1);
}

glm::dvec3 TextureMap::getPixelAt(int x, int y) const {
  // YOUR CODE HERE
  //
  // In order to add texture mapping support to the
  // raytracer, you need to implement this function.

  return glm::dvec3(1, 1, 1);
}

glm::dvec3 MaterialParameter::value(const isect &is) const {
  if (0 != _textureMap)
    return _textureMap->getMappedValue(is.getUVCoordinates());
  else
    return _value;
}

double MaterialParameter::intensityValue(const isect &is) const {
  if (0 != _textureMap) {
    glm::dvec3 value(_textureMap->getMappedValue(is.getUVCoordinates()));
    return (0.299 * value[0]) + (0.587 * value[1]) + (0.114 * value[2]);
  } else
    return (0.299 * _value[0]) + (0.587 * _value[1]) + (0.114 * _value[2]);
}
