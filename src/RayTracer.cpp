// The main ray tracer.
#pragma warning(disable : 4786)

#include "RayTracer.h"

#include "scene/material.h"
#include "scene/ray.h"

#include "parser/JsonParser.h"
#include "parser/Parser.h"
#include "parser/Tokenizer.h"
#include <json.hpp>

#include "ui/TraceUI.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>
#include <string.h> // for memset

#include <fstream>
#include <iostream>
#include <stdio.h>
#include "scene/light.h"

using namespace std;
extern TraceUI *traceUI;

// TODO: what to do with light illuminating a back face of an object from inside the object
// TODO: The Phong illumination model only really makes sense for opaque objects. So you'll
// have to decide how to apply it in a reasonable way (to you) for translucent objects
// TODO: whether light hitting the inside (back) face of an object should leave a specular highlight

// Use this variable to decide if you want to print out debugging messages. Gets
// set in the "trace single ray" mode in TraceGLWindow, for example.
bool debugMode = false;

// Constant to stop the supersampling recursion
static const double supersampling_recursion = 4;

static const bool doJitterAntiAliasing = false;

// Early termination parameters
static const glm::dvec3 reflection_treshold = glm::dvec3(0.0001, 0.0001, 0.0001);
static const glm::dvec3 refrac_reflect_treshold = glm::dvec3(0.001, 0.001, 0.001);

// Trace a top-level ray through pixel(i,j), i.e. normalized window coordinates
// (x,y), through the projection plane, and out into the scene. All we do is
// enter the main ray-tracing method, getting things started by plugging in an
// initial ray weight of (0.0,0.0,0.0) and an initial recursion depth of 0.


glm::dvec3 RayTracer::trace(double x, double y) {
  // Clear out the ray cache in the scene for debugging purposes,
  if (TraceUI::m_debug) {
    scene->clearIntersectCache();
  }
  // pp position or origin, dd direction, w: wavelength? ray's power?, tt = raytype=VISIBILITY
  // TODO:confirm
  ray r(glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 0), glm::dvec3(1, 1, 1),
        ray::VISIBILITY);
  // rayThrough(): Ray through normalized window point x,y.  In normalized coordinates
  // the camera's x and y vary both vary from 0 to 1.
  // rayThrough calls:
  // r.setPosition(eye); eye = glm::dvec3(0, 0, 0);
  // r.setDirection(dir); dir = glm::normalize(look + x * u + y * v);
  scene->getCamera().rayThrough(x, y, r);
  double dummy;
  glm::dvec3 threshold = glm::dvec3(1.0, 1.0, 1.0);
  // traceUI->getDepth() returns the max depth of recursion 
  // glm::dvec3(1.0, 1.0, 1.0) = &thresh: Threshold for interpolation within block?
  glm::dvec3 ret = 
      traceRay(r, threshold, traceUI->getDepth(), dummy);
  // Returns min(max(x, minVal), maxVal) for each component in x using the floating-point
  // values minVal and maxVal.
  // Restrict the values to lie between 0 and 1
  ret = glm::clamp(ret, 0.0, 1.0);
  return ret;
}

