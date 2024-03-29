#include <cstdlib>
#include <ctime>
#include <string>

#include "flatlighting.h"
#include "objects.h"
#include "rasteriser.h"
#include "rastlighting.h"
#include "raytracer.h"
#include "standardlighting.h"
#include "starscreen.h"
#include "terrain_gen.h"

#ifdef useCL
#include "raytracer_cl.h"
#endif

#ifndef unix
extern "C" {
FILE __iob_func[3] = {stdin, stdout, *stderr};
}
#endif

int main(int argc, char *argv[]) {
#ifndef unix
  srand(static_cast<unsigned>(time(0)));
#endif
  if (argc >= 2) {
    string mode(argv[1]);

    Object *object;

    if (argc >= 3 && string(argv[2]) == "teapot") {
      object = new Teapot();
    } else {
      object = new Box;
    }

    Ptr_Triangles geometry = object->allTriangles();

    BoundingVolume bvh = object->createBoundingVolume();

    Cube bounds(geometry);

    vec3 lightPosition = lerpV(bounds.a, bounds.b, vec3(0.8, 0.8, 0.1));

    vec3 lightColour(1.0f, 1.0f, 1.0f);

    float lightPower = 0.5f * pow(glm::distance(bounds.a, bounds.b), 2.0f);

    PointLight light(lightPosition, bounds, 5.0f, lightColour, lightPower);
    SphereLight softLight(lightPosition, bounds, 5.0f, lightColour, lightPower,
                          4.0f, 5);

    Scene scene_low_quality = {light, geometry, bvh, bounds};
    Scene scene = {softLight, geometry, bvh, bounds};

    float viewAngle = 30.0f;

#pragma omp parallel
    {
#pragma omp master
      {
        int threads = omp_get_num_threads();
        cout << "running on " << threads << " threads" << endl;
      }
    }

    SdlScreen *screen = nullptr;
    LightingEngine *engine = nullptr;

    if (mode == "stars") {
      screen = new StarScreen(500, 500, 1000, 0.5);
    } else if (mode == "ray") {
      engine = new StandardLighting(scene);
      screen = new RayTracer(500, 500, viewAngle, 4, *engine, scene);
    } else if (mode == "raysmall") {
      engine = new StandardLighting(scene);
      screen = new RayTracer(150, 150, viewAngle, 4, *engine, scene);
    } else if (mode == "rast") {
      engine = new RastLighting(scene_low_quality);
      screen = new Rasteriser(500, 500, viewAngle, *engine, scene_low_quality);
    } else if (mode == "gi") {
      int sampleCount = 1;
      engine = new GlobalIllumination(scene_low_quality, sampleCount);
      screen =
          new RayTracer(500, 500, viewAngle, 4, *engine, scene_low_quality);
    } else if (mode == "conv") {
      int sampleCount = 1;
	  scene.light.power /= 10;
      engine = new ConvergentGlobalIllumination(scene, sampleCount, 512, 512);
      screen = new RayTracer(512, 512, viewAngle, 4, *engine, scene, false, false);
    }
#ifdef useCL
    else if (mode == "cl") {
      light.power *= 10;
      engine = new StandardLighting(scene);
      screen =
          new RayTracerCL(1024, 1024, *engine, light, geometry, bvh, scene);
    }
#endif
    else {
      cout << "Unknown mode \"" << mode << "\"" << endl;

      return EXIT_FAILURE;
    }

    screen->run();
    screen->saveBMP("screenshot.bmp");

    delete screen;
    delete engine;
    delete object;

    return EXIT_SUCCESS;
  } else {
    cout << "Please enter a mode:" << endl;
    cout << "\tstars - stars" << endl;
    cout << "\tray - raytracer" << endl;
    cout << "\trast - rasterizer" << endl;
    cout << "\tgi - global illumination" << endl;
    cout << "\tconv - convergent global illumination" << endl;
#ifdef useCL
    cout << "\tcl - openCL raytracer" << endl;
#endif

    return EXIT_FAILURE;
  }
}
