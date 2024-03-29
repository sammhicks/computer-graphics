#include "triangle.h"

vec3 Triangle::calculateNormal(vec3 v0, vec3 v1, vec3 v2) {
  return glm::normalize(glm::cross(v2 - v0, v1 - v0));
}

Triangle::Triangle(vec3 v0, vec3 v1, vec3 v2, vec2 vt0, vec2 vt1, vec2 vt2,
                   const Material *const mat)
    : v0(v0), v1(v1), v2(v2), e1(v1 - v0), e2(v2 - v0), vt0(vt0), vt1(vt1),
      vt2(vt2), et1(vt1 - vt0), et2(vt2 - vt0),
      vn0(calculateNormal(v0, v1, v2)), vn1(calculateNormal(v0, v1, v2)),
      vn2(calculateNormal(v0, v1, v2)), en1(vn1 - vn0), en2(vn2 - vn0),
      normal(calculateNormal(v0, v1, v2)), mat(mat) {}

Triangle::Triangle(vec3 v0, vec3 v1, vec3 v2, vec2 vt0, vec2 vt1, vec2 vt2,
                   vec3 vn0, vec3 vn1, vec3 vn2, const Material *const mat)
    : v0(v0), v1(v1), v2(v2), e1(v1 - v0), e2(v2 - v0), vt0(vt0), vt1(vt1),
      vt2(vt2), et1(vt1 - vt0), et2(vt2 - vt0), vn0(vn0), vn1(vn1), vn2(vn2),
      en1(vn1 - vn0), en2(vn2 - vn0), normal(calculateNormal(v0, v1, v2)),
      mat(mat) {}

bool Triangle::calculateIntersection(Ray &ray) const {
  if (glm::dot(normal, ray.getDirection()) < 0) {
    vec3 b = ray.getPosition() - v0;

    glm::mat3 A(-ray.getDirection(), e1, e2);
    auto det_A = glm::determinant(A);

    auto t = glm::determinant(glm::mat3(b, e1, e2)) / det_A;
    if (t >= 0 && t < ray.getLength()) {
      auto u = glm::determinant(glm::mat3(-ray.getDirection(), b, e2)) / det_A;
      auto v = glm::determinant(glm::mat3(-ray.getDirection(), e1, b)) / det_A;

      if (u >= 0.0f && v >= 0.0f && (u + v) < 1.0f) {
        ray.updateCollision(this, t, vec2(u, v));
        return true;
      }
    }
  }

  return false;
}

// Position

vector<vec3> Triangle::getVertices() const { return {v0, v1, v2}; }

vec3 Triangle::getPosition(vec2 uv) const { return v0 + uv.x * e1 + uv.y * e2; }

vec3 Triangle::getPosition(vec3 bary) const {
  return bary.x * v0 + bary.y * v1 + bary.z * v2;
}

// Texture coordinates

vec2 Triangle::getTexUV(vec2 uv) const { return vt0 + uv.x * et1 + uv.y * et2; }

vec2 Triangle::getTexUV(vec3 bary) const {
  return bary.x * vt0 + bary.y * vt1 + bary.z * vt2;
}

// Normals

vec3 Triangle::getNormal(vec2 uv) const {
  return calculateSurfaceNormal(getTexUV(uv),
                                normalize(vn0 + uv.x * en1 + uv.y * en2));
}

vec3 Triangle::getNormal(vec3 bary) const {
  return calculateSurfaceNormal(
      getTexUV(bary), normalize(bary.x * vn0 + bary.y * vn1 + bary.z * vn2));
}

vec3 Triangle::calculateSurfaceNormal(vec2 texUV, vec3 normal) const {
  if (abs(et1.x * et2.y - et2.x * et1.y) < 0.01f) {
    return normal;
  } else {

    float f = 1.0f / (et1.x * et2.y - et2.x * et1.y);

    vec3 tangent = normalize(f * vec3(et2.y * e1.x - et1.y * e2.x,
                                      et2.y * e1.y - et1.y * e2.y,
                                      et2.y * e1.z - et1.y * e2.z));

    vec3 bitangent = normalize(f * vec3(et2.x * e1.x - et1.x * e2.x,
                                        et2.x * e1.y - et1.x * e2.y,
                                        et2.x * e1.z - et1.x * e2.z));

    vec3 tangentSpaceNormal = mat->normal(texUV);

    return normalize(tangentSpaceNormal.x * tangent +
                     tangentSpaceNormal.y * bitangent +
                     tangentSpaceNormal.z * normal);
  }
}

// Ambient Colour

vec3 Triangle::ambientColourNorm(vec2 uv) const { return mat->ambient(uv); }

vec3 Triangle::ambientColour(vec2 uv) const {
  return ambientColourNorm(getTexUV(uv));
}

vec3 Triangle::ambientColour(vec3 bary) const {
  return ambientColourNorm(getTexUV(bary));
}

// Diffuse Colour

vec3 Triangle::diffuseColourNorm(vec2 uv, vec3 lightIncidentDirection,
                                 vec3 surfaceNormal) const {
  return std::max(dot(-lightIncidentDirection, surfaceNormal), 0.0f) *
         mat->diffuse(uv);
}

vec3 Triangle::diffuseColour(vec2 uv, vec3 lightIncidentDirection) const {
  return diffuseColourNorm(getTexUV(uv), lightIncidentDirection, getNormal(uv));
}

vec3 Triangle::diffuseColour(vec3 bary, vec3 lightIncidentDirection) const {
  return diffuseColourNorm(getTexUV(bary), lightIncidentDirection,
                           getNormal(bary));
}

// Specular Colour

vec3 Triangle::specularColourNorm(vec3 specular, float specularExponent,
                                  vec3 lightIncidentDirection,
                                  vec3 surfaceNormal,
                                  vec3 cameraIncidentDirection) const {
  auto reflection = normalize(reflect(lightIncidentDirection, surfaceNormal));
  auto specularCoefficient =
      glm::pow(std::max(dot(cameraIncidentDirection, -reflection), 0.0f),
               specularExponent);

  return specularCoefficient * specular;
}

vec3 Triangle::specularColour(vec3 lightIncidentDirection,
                              vec3 cameraIncidentDirection) const {
  return specularColourNorm(mat->specular(), mat->specularExponent(),
                            lightIncidentDirection, normal,
                            cameraIncidentDirection);
}

vec3 Triangle::specularColour(vec2 uv, vec3 lightIncidentDirection,
                              vec3 cameraIncidentDirection) const {
  return specularColourNorm(
      mat->specular(getTexUV(uv)), mat->specularExponent(getTexUV(uv)),
      lightIncidentDirection, getNormal(uv), cameraIncidentDirection);
}

vec3 Triangle::specularColour(vec3 bary, vec3 lightIncidentDirection,
                              vec3 cameraIncidentDirection) const {
  return specularColourNorm(
      mat->specular(getTexUV(bary)), mat->specularExponent(getTexUV(bary)),
      lightIncidentDirection, getNormal(bary), cameraIncidentDirection);
}

bool Triangle::isMirrored() const { return mat->isMirrored; }