// computes the color of a specific pixel (i, j) in a ray-traced scene and updates 
// the pixel buffer with this color.
glm::dvec3 RayTracer::tracePixel(int i, int j) {
  glm::dvec3 col(0, 0, 0);

  if (!sceneLoaded())
    return col;

  unsigned char *pixel = buffer.data() + (i + j * buffer_width) * 3;
  if(!traceUI->aaSwitch()) {
    // cout<<"TracePixel: "<<buffer_width<<" "<<buffer_height<<"\n";
    // calculates the address of the pixel in the buffer, *3 bc there are 3 colors?

    // normalized window coordinates (x,y)
    double x = double(i) / double(buffer_width);
    double y = double(j) / double(buffer_height);

    col = trace(x, y);
  } else if (!doJitterAntiAliasing) {
    unsigned int *numRays = aaNumRaysPerPixel.data() + (i + j * buffer_width);
    glm::dvec2 center = glm::dvec2((i + 0.5)/double(buffer_width), (j + 0.5)/double(buffer_height));
    col = adaptative_supersampling(center, 1, numRays[0]);
  } else {
    // Jitter (Stochastic) Anti-Aliasing
    for (int k = 0; k < samples; k++) {
      for (int l = 0; l < samples; l++) {
        // We wanna make sure that after adding the random noise, the new x,y are still in the planned subpixel
        double jitterX = (((double)rand()) / RAND_MAX)/(double(buffer_width) * samples);
        double jitterY = (((double)rand()) / RAND_MAX)/(double(buffer_height) * samples);
        double x = (double(i) + double(k)/samples)/double(buffer_width) + jitterX;
        double y = (double(j) + double(l)/samples)/double(buffer_height) + jitterY;

        col += trace(x,y);
      }
    }
    col = col / (double)(samples*samples);
  }
    // map the values in col (in the range [0, 1]) to integers in the range [0, 255] RGB colors
    pixel[0] = (int)(255.0 * col[0]);
    pixel[1] = (int)(255.0 * col[1]);
    pixel[2] = (int)(255.0 * col[2]);
  
  return col;
}
// TODO: include memorize hash or matrix
glm::dvec3 RayTracer::adaptative_supersampling(glm::dvec2 center, int depth, unsigned int &numRays) {

  // cout<<"RayTracer.cpp RayTracer::adaptative_supersampling "<<center[0]<<" "<<center[1]<<" "<<depth<<"\n";

  // unsigned char *pixel = tempBuffer + (i + j * buffer_width) * 3;
  glm::dvec3 traced_center = trace(center.x, center.y);

  glm::dvec3 top_left = trace(center.x - pow(0.5, depth)/(double) buffer_width, center.y - pow(0.5, depth)/(double) buffer_height);
  glm::dvec3 top_right = trace(center.x - pow(0.5, depth)/(double) buffer_width, center.y + pow(0.5, depth)/(double) buffer_height);
  glm::dvec3 bottom_left = trace(center.x + pow(0.5, depth)/(double) buffer_width, center.y - pow(0.5, depth)/(double) buffer_height);
  glm::dvec3 bottom_right = trace(center.x + pow(0.5, depth)/(double) buffer_width, center.y + pow(0.5, depth)/(double) buffer_height);

  numRays += 5;
  if (depth > supersampling_recursion) {
    return ((double)4 * traced_center + top_left + top_right + bottom_left + bottom_right) / (double)8;
  }

  double color_dist1 = (double)colour_dist(top_left, traced_center);
  double color_dist2 = (double)colour_dist(top_right, traced_center);
  double color_dist3 = (double)colour_dist(bottom_left, traced_center);
  double color_dist4 = (double)colour_dist(bottom_right, traced_center);

  glm::dvec3 result(0, 0, 0);

  if (color_dist1 > aaThresh) {
    result += adaptative_supersampling(glm::dvec2(center.x - pow(0.5, depth+1)/(double) buffer_width, center.y - pow(0.5, depth+1)/(double) buffer_height), depth + 1, numRays);
  }
  else {
    result += (traced_center + top_left)/ (double)2;
  }

  if (color_dist2 > aaThresh) {
    result += adaptative_supersampling(glm::dvec2(center.x - pow(0.5, depth+1)/(double) buffer_width, center.y + pow(0.5, depth+1)/(double) buffer_height), depth + 1, numRays);
  }
  else {
    result += (traced_center + top_right)/ (double)2;
  }

  if (color_dist3 > aaThresh) {
    result += adaptative_supersampling(glm::dvec2(center.x + pow(0.5, depth+1)/(double) buffer_width, center.y - pow(0.5, depth+1)/(double) buffer_height), depth + 1, numRays);
  }
  else {
    result += (traced_center + bottom_left)/ (double)2;
  }

  if (color_dist4 > aaThresh) {
    result += adaptative_supersampling(glm::dvec2(center.x + pow(0.5, depth+1)/(double) buffer_width, center.y + pow(0.5, depth+1)/(double) buffer_height), depth + 1, numRays);
  }
  else {
    result += (traced_center + bottom_right)/ (double)2;
  }
  
  result = result / (double)4;
  return result;

}

