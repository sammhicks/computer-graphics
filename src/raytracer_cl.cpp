#ifdef useCL
#include "raytracer_cl.h"

RayTracerCL::RayTracerCL(int width, int height, shared_ptr<LightingEngine> lighting,
	shared_ptr<PointLight> light,
	const shared_ptr<const vector<Triangle>> triangles, const shared_ptr<BoundingVolume> boundingVolume, vec3 cameraPos,
	bool fullscreen)
	: SdlScreen(width, height, fullscreen), triangles(triangles),
	camera(cameraPos, 0.0f, 30.0f), light(light),
	lighting(lighting), boundingVolume(boundingVolume), averageImage(vector<vec3>(width*height)) { 

	gpu = getGPU(triangles->size());
	create_global_memory(width, height);
	int err = 0;
	gpu.queue.enqueueWriteBuffer(triangleBuffer, CL_TRUE, 0, sizeof(cl_float3) * triangles->size()*5, cl_triangles);

	cl_float3 lightLoc = { 300.0f, 400.0f, 100.0f };
	gpu.queue.enqueueWriteBuffer(lightBuffer, CL_TRUE, 0, sizeof(cl_float3), &lightLoc);
	castRays = cl::Kernel(gpu.program, "castRays", &err);
	if (err != 0) {
		cout << "error creating kernel: " << err << "\n";
		exit(1); 
	}
	castRays.setArg(0, triangleBuffer);
	castRays.setArg(2, pointBuffer);
	castRays.setArg(4, static_cast<int>(triangles->size()));
	castRays.setArg(5, (cl_int)width);
	castRays.setArg(6, (cl_int)height);
	shader = cl::Kernel(gpu.program, "pathTrace", &err);
	if (err != 0) {
		cout << "error creating kernel: " << err << "\n";
		exit(1);
	}
	shader.setArg(0, triangleBuffer);
	shader.setArg(2, pointBuffer);
	shader.setArg(3, imageBuffer);
	shader.setArg(4, static_cast<int>(triangles->size())); 
	shader.setArg(5, (cl_int)width);
	shader.setArg(6, (cl_int)height);
	shader.setArg(7, randBuffer); 


#pragma omp parallel for 
	for (int y = 0; y < height; ++y) {
#ifdef unix
#pragma omp simd
#endif
		for (int x = 0; x < width; ++x) {
			float rnd = RAND();
			rands[(y*height + x)] = static_cast<cl_uint>(RAND()*numeric_limits<cl_uint>::max());
			averageImage[(y*height + x)] = vec3(0, 0, 0);
		}
	}
	err = gpu.queue.enqueueWriteBuffer(randBuffer, CL_TRUE, 0, sizeof(cl_uint) * width*height, rands);
	refresh = true;
}

void RayTracerCL::create_global_memory(int width, int height) {
	cl_triangles = (cl_float3*)malloc(triangles->size() * sizeof(cl_float3)*5);
	vector<Triangle> tris = *triangles;
	for (int i = 0; i < triangles->size(); i++) {
		cl_float3 v0 = { tris[i].v0.x, tris[i].v0.y, tris[i].v0.z };
		cl_float3 v1 = { tris[i].v1.x, tris[i].v1.y, tris[i].v1.z };
		cl_float3 v2 = { tris[i].v2.x, tris[i].v2.y, tris[i].v2.z };
		cl_float3 c = { tris[i].colour.x, tris[i].colour.y, tris[i].colour.z };
		cl_float3 normal = { tris[i].normal.x, tris[i].normal.y, tris[i].normal.z };
		cl_triangles[i] = v0;
		cl_triangles[triangles->size() + i] = v1;
		cl_triangles[triangles->size()*2 + i] = v2;
		cl_triangles[triangles->size()*3 + i] = c;
		cl_triangles[triangles->size()*4 + i] = normal;
	}
	image = (cl_float3*)malloc(width*height * sizeof(cl_float3));
	rands = (cl_uint*)malloc(width*height * sizeof(cl_float));
	triangleBuffer = cl::Buffer(gpu.context, CL_MEM_READ_ONLY, sizeof(cl_float3) * triangles->size()*5);
	imageBuffer = cl::Buffer(gpu.context, CL_MEM_READ_WRITE, sizeof(cl_float3)* width*height);
	pointBuffer = cl::Buffer(gpu.context, CL_MEM_READ_WRITE, sizeof(RayStruct)* width*height);
	cameraBuffer = cl::Buffer(gpu.context, CL_MEM_READ_ONLY, sizeof(cl_float)*(4+9));
	randBuffer = cl::Buffer(gpu.context, CL_MEM_READ_ONLY, sizeof(cl_uint)* width*height);
}

void RayTracerCL::update(float dt) {
	if (light->update(dt)) {
		refresh = true;
	}
	if (camera.update(dt)) {
		refresh = true;
	}
	
}
 
void RayTracerCL::draw(int width, int height) {
	cl_float3 lightLoc = vecToFloat(light->position);

	if (refresh) {
		cl_float viewOffset = (cl_float)camera.viewOffset;
		cameraArray[0] = camera.position.x;
		cameraArray[1] = camera.position.y;
		cameraArray[2] = camera.position.z;
		cameraArray[3] = viewOffset;

		for (int i = 0; i < 3; i++) {
			cameraArray[4 + i * 3] = camera.rotation[i].x;
			cameraArray[4 + i * 3 + 1] = camera.rotation[i].y;
			cameraArray[4 + i * 3 + 2] = camera.rotation[i].z;
		}

		cl_float3 rotation[3];
		rotation[0] = vecToFloat(camera.rotation[0]);
		rotation[1] = vecToFloat(camera.rotation[1]);
		rotation[2] = vecToFloat(camera.rotation[2]);
		int err = gpu.queue.enqueueWriteBuffer(cameraBuffer, CL_TRUE, 0, sizeof(cl_float) * 13, cameraArray);

		if (err != 0) {
			cout << "err: " << err << "\n";
			exit(1);
		}



		castRays.setArg(1, lightLoc);
		castRays.setArg(3, cameraBuffer);
		gpu.queue.enqueueNDRangeKernel(castRays, cl::NullRange, cl::NDRange((size_t)width, (size_t)height), cl::NullRange);
	}
	shader.setArg(1, lightLoc);

	int err = gpu.queue.enqueueNDRangeKernel(shader, cl::NullRange, cl::NDRange((size_t)width, (size_t)height), cl::NullRange);
	if (err != 0) {
		cout << "err: " << err << "\n";
		exit(1);
	}
	gpu.queue.enqueueReadBuffer(imageBuffer, CL_TRUE, 0, sizeof(cl_float3) * width*height, image );

	if (refresh) {
		frameCounter = 1;
	}
#pragma omp parallel for
	for (int y = 0; y < height; ++y) {
#ifdef unix
#pragma omp simd
#endif
		for (int x = 0; x < width; ++x) {
			cl_float3 pixel = image[(y*height + x)];

			vec3 lightColour(pixel.x, pixel.y, pixel.z);
			if (refresh) {
				averageImage[(y*height + x)] = lightColour;
			}
			else {
				averageImage[(y*height + x)] += lightColour;
			}
			lightColour = averageImage[(y*height + x)] / (float)frameCounter;
				
			drawPixel(x, y, vec3(std::min(lightColour.r, 1.0f),
					std::min(lightColour.g, 1.0f),
					std::min(lightColour.b, 1.0f)));
			}
		}
	if (refresh) {
		refresh = false;
	}
	frameCounter++;
}
#endif 