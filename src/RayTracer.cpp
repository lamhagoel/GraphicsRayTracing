// The main ray tracer.
#pragma warning(disable : 4786)

#include "RayTracer.h"
#include "scene/light.h"
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

using namespace std;
extern TraceUI *traceUI;

// TODO: what to do with light illuminating a back face of an object from inside the object
// TODO: The Phong illumination model only really makes sense for opaque objects. So you'll 
// have to decide how to apply it in a reasonable way (to you) for translucent objects
// TODO: whether light hitting the inside (back) face of an object should leave a specular highlight

// Use this variable to decide if you want to print out debugging messages. Gets
// set in the "trace single ray" mode in TraceGLWindow, for example.
bool debugMode = false;

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
  // traceUI->getDepth() returns the max depth of recursion 
  // glm::dvec3(1.0, 1.0, 1.0) = &thresh: Threshold for interpolation within block?
  glm::dvec3 ret = 
      traceRay(r, glm::dvec3(1.0, 1.0, 1.0), traceUI->getDepth(), dummy);
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

  // normalized window coordinates (x,y)
  double x = double(i) / double(buffer_width);
  double y = double(j) / double(buffer_height);

  // cout<<"TracePixel: "<<buffer_width<<" "<<buffer_height<<"\n";
  // calculates the address of the pixel in the buffer, *3 bc there are 3 colors?
  unsigned char *pixel = buffer.data() + (i + j * buffer_width) * 3;
  col = trace(x, y);
  // map the values in col (in the range [0, 1]) to integers in the range [0, 255] RGB colors
  pixel[0] = (int)(255.0 * col[0]);
  pixel[1] = (int)(255.0 * col[1]);
  pixel[2] = (int)(255.0 * col[2]);
  return col;
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
      glm::dvec3 w_ref = glm::normalize(w_in - 2 * (glm::dot(N, w_in))*N);
      
      ray r_reflection(pos, w_ref, glm::dvec3(1, 1, 1),
          ray::REFLECTION);
      colorC += m.kr(i) * traceRay(r_reflection, thresh, depth - 1, t);
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
      if (cos_i >= 0) { // entering the object
        n_1 = 1.0;
        n_2 = m.index(i);
      } else { // we are inside an object, therefore, exiting the object
        n_1 = m.index(i);
        n_2 = 1.0;
        // normal should be negative
        N = -1.0 * N;
        // therefore it changes the cos_i
        cos_i = glm::dot(N, V);
      }
      double cos_i_2 = pow(cos_i, 2);
      double eta = n_1 / n_2;
      double cos_t_2 = 1 - pow(eta, 2) * (1 - cos_i_2);
      double cos_t = glm::sqrt(cos_t_2);

      // check if we consider total internal reflection or not
      if (cos_t_2 >= 0) { // we have refraction
        // TODO: confirm signs, maybe I forgot to change something
        glm::dvec3 t_refract = (eta * cos_i - cos_t) * N - (eta * V);
        ray r_refraction(pos, t_refract, glm::dvec3(1, 1, 1),
          ray::REFRACTION);
        colorC += traceRay(r_refraction, thresh, depth - 1, t);
      } else { // the square root is imaginary so we have total internal reflection
        // TODO: since the reference does not have this, confirm if it's better w/o this.
        glm::dvec3 r_t_reflection = glm::normalize(r.getDirection() - 2 * (glm::dot(N, r.getDirection()))*N);
        ray t_i_reflection(pos, r_t_reflection, glm::dvec3(1, 1, 1),
          ray::REFLECTION);
        // TODO: confirm if we multiply by m.kr(i) or not here
        colorC += m.kr(i) * traceRay(t_i_reflection, thresh, depth - 1, t);
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
    // CubeMap *cm = traceUI->getCubeMap();
		// if(cm){
		// 	colorC =  cm->getColor(r);
		// } else {
		// 	colorC = glm::dvec3(0.0, 0.0, 0.0);
		// }
    colorC = glm::dvec3(0.0, 0.0, 0.0);
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
  aaThresh = traceUI->getAaThreshold();

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
  cout <<"TraceImage: "<<w<<" "<<h<<" "<<samples<<"\n";
  if(traceUI->aaSwitch()) {
    samples = traceUI ->getSuperSamples();
    h = h * samples;
    w = w * samples;
  }
  //TODO: the value of samples changes after this (incorrectly if the anti-aliasing is not selected). Tried fixing it, verify.
  traceSetup(w, h);
  for (int i = 0; i < w; i++)
  {
    for (int j = 0; j < h; j++) {
      tracePixel(i, j);
    }
  }

  if(traceUI->aaSwitch()) {
    aaImage(w/samples, h/samples);
  }
  
  // YOUR CODE HERE
  // FIXME: Start one or more threads for ray tracing
  //
  // TIPS: Ideally, the traceImage should be executed asynchronously,
  //       i.e. returns IMMEDIATELY after working threads are launched.
  //
  //       An asynchronous traceImage lets the GUI update your results
  //       while rendering.
}