double RayTracer::colour_dist(glm::dvec3 e1, glm::dvec3 e2) {
    glm::dvec3 e1_255 = 255.0 * e1;
    glm::dvec3 e2_255 = 255.0 * e2;
    long rmean = ((long) e1_255.x + (long) e2_255.x) / 2;
    long r = (long)e1_255.x - (long)e2_255.x;
    long g = (long)e1_255.y - (long)e2_255.y;
    long b = (long)e1_255.z - (long)e2_255.z;
    return std::sqrt((((512 + rmean) * r * r) >> 8 ) + ((g * g) << 2) + (((767 - rmean) * b * b) >> 8));
}

#define VERBOSE 0

// Do recursive ray tracing! You'll want to insert a lot of code here (or places
// called from here) to handle reflection, refraction, etc etc.
glm::dvec3 RayTracer::traceRay(ray &r, const glm::dvec3 &thresh, int depth,
                               double &t) {
  isect i;
  glm::dvec3 colorC;
#if VERBOSE
  std::cerr << "== current depth: " << depth << std::endl;
#endif
  // Get any intersection with an object.  Return information about the
  // intersection through the reference parameter.
  if (scene->intersect(r, i)) {
    // YOUR CODE HERE

    // An intersection occurred!  We've got work to do. For now, this code gets
    // the material for the surface that was intersected, and asks that material
    // to provide a color for the ray.

    // This is a great place to insert code for recursive ray tracing. Instead
    // of just returning the result of shade(), add some more steps: add in the
    // contributions from reflected and refracted rays.

    const Material &m = i.getMaterial();
    // asks the material to provide a color for the ray.
    // TODO: shade to determine the light contribution
    colorC = m.shade(scene.get(), r, i);

    if (depth <= 0) {
      return colorC;
    }

    // TODO: include traceUI->getMaxDepth() which returns the max recursion limit?
    // it is not necessary since the HW will be graded with recursion = 5

    // Reflection: recursively call traceRay
    // if depth > 0 and the material is reflective we consider reflections
    glm::dvec3 pos = r.at(i.getT());
    if (m.Refl() && depth > 0) {
      // r.at(i.getT()) gives you the intersection of ray with the surface: p + (t * d)
      // in this case r.at(i.getT()) = position
      glm::dvec3 w_in = r.getDirection();
      glm::dvec3 N = i.getN();
      // direction: w_ref normalize
      glm::dvec3 w_ref = glm::normalize(w_in - 2 * glm::dot(N, w_in)*N);
      ray r_reflection(pos, w_ref, glm::dvec3(1, 1, 1),
          ray::REFLECTION);
      glm::dvec3 thresh_refl = thresh * m.kr(i);
      if (thresh_refl.x > reflection_treshold.x || thresh_refl.y > reflection_treshold.y ||
        thresh_refl.z > reflection_treshold.z) {
        colorC += m.kr(i) * traceRay(r_reflection, thresh_refl, depth - 1, t);
      }
    }

    // TODO: for refraction he said mantain a stack to know if u are inside or outside an object
    
    // Refraction
    if (m.Trans() && depth > 0) {
      // refractive index m.index(i);
      // transmission angle
      glm::dvec3 V = -1.0 * r.getDirection();
      // if cos < 0 we are inside the object and the normal should be negative
      // if cos > 0 we are comming from outside of the object, so normal stay positive
      double cos_i = glm::dot(i.getN(), V);
      glm::dvec3 N = i.getN();
      double n_1;
      double n_2;
      // TODO: what if cos_i > 0, <0 or = 0, maybe replace the first if for cos_i and add elseif and else
      if (cos_i > 0) { // entering the object
        n_1 = 1.0;
        n_2 = m.index(i);
      } else if (cos_i < 0) { // we are inside an object, therefore, exiting the object
        n_1 = m.index(i);
        n_2 = 1.0;
        // normal should be negative
        N = -1.0 * N;
        // therefore it changes the cos_i
        cos_i = glm::dot(N, V);
      } else {
				N = glm::dvec3 (0,0,0);
			}
      double cos_i_2 = pow(cos_i, 2);
      double eta = n_1 / n_2;
      double cos_t_2 = 1 - pow(eta, 2) * (1 - cos_i_2);
      double cos_t = glm::sqrt(cos_t_2);

      // check if we consider total internal reflection or not
      if (cos_t_2 >= RAY_EPSILON) { // we have refraction
        // TODO: confirm signs, maybe I forgot to change something
        //changed
        glm::dvec3 t_refract = glm::normalize(glm::refract(r.getDirection(), N, eta));
        // glm::dvec3 t_refract = glm::normalize((eta * cos_i - cos_t) * N - (eta * V));
        // changed
        ray r_refraction(pos, t_refract, glm::dvec3(1, 1, 1), ray::REFRACTION);
        // ray r_refraction(pos, t_refract, glm::dvec3(1, 1, 1), ray::REFRACTION);
        // TODO: scale by distance
        isect iRefract; 
        double d = 1.0;
        if (scene->intersect(r_refraction, iRefract)) {
          double d = iRefract.getT();
        }
        // TODO: confirm if m.kt(i) goes here
        // colorC += traceRay(r_refraction, thresh, depth - 1, t) * pow(m.kt(i), glm::dvec3(d));
        colorC += traceRay(r_refraction, thresh, depth - 1, t);
      } else if (m.Refl()) { // the square root is imaginary so we have total internal reflection
        // TODO: since the reference does not have this, confirm if it's better w/o this.
        glm::dvec3 r_t_reflection = glm::normalize(-r.getDirection() + 2 * glm::dot(r.getDirection(), N) * N);
        ray t_i_reflection(pos, r_t_reflection, glm::dvec3(1, 1, 1), ray::REFLECTION);
        isect iReflect; 
        double d = 1.0;
        if (scene->intersect(t_i_reflection, iReflect)) {
          double d = iReflect.getT();
        }
        // TODO: confirm if we multiply by m.kr(i) or not here
        // colorC += m.kr(i) * traceRay(t_i_reflection, thresh, depth - 1, t);
        // changed:
        glm::dvec3 reflection_kr_index = m.kr(i) * pow(m.kt(i), glm::dvec3(d));
        glm::dvec3 thresh_refrac = thresh * reflection_kr_index;
        if (thresh_refrac.x > refrac_reflect_treshold.x && thresh_refrac.y > refrac_reflect_treshold.y &&
        thresh_refrac.z > refrac_reflect_treshold.z) {
          colorC += reflection_kr_index * traceRay(t_i_reflection, thresh_refrac, depth - 1, t);
        }
      }   
    }
    return colorC;

  } else {
    // No intersection. This ray travels to infinity, so we color
    // it according to the background color, which in this (simple)
    // case is just black.
    //
    // FIXME: Add CubeMap support here.
    // TIPS: CubeMap object can be fetched from
    // traceUI->getCubeMap();
    //       Check traceUI->cubeMap() to see if cubeMap is loaded
    //       and enabled.
    if (traceUI->cubeMap()) {
	colorC = traceUI->getCubeMap()->getColor(r);
    } else {
	colorC = glm::dvec3(0.0, 0.0, 0.0);
    }
  }
#if VERBOSE
  std::cerr << "== depth: " << depth + 1 << " done, returning: " << colorC
            << std::endl;
#endif
  return colorC;
}

