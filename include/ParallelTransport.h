#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cmath>

// Parallel Transport Frame: a smoothly varying coordinate frame along a curve.
// Unlike Frenet frames, PTF avoids discontinuities and unnecessary twist.
struct PTFrame {
    glm::vec3 position;
    glm::vec3 tangent;   // Forward direction along the curve
    glm::vec3 normal;    // "Up" of the frame (minimal rotation from previous)
    glm::vec3 binormal;  // "Right" of the frame

    // Build a 4x4 model matrix from this frame (column-major: right, up, forward, pos)
    glm::mat4 toMatrix() const {
        return glm::mat4(
            glm::vec4(binormal, 0.0f),
            glm::vec4(normal,   0.0f),
            glm::vec4(-tangent, 0.0f),  // Negate tangent so fish faces forward (OBJ convention: -Z forward)
            glm::vec4(position, 1.0f)
        );
    }
};

// Compute Parallel Transport Frames along a polyline.
// Algorithm: starting from an initial frame, each successive frame is obtained
// by rotating the previous frame's normal/binormal to align with the new tangent
// using the minimum rotation (reflection method by Andrew Hanson).
inline std::vector<PTFrame> computePTFrames(const std::vector<glm::vec3>& points) {
    std::vector<PTFrame> frames;
    if (points.size() < 2) return frames;

    frames.resize(points.size());

    // First tangent
    glm::vec3 t0 = glm::normalize(points[1] - points[0]);

    // Choose initial normal: pick a vector not parallel to t0
    glm::vec3 arbitrary = (std::abs(glm::dot(t0, glm::vec3(0, 1, 0))) < 0.99f)
                          ? glm::vec3(0, 1, 0)
                          : glm::vec3(1, 0, 0);
    glm::vec3 n0 = glm::normalize(glm::cross(t0, arbitrary));
    glm::vec3 b0 = glm::cross(t0, n0);

    frames[0].position = points[0];
    frames[0].tangent  = t0;
    frames[0].normal   = n0;
    frames[0].binormal = b0;

    // Propagate frames using parallel transport (double reflection method)
    for (size_t i = 1; i < points.size(); ++i) {
        glm::vec3 ti;
        if (i < points.size() - 1) {
            ti = glm::normalize(points[i + 1] - points[i]);
        } else {
            ti = glm::normalize(points[i] - points[i - 1]);
        }

        glm::vec3 prevT = frames[i - 1].tangent;
        glm::vec3 prevN = frames[i - 1].normal;
        glm::vec3 prevB = frames[i - 1].binormal;

        // Reflection 1: reflect previous frame across the bisecting plane
        glm::vec3 v1 = points[i] - points[i - 1];
        float c1 = glm::dot(v1, v1);
        if (c1 < 1e-10f) {
            // Degenerate segment: copy previous frame
            frames[i].position = points[i];
            frames[i].tangent  = prevT;
            frames[i].normal   = prevN;
            frames[i].binormal = prevB;
            continue;
        }

        glm::vec3 rL = prevN - (2.0f / c1) * glm::dot(v1, prevN) * v1;
        glm::vec3 tL = prevT - (2.0f / c1) * glm::dot(v1, prevT) * v1;

        // Reflection 2: reflect across the plane perpendicular to ti+tL
        glm::vec3 v2 = ti - tL;
        float c2 = glm::dot(v2, v2);

        glm::vec3 ni;
        if (c2 < 1e-10f) {
            ni = rL; // tangents nearly identical
        } else {
            ni = rL - (2.0f / c2) * glm::dot(v2, rL) * v2;
        }

        frames[i].position = points[i];
        frames[i].tangent  = ti;
        frames[i].normal   = glm::normalize(ni);
        frames[i].binormal = glm::normalize(glm::cross(ti, frames[i].normal));
    }

    return frames;
}

// Catmull-Rom spline interpolation: returns a point on the spline at parameter t in [0, 1]
// controlPoints must have at least 4 points. The spline passes through points [1..n-2].
inline glm::vec3 catmullRom(const std::vector<glm::vec3>& cp, float t) {
    if (cp.size() < 4) return cp.empty() ? glm::vec3(0) : cp[0];

    int numSegments = (int)cp.size() - 3;
    float scaledT = t * numSegments;
    int segment = glm::clamp((int)std::floor(scaledT), 0, numSegments - 1);
    float localT = scaledT - segment;

    const glm::vec3& p0 = cp[segment];
    const glm::vec3& p1 = cp[segment + 1];
    const glm::vec3& p2 = cp[segment + 2];
    const glm::vec3& p3 = cp[segment + 3];

    float t2 = localT * localT;
    float t3 = t2 * localT;

    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * localT +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// Generate evenly-spaced sample points along a Catmull-Rom spline
inline std::vector<glm::vec3> sampleSpline(const std::vector<glm::vec3>& controlPoints, int numSamples) {
    std::vector<glm::vec3> samples;
    samples.reserve(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / (float)(numSamples - 1);
        samples.push_back(catmullRom(controlPoints, t));
    }
    return samples;
}
