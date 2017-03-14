//#define M_PI 3.14159265359f
#define sampleCount 2
#define V0(i) triangles[i]
#define V1(i) triangles[TRIANGLE_COUNT + i]
#define V2(i) triangles[(TRIANGLE_COUNT*2) + i]
#define COLOR(i) triangles[(TRIANGLE_COUNT*3) + i]
#define NORM(i) triangles[(TRIANGLE_COUNT*4) + i]

typedef struct TriangleStruct {
	float3 v0;
	float3 v1;
	float3 v2;
	float3 color;
	float3 normal;
} TriangleStruct;

typedef struct Ray {
	float3 origin;
	float3 direction;
	float3 collisionLocation;
	float length;
	int collision;
} Ray;

typedef struct Basis{
	float3 x;
	float3 y;
	float3 z;
 } Basis;

inline float lerpF(float a, float b, float t) { return a + (b - a) * t; }

inline float dot_product(float3 a, float3 b) {
	return a.x*b.x + a.y*b.y + a.y*b.y;
}

inline float det(float3 mat[3]) {
	return mat[0].x*(mat[1].y*mat[2].z - mat[1].z*mat[2].y) - mat[0].y*(mat[1].x*mat[2].z - mat[1].z*mat[2].x) + mat[0].z*(mat[1].x*mat[2].y - mat[1].y*mat[2].x);
}
inline float3 norm(float3 vec) {
	float len = native_rsqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);
	return (float3) { vec.x*len, vec.y*len, vec.z*len };
}
inline float len(float3 vec) {
	return native_sqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);
}
inline float3 phong(float3 v, float3 l, float3 n) {
	float3 r = l - 2.f* dot((dot(n, l)), n);
	float3 spec = dot(v, r);
	float power = len(spec);
	return (float3) { power, power, power };
}

inline float3 directLight(Ray ray, float3 lightPos, float3 normal) {
	float3 offset = lightPos - ray.collisionLocation;
	float3 direction = norm(offset);
	float radius = len(offset);
	float3 color = { 0.75f, 0.75f, 0.75f };
	return (max(dot(direction, normal), 0.0f) * 5000000.f / (4.0f * (float)M_PI * radius * radius)) * color;
}
inline float randomNumberGenerator(float seed){
	//return seed;
	return fmod(seed*10,1);
}
uint xorshift32(uint seed)
{
	uint x = seed;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return x;
}


inline Ray castRayLocal(float3 origin, float3 direction, const global float3* triangles, int TRIANGLE_COUNT){
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	ray.length = FLT_MAX;
	ray.collision = -1;
	ray.collisionLocation = (float3){0,0,0};
    //barrier(CLK_LOCAL_MEM_FENCE);
	barrier(CLK_LOCAL_MEM_FENCE);	
	#pragma unroll
	for (int i = 0; i < TRIANGLE_COUNT; i++) {
		bool intersected =  (dot(ray.direction, NORM(i)) < 0) ? true : false;
		float3 v0 = V0(i);
		float3 b = origin - v0; 
		float3 e1 = V1(i) - v0; 
		float3 e2 = V2(i) - v0; 
 
		float3 A[3] = { -direction, e1, e2 }; 
		float det_A = native_recip(det(A)); 
		float3 B[3] = { b, e1, e2 }; 
		float t = det(B) * det_A; 
		intersected = (t >= 0 && t < ray.length) ? intersected : false;
		float3 U[3] = { -ray.direction, b, e2 }; 
        float3 V[3] = { -ray.direction, e1, b }; 
        float u = det(U) * det_A; 
        float v = det(V) * det_A; 
		intersected = (u >= 0 && v >= 0 && (u + v) <= 1) ? intersected : false;
		ray.length = intersected ? t : ray.length; 
		ray.collision = intersected ? i : ray.collision; 
		ray.collisionLocation = intersected ? v0 + u * e1 + v * e2 : ray.collisionLocation; 

	}
		return ray;
}

 inline float3 randomDirection(float r1, float r2, float3 normal){
	float3 normalX;
	float3 normalY;
	bool mask = (fabs(normal.x) > fabs(normal.y)) ? true : false;
	normalX = (mask) ? ((float3){normal.z, 0, -normal.x} / native_sqrt(normal.x*normal.x + normal.z*normal.z)) : ((float3){0, -normal.z, normal.y} / native_sqrt(normal.z*normal.z + normal.y*normal.y));
	normalY = cross(normalX, normal);
	float sinTheta = native_sqrt(1 - r1*r1);
	float phi = 2 * M_PI * r2;
	float x = sinTheta * native_cos(phi);
	float z = sinTheta * native_sin(phi);
	float3 sample = (float3){x, r1, z};

	float3 direction = (float3){sample.x * normalX.x + sample.y * normal.x + sample.z * normalY.x,
		sample.x * normalX.y + sample.y * normal.y + sample.z * normalY.y,
		sample.x * normalX.z + sample.y * normal.z + sample.z * normalY.z};
	return direction;
  }

  //Casts a given number of rays
  kernel void fastRayCast(global const float3 * triangles, global Ray* points, int TRIANGLE_COUNT, int width, int height){
  	int x = get_global_id(0);
	int y = get_global_id(1);
	int index = (y*width + x);
	float3 origin = points[index].origin;
	float3 direction = points[index].direction;
	points[index] = castRayLocal(origin, direction, triangles, TRIANGLE_COUNT);
   }