RayTracer::RayTracer()
    : scene(nullptr), buffer(0), thresh(0), buffer_width(0), buffer_height(0),
      m_bBufferReady(false) {
}

RayTracer::~RayTracer() {}

void RayTracer::getBuffer(unsigned char *&buf, int &w, int &h) {
  buf = buffer.data();
  w = buffer_width;
  h = buffer_height;
}

double RayTracer::aspectRatio() {
  return sceneLoaded() ? scene->getCamera().getAspectRatio() : 1;
}

bool RayTracer::loadScene(const char *fn) {
  ifstream ifs(fn);
  if (!ifs) {
    string msg("Error: couldn't read scene file ");
    msg.append(fn);
    traceUI->alert(msg);
    return false;
  }

  // Check if fn ends in '.ray'
  bool isRay = false;
  const char *ext = strrchr(fn, '.');
  if (ext && !strcmp(ext, ".ray"))
    isRay = true;

  // Strip off filename, leaving only the path:
  string path(fn);
  if (path.find_last_of("\\/") == string::npos)
    path = ".";
  else
    path = path.substr(0, path.find_last_of("\\/"));

  if (isRay) {
    // .ray Parsing Path
    // Call this with 'true' for debug output from the tokenizer
    Tokenizer tokenizer(ifs, false);
    Parser parser(tokenizer, path);
    try {
      scene.reset(parser.parseScene());
    } catch (SyntaxErrorException &pe) {
      traceUI->alert(pe.formattedMessage());
      return false;
    } catch (ParserException &pe) {
      string msg("Parser: fatal exception ");
      msg.append(pe.message());
      traceUI->alert(msg);
      return false;
    } catch (TextureMapException e) {
      string msg("Texture mapping exception: ");
      msg.append(e.message());
      traceUI->alert(msg);
      return false;
    }
  } else {
    // JSON Parsing Path
    try {
      JsonParser parser(path, ifs);
      scene.reset(parser.parseScene());
    } catch (ParserException &pe) {
      string msg("Parser: fatal exception ");
      msg.append(pe.message());
      traceUI->alert(msg);
      return false;
    } catch (const json::exception &je) {
      string msg("Invalid JSON encountered ");
      msg.append(je.what());
      traceUI->alert(msg);
      return false;
    }
  }

  if (!sceneLoaded())
    return false;

  return true;
}

