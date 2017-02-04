#include "triangle.h"

Triangle::Triangle(vec3 v0, vec3 v1, vec3 v2, vec3 color, vector<unsigned char>& texture):
	v0(v0), e1(v1 - v0), e2(v2 - v0),
	normal(glm::normalize(glm::cross(v2 - v0, v1 - v0))),
	color(color),
	texture(texture){

}

bool Triangle::calculateIntection(Ray &ray) const
{
	if (glm::dot(normal, ray.direction) < 0) {

		vec3 b = ray.position - v0;

		glm::mat3 A(-ray.direction, e1, e2);
		float det_A = glm::determinant(A);

		float t = glm::determinant(glm::mat3(b, e1, e2)) / det_A;
		if(t >= 0 && t < ray.length){
			float u = glm::determinant(glm::mat3(-ray.direction, b, e2)) / det_A;
			float v = glm::determinant(glm::mat3(-ray.direction, e1, b)) / det_A;

			if(u >= 0 && v >= 0 && (u + v) < 1){
				ray.length = t;
				ray.collision = this;
				ray.collisionLocation = v0 + u * e1 + v * e2;
				ray.collisionColor = getColor(u, v);
				return true;
			}
		}
	}

	return false;
}

vec3 Triangle::getColor(float u, float v) const{
	int x = static_cast<int>(u*128.f);
	int y = static_cast<int>(v*128.f);
	int offset = (128*y+x)*4;
	int r  = static_cast<int>(texture[offset]);
	int g  = static_cast<int>(texture[offset+1]);
	int b  = static_cast<int>(texture[offset+2]);
	return vec3((float)r/255, (float)g/255, (float)b/255);

}
