#pragma once
#include <limits>
#include <omp.h>
#include <fstream>
#include <CL/cl.hpp>
#include "camera.h"
#include "convergent_gi.h"
#include "sdlscreen.h"
#include "testmodel.h"
#include "bvh.h"
struct TriangleStruct {
	cl_float3 v0;
	cl_float3 v1;
	cl_float3 v2;
	cl_float3 color;
	cl_float3 normal;
};
#pragma pack(1)
typedef struct CameraStruct {
	cl_float viewOffset;
	cl_float3 position;
	//cl_float3 rotation[3];
} CameraStruct;
#pragma pack()
typedef struct RayStruct {
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 collisionLocation;
	cl_float length;
	cl_int collision;
} RayStruct;

inline cl_float3 vecToFloat(vec3 vec) {
	cl_float3 f;
	f.x = vec.x;
	f.y = vec.y;
	f.z = vec.z;
	return f;
}


class RayTracerCL : public SdlScreen {
private:
	//bool ClosestIntersection(Ray &ray);
	const shared_ptr<const vector<Triangle>> triangles;
	Camera camera;

	const vec3 ambientLight = vec3(0.1f, 0.1f, 0.1f);
	const shared_ptr<PointLight> light;
	shared_ptr<LightingEngine> lighting;
	const shared_ptr<BoundingVolume> boundingVolume;
	void create_global_memory(int width, int height);
	cl::Context context;
	cl::Device default_device;
	TriangleStruct* cl_triangles;
	cl::Buffer triangleBuffer;
	cl::Buffer lightBuffer;
	cl::Buffer imageBuffer;
	cl::Buffer cameraBuffer;
	cl::Buffer pointBuffer;
	cl::CommandQueue queue;
	cl::Kernel castRays;
	cl::Kernel shader;
	cl::Program program;
	cl::Program::Sources sources;
	std::vector<cl::Device> all_devices;
	cl::Platform default_platform;
	std::vector<cl::Platform> all_platforms;
	std::string sourceCode;
	cl_float3* image;
	CameraStruct cameraStruct;
	cl_float cameraArray[4] = { 0,0,0,0 };

protected:
	void update(float dt) override;
	void draw(int width, int height) override;
	void boilerPlate(int width, int height);
public:
	RayTracerCL(int width, int height, shared_ptr<LightingEngine> lighting,
		const shared_ptr<PointLight> light,
		const shared_ptr<const vector<Triangle>> triangles, const shared_ptr<BoundingVolume> boundingVolume,
		bool fullscreen = false);
};