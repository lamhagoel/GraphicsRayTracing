#include <cmath>
#include <iostream>

#include "light.h"
#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>

using namespace std;

double DirectionalLight::distanceAttenuation(const glm::dvec3 &) const {
  // distance to light is infinite, so f(di) goes to 0.  Return 1.
  return 1.0;
}

glm::dvec3 DirectionalLight::shadowAttenuation(const ray &r,
                                               const glm::dvec3 &p) const {
  // YOUR CODE HERE:
  // You should implement shadow-handling code here.
	ray new_ray = r;
	isect i;
	// check if we have intersection
	if (getScene()->intersect (new_ray, i)) {
		glm::dvec3 point = new_ray.at(i.getT());
		// check if the object is translucent
		if (i.getMaterial().Trans()) {
			// // shoot new ray 
			// ray r_trans(new_ray);
			// r_trans.setPosition(point);
			// // second intersection
			// isect iItranlucid;
			// // confirm we have second intersection
			// if (getScene()->intersect(r_trans, iItranlucid)) {
      if (glm::dot(new_ray.getDirection(), i.getN()) > 0) {
        double d = i.getT();
        glm::dvec3 multiply = glm::pow (i.getMaterial().kt(i), glm::dvec3(d, d, d));
        new_ray.setPosition(new_ray.at(i) + i.getN() * RAY_EPSILON);
        return multiply * shadowAttenuation(new_ray, p);
      }
      else {
        new_ray.setPosition(new_ray.at(i) - i.getN() * RAY_EPSILON);
        return shadowAttenuation(new_ray, p);
      }
			// }
		}
		return glm::dvec3(0, 0, 0);
	}

	return glm::dvec3(1,1,1);
	
}

glm::dvec3 DirectionalLight::getColor() const { return color; }

glm::dvec3 DirectionalLight::getDirection(const glm::dvec3 &) const {
  return -orientation;
}

double PointLight::distanceAttenuation(const glm::dvec3 &P) const {
  using namespace std;
  // YOUR CODE HERE

  // You'll need to modify this method to attenuate the intensity
  // of the light based on the distance between the source and the
  // point P.  For now, we assume no attenuation and just return 1.0
  // These three values are the a, b, and c in the distance attenuation function
  // (from the slide labelled "Intensity drop-off with distance"):
  //    f(d) = min( 1, 1/( a + b d + c d^2 ) )
  // float constantTerm;  // a
  // float linearTerm;    // b
  // float quadraticTerm; // c
  double d = glm::length(position - P);
  double result = 1.0/(constantTerm + linearTerm * d + quadraticTerm * pow(d, 2.0));
  if (result > 1) {
    result = 1.0;
  }
  return result;
}

glm::dvec3 PointLight::getColor() const { return color; }

glm::dvec3 PointLight::getDirection(const glm::dvec3 &P) const {
  return glm::normalize(position - P);
}

glm::dvec3 PointLight::shadowAttenuation(const ray &r,
                                         const glm::dvec3 &p) const {
	// TODO: handle when the light reflect inside the object?
	ray new_ray = r;
	isect i;
	// check if we have intersection
	if (getScene()->intersect (new_ray, i)) {
		// now we check if the distance point to light is less than distance point to intersection
		glm::dvec3 point = new_ray.at(i.getT());
		double d1 = glm::distance(p, point);
		double d2 = glm::distance(position, p);
		if (d1 <= d2) {
			// check if the object is translucent
			if (i.getMaterial().Trans()) {
				// // shoot new ray
				// ray r_trans(new_ray);
				// r_trans.setPosition(point);	
				// // second intersection
				// isect iItranlucid;
				// // confirm we have second intersection
				// if (getScene()->intersect(r_trans, iItranlucid)) {
				// 	// TODO: confirm if we get the distance like this
				// 	// double d = glm::distance(iItranlucid.getT(), i.getT());
				// 	// changed:
				// 	double d = iItranlucid.getT();
				// 	// double d = glm::distance(point, p);
				// 	glm::dvec3 multiply = glm::pow (i.getMaterial().kt(i), glm::dvec3(d, d, d));
				// 	r_trans.setPosition(r_trans.at(iItranlucid));
				// 	return multiply * shadowAttenuation(r_trans, r_trans.at(iItranlucid));
				// } 

        if (glm::dot(new_ray.getDirection(), i.getN()) > 0) {
          double d = i.getT();
          glm::dvec3 multiply = glm::pow (i.getMaterial().kt(i), glm::dvec3(d, d, d));
          new_ray.setPosition(new_ray.at(i) + i.getN() * RAY_EPSILON);
          return multiply * shadowAttenuation(new_ray, p);
        }
        else {
          new_ray.setPosition(new_ray.at(i) - i.getN() * RAY_EPSILON);
          return shadowAttenuation(new_ray, p);
        }
			}
			return glm::dvec3(0, 0, 0);
		}
	}

	return glm::dvec3(1,1,1);
}

#define VERBOSE 0