// TODO: last part
// TODO: Check the role of threshold??
// TODO: Verify if it works as expected
int RayTracer::aaImage(int w, int h) {
  // YOUR CODE HERE
  // FIXME: Implement Anti-aliasing here
  //

  cout<<"aaImage: "<<w<<" "<<h<<" "<<samples<<" "<<buffer.size()<<"\n";

  // Nothing to do if we weren't oversampling
  if (samples<=1) {
    return 0;
  }

  int overSamplingFactor = samples*samples;
  int imageSize = buffer.size()/overSamplingFactor;

  if (imageSize != w * h * 3) {
    // Current buffer size is not as expected - maybe aaImage has been called previously?
    // CommandLineUI explicitly calls it, but graphical UI doesn't I think?
    cout<<"Double call to aaImage?\n";
    return 0;
  }

  std::vector<unsigned char> tempBuffer(imageSize);

  // bool bufferzeros = std::all_of(buffer.begin(), buffer.end(), [](int i) { return i==0; });

  for (int j = 0; j < h; j++)
  {
    for (int i = 0; i < w; i++) {
      unsigned char *pixel = tempBuffer.data() + (i + j * w) * 3;
      long int sum0 = 0, sum1 = 0, sum2 = 0; // TODO: Can we change this to a vector instead?
      for (int l = 0; l < samples; l++) {
        for (int k = 0; k < samples; k++) {
          unsigned char *bufferLoc = buffer.data() + (j*w*samples*samples + l*w*samples + i*samples + k) * 3;
          // cout<<(j*w*samples*samples + l*w*samples + i*samples + k)<<"\n";
          // cout<<((i*samples+k) + (j*samples+l) * w)<<"\n";
          // cout<<((i+j*w)*samples*samples + (k+l*samples))<<"\n"; // TODO: Check which is the correct expression - basically denotes how many pixels before this
          sum0 += (int)bufferLoc[0];
          sum1 += (int)bufferLoc[1];
          sum2 += (int)bufferLoc[2];
        }
      }
      pixel[0] = (int)(sum0/overSamplingFactor);
      pixel[1] = (int)(sum1/overSamplingFactor);
      pixel[2] = (int)(sum2/overSamplingFactor);
    }
  }

  // bool tempBufferZeros = std::all_of(tempBuffer.begin(), tempBuffer.end(), [](int i) { return i==0; });

  // cout<<"Buffer: "<<bufferzeros<<" TempBuffer: "<<tempBufferZeros<<"\n";

  size_t newBufferSize = imageSize;;
  if (newBufferSize != buffer.size()) {
    bufferSize = newBufferSize;
    buffer.resize(bufferSize);
  }
  buffer_width = w;
  buffer_height = h;
  buffer.assign(tempBuffer.begin(), tempBuffer.end());  

  // cout<<buffer.size()<<"\n";

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