void RayTracer::traceSetup(int w, int h) {
  size_t newBufferSize = w * h * 3;
  if (newBufferSize != buffer.size()) {
    bufferSize = newBufferSize;
    buffer.resize(bufferSize);
  }
  buffer_width = w;
  buffer_height = h;
  std::fill(buffer.begin(), buffer.end(), 0);
  m_bBufferReady = true;

  /*
   * Sync with TraceUI
   */

  threads = traceUI->getThreads();
  block_size = traceUI->getBlockSize();
  thresh = traceUI->getThreshold();
  samples = traceUI->getSuperSamples();
  aaThresh = traceUI->getAaThreshold()*1000; // We revert the multiplication by 0.001 here because we wanna use the original value instead of scaling it between 0 to 1.

  if (traceUI->aaSwitch() && !doJitterAntiAliasing) {
    size_t imageSize = w * h;
    if (imageSize != aaNumRaysPerPixel.size()) {
      aaNumRaysPerPixel.resize(imageSize);
    }
    std::fill(aaNumRaysPerPixel.begin(), aaNumRaysPerPixel.end(), 0);
  }

  // YOUR CODE HERE
  // FIXME: Additional initializations
}

/*
 * RayTracer::traceImage
 *
 *	Trace the image and store the pixel data in RayTracer::buffer.
 *
 *	Arguments:
 *		w:	width of the image buffer
 *		h:	height of the image buffer
 *
 */
void RayTracer::traceImage(int w, int h) {
  // Always call traceSetup before rendering anything.
  traceSetup(w, h);
  scene->buildBVH();
  for (int i = 0; i < w; i++)
  {
    for (int j = 0; j < h; j++) {
      tracePixel(i, j);
    }
  }

  /*
  * Uncomment this piece of code to output the antialiasing ray sampling intensity instead of the output raytraced image
  */
  // if (traceUI->aaSwitch() && !doJitterAntiAliasing)
  // {
  //   // cout<< "aaSwitch is on\n";
  //   unsigned int minVal = aaNumRaysPerPixel.data()[0], maxVal = aaNumRaysPerPixel.data()[0];

  //   for (int i=0; i<w; i++) {
  //     for (int j=0; j < h; j++) {
  //       int loc = (i + j * w);
  //       unsigned int val = (aaNumRaysPerPixel.data() + loc)[0];
  //       // cout<<val<<" ";
  //       if (minVal > val) {
  //         minVal = val;
  //       }
  //       if (maxVal < val) {
  //         maxVal = val;
  //       }

  //     }
  //     // cout<<"\n";

  //     for (int i=0; i<w; i++) {
  //       for (int j=0; j<h; j++) {
  //         int loc = (i + j * w);
  //         unsigned int val = (aaNumRaysPerPixel.data() + loc)[0];
  //         val = ((val-minVal)*255/(maxVal-minVal));
  //         unsigned char *pixel = buffer.data() + loc * 3;
  //         pixel[0] = pixel[1] = pixel[2] = val;
  //       }
  //     }
  //   }
  // }
  
  // YOUR CODE HERE
  // FIXME: Start one or more threads for ray tracing
  //
  // TIPS: Ideally, the traceImage should be executed asynchronously,
  //       i.e. returns IMMEDIATELY after working threads are launched.
  //
  //       An asynchronous traceImage lets the GUI update your results
  //       while rendering.
}