kernel void pathTrace(global const float3* triangles, float3 lightLoc, global Ray* points, global float3* image, int TRIANGLE_COUNT, int width, int height, global uint* rands) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	bool debug = false;
	Ray cameraRay = points[(y*width + x)];
	float3 originalColor = COLOR(cameraRay.collision);
	float3 color = (float3) ( 0, 0, 0 );
	//DIRECT LIGHT

	Ray lightRay = castRayLocal(lightLoc, cameraRay.collisionLocation - lightLoc, triangles, TRIANGLE_COUNT);
	

	float3 n = NORM(cameraRay.collision);
	//lightColour = directLight(lightRay, lightLoc, n)*diffuse + spec * specularity;
	color = (lightRay.collision == cameraRay.collision) ? directLight(lightRay, lightLoc, n) : (float3){0,0,0};	

	//generates and calculates all the ray intersections needed
	Ray layer1[sampleCount];
	uint seed = rands[(y*width + x)];
	float r1 = (float)seed / UINT_MAX;
	#pragma unroll
	for(int i=0; i < sampleCount; i++){
		seed = xorshift32(seed);
		float r2 = (float)seed / UINT_MAX;
		float3 direction = norm(randomDirection(r1, r2, n));
		layer1[i] = castRayLocal(cameraRay.collisionLocation, direction, triangles, TRIANGLE_COUNT);
		layer1[i].length = r1;

		seed = xorshift32(seed);
		r1 = (float)seed / UINT_MAX;

	}
	Ray layer2[(sampleCount*sampleCount)];
	#pragma unroll
	for(int i=0; i < sampleCount; i++){
		#pragma unroll
		for(int j=0; j < sampleCount; j++){
			seed = xorshift32(seed);
			float r2 = (float)seed / UINT_MAX;
			float3 direction = norm(randomDirection(r1, r2, NORM(layer1[i].collision)));
			layer2[i*sampleCount+j] = castRayLocal(layer1[i].collisionLocation, direction, triangles, TRIANGLE_COUNT);
			layer2[i*sampleCount+j].length = r1;
			seed = xorshift32(seed);
			r1 = (float)seed / UINT_MAX;

		}
	}
	#pragma unroll
	for(int i=0; i < (sampleCount*sampleCount); i++){
		Ray lightRay = castRayLocal(lightLoc, layer2[i].collisionLocation - lightLoc, triangles, TRIANGLE_COUNT);
		float3 directLightHere;
        float3 n = NORM(layer2[i].collision);
        directLightHere = directLight(layer2[i], lightLoc, n);
        directLightHere = (lightRay.collision == layer2[i].collision) ? directLightHere : (float3){0,0,0};
		directLightHere = (layer2[i].collision > -1) ? directLightHere*(COLOR(layer2[i].collision)) : (float3){0.2,0.2,0.2 };
		layer2[i].origin = (directLightHere/(float)M_PI);
	}
	#pragma unroll
	for(int i=0; i < sampleCount; i++){
		Ray lightRay = castRayLocal(lightLoc, layer1[i].collisionLocation - lightLoc, triangles, TRIANGLE_COUNT);
		float3 directLightHere;
        float3 n = NORM(layer1[i].collision);
        directLightHere = directLight(layer1[i], lightLoc, n);
        directLightHere = (lightRay.collision == layer1[i].collision) ? directLightHere : (float3){0,0,0};
		directLightHere = (layer1[i].collision > -1) ? directLightHere*(COLOR(layer1[i].collision)) : (float3){0.2,0.2,0.2 };
		float3 indirectLight = (float3){0,0,0};
        #pragma unroll
		for(int j=0; j < sampleCount; j++){
			float3 sampleLight = (layer2[i*sampleCount+j].length)*layer2[i*sampleCount+j].origin;
			indirectLight += sampleLight;
		}
		indirectLight /= (float)sampleCount;
		layer1[i].origin = (directLightHere / (float)M_PI + 2.f * indirectLight);
	}

	float3 indirectLight = (float3){0,0,0};
	#pragma unroll
	for(int j=0; j < sampleCount; j++){
		float3 sampleLight = (layer1[j].length)*layer1[j].origin;
		indirectLight += sampleLight;
	}
	indirectLight /= (float)sampleCount;
	float3 finalLight = (color / (float)M_PI + 2.f * indirectLight)*0.75f;
	finalLight *= originalColor;
	finalLight = debug ? (float3){0,1,0 } : finalLight;
	finalLight = (cameraRay.collision > -1) ? finalLight : (float3){0,0,0};
	image[(y*width + x)] = (float3) { min(finalLight.x, 1.0f), min(finalLight.y, 1.0f), min(finalLight.z, 1.0f) };
	rands[(y*width + x)] = seed;
}


