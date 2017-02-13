#pragma once

#include <limits>
#include <omp.h>

#include "camera.h"

#include "convergent_gi.h"

#include "standardlighting.h"
#include "cube.h"
#include "lightingengine.h"

#include "pointlight.h"
#include "sdlscreen.h"
#include "testmodel.h"
#include "bvh.h"
using std::numeric_limits;

class RayTracer : public SdlScreen {
private:
  //bool ClosestIntersection(Ray &ray);
  const shared_ptr<const vector<Triangle>> triangles;
  Camera camera;

  const vec3 ambientLight = vec3(0.1f, 0.1f, 0.1f);
  const shared_ptr<PointLight> light;
  shared_ptr<LightingEngine> lighting;
  const shared_ptr<BoundingVolume> boundingVolume;


protected:
  void update(float dt) override;
  void draw(int width, int height) override;

public:
  RayTracer(int width, int height, shared_ptr<LightingEngine> lighting,
	  const shared_ptr<PointLight> light,
            const shared_ptr<const vector<Triangle>> triangles, const shared_ptr<BoundingVolume> boundingVolume,
            bool fullscreen = false);
};