int RayTracer::aaImage() {
  // YOUR CODE HERE
  // FIXME: Implement Anti-aliasing here

  // WE HAVE REMOVED THIS CODE BECAUSE WE ARE NOW DOING ADAPTIVE ANTI-ALIASING NOW INSTEAD

//   // Nothing to do if we weren't oversampling
//   if (samples<=1) {
//     return 0;
//   }

//   int overSamplingFactor = samples*samples;
//   int imageSize = buffer.size()/overSamplingFactor;

//   int w = 0, h = 0;
//   unsigned char *buf = NULL;
//   getBuffer(buf, w, h);
//   w = w/samples;
//   h = h/samples;

//   if (imageSize != w * h * 3) {
//     // Current buffer size is not as expected - maybe aaImage has been called previously?
//     // CommandLineUI explicitly calls it, but graphical UI doesn't I think?
//     cout<<"Double call to aaImage?\n";
//     return 0;
//   }

//   std::vector<unsigned char> tempBuffer(imageSize);

//   // bool bufferzeros = std::all_of(buffer.begin(), buffer.end(), [](int i) { return i==0; });

//   for (int j = 0; j < h; j++) {
//     for (int i = 0; i < w; i++) {
//       unsigned char *pixel = tempBuffer.data() + (i + j * w) * 3;
//       // long int sum0 = 0, sum1 = 0, sum2 = 0; // TODO: Can we change this to a vector instead?
//       glm::dvec3 sum = glm::dvec3(0, 0, 0);
//       for (int l = 0; l < samples; l++) {
//         for (int k = 0; k < samples; k++) {
//           unsigned char *bufferLoc = buf + (j*w*samples*samples + l*w*samples + i*samples + k) * 3;
//           sum += glm::dvec3((int)bufferLoc[0], (int)bufferLoc[1], (int)bufferLoc[2]);
//           // sum0 += (int)bufferLoc[0];
//           // sum1 += (int)bufferLoc[1];
//           // sum2 += (int)bufferLoc[2];
//         }
//       }
//       pixel[0] = (int)(sum.x/overSamplingFactor);
//       pixel[1] = (int)(sum.y/overSamplingFactor);
//       pixel[2] = (int)(sum.z/overSamplingFactor);
//     }
//   }

//   // bool tempBufferZeros = std::all_of(tempBuffer.begin(), tempBuffer.end(), [](int i) { return i==0; });

//   size_t newBufferSize = imageSize;;
//   if (newBufferSize != buffer.size()) {
//     bufferSize = newBufferSize;
//     buffer.resize(bufferSize);
//   }
//   buffer_width = w;
//   buffer_height = h;
//   buffer.assign(tempBuffer.begin(), tempBuffer.end());

  // std::fill(buffer.begin(), buffer.end(), 0);
  // m_bBufferReady = true;

  // TIP: samples and aaThresh have been synchronized with TraceUI by
  //      RayTracer::traceSetup() function
  return 0;
}

bool RayTracer::checkRender() {
  // YOUR CODE HERE
  // FIXME: Return true if tracing is done.
  //        This is a helper routine for GUI.
  //
  // TIPS: Introduce an array to track the status of each worker thread.
  //       This array is maintained by the worker threads.
  return true;
}

void RayTracer::waitRender() {
  // YOUR CODE HERE
  // FIXME: Wait until the rendering process is done.
  //        This function is essential if you are using an asynchronous
  //        traceImage implementation.
  //
  // TIPS: Join all worker threads here.
}


glm::dvec3 RayTracer::getPixel(int i, int j) {
  unsigned char *pixel = buffer.data() + (i + j * buffer_width) * 3;
  return glm::dvec3((double)pixel[0] / 255.0, (double)pixel[1] / 255.0,
                    (double)pixel[2] / 255.0);
}

void RayTracer::setPixel(int i, int j, glm::dvec3 color) {
  unsigned char *pixel = buffer.data() + (i + j * buffer_width) * 3;

  pixel[0] = (int)(255.0 * color[0]);
  pixel[1] = (int)(255.0 * color[1]);
  pixel[2] = (int)(255.0 * color[2]);
}