kernel void flatShade(global const float3* triangles, float3 lightLoc, global Ray* points, global float3* image, int TRIANGLE_COUNT, int width, int height, global uint* rands) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	Ray ray = points[(y*width + x)];
	float3 color = (float3) { 0, 0, 0 };
	if (ray.collision > -1 && ray.collision < TRIANGLE_COUNT) {
		color = COLOR(ray.collision);
	}
	image[(y*width + x)] = color;
}

kernel void standardShade(global const float3* triangles, float3 lightLoc, global Ray* points, global float3* image, int TRIANGLE_COUNT, int width, int height, global uint* rands) {
	int x = get_global_id(0);
	int y = get_global_id(1);

	Ray cameraRay = points[(y*width + x)];
	//TriangleStruct triangle = triangles[cameraRay.collision];
	Ray lightRay = castRayLocal(lightLoc, cameraRay.collisionLocation - lightLoc, triangles, TRIANGLE_COUNT);
	int diff = cameraRay.collision - lightRay.collision;
	float3 ambient = 0.4f;

	float3 n = NORM(cameraRay.collision);
	float3 v = norm(cameraRay.direction);
	float3 l = norm(lightRay.direction);
	float3 spec = phong(v, l, n);
	float diffuse = 0.75f;
	float specularity = 0.1f;
	float3 direct = directLight(lightRay, lightLoc, n)*diffuse + spec * specularity;

	float3 color = (diff == 0)  ? direct : ambient;
	color *= COLOR(cameraRay.collision);
	image[(y*width + x)] = (float3) { min(color.x, 1.0f), min(color.y, 1.0f), min(color.z, 1.0f) };
}

kernel void castRays(global const float3* triangles, float3 lightLoc, global Ray* points, global float* camera, int TRIANGLE_COUNT, int width, int height) {

	int x = get_global_id(0);
	int y = get_global_id(1);

	float3 cameraPos = (float3){(float)camera[0],(float)camera[1],(float)camera[2] };
	Ray cameraRay;
	cameraRay.origin = cameraPos;

	float3 cameraSpaceDirection = (float3) { lerpF(camera[3], -camera[3], (float)x / (float)width), lerpF(camera[3], -camera[3], (float)y / (float)height), 1.0f };
	float3 rotation[3];
	rotation[0] = (float3) { camera[4], camera[5], camera[6] };
	rotation[1] = (float3) { camera[7], camera[8], camera[9] };
	rotation[2] = (float3) { camera[10], camera[11], camera[12]  };
	//dot product for camera ray
	cameraRay.direction.x = dot(rotation[0], cameraSpaceDirection);
	cameraRay.direction.y = dot(rotation[1], cameraSpaceDirection);
	cameraRay.direction.z = dot(rotation[2], cameraSpaceDirection);
	points[(y*width + x)] = castRayLocal(cameraRay.origin, cameraRay.direction, triangles, TRIANGLE_COUNT);
	return;
}

