#include "Realtime3DPoseComponent.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
constexpr const char* poseNames[]
{
    "IDLE / FRONT",
    "WALK CYCLE",
    "WALK START / STOP",
    "TURN 0-180",
    "BACK VIEW",
    "SIT",
    "CROUCH",
    "KNEES UP",
    "LIE DOWN"
};

constexpr int poseCount = 9;
constexpr float pi = 3.14159265358979323846f;

struct V3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

V3 operator+ (V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
V3 operator- (V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
V3 operator* (V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }

float dot (V3 a, V3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

V3 cross (V3 a, V3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float length (V3 a)
{
    return std::sqrt (dot (a, a));
}

V3 normalise (V3 a)
{
    const auto n = length (a);
    if (n <= 0.00001f)
        return { 0.0f, 1.0f, 0.0f };

    return a * (1.0f / n);
}

juce::Matrix3D<float> scaleMatrix (float x, float y, float z)
{
    return {
        x, 0.0f, 0.0f, 0.0f,
        0.0f, y, 0.0f, 0.0f,
        0.0f, 0.0f, z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

juce::Matrix3D<float> modelTRS (V3 position,
                                V3 scale,
                                V3 rotation = {})
{
    const auto t = juce::Matrix3D<float>::fromTranslation (
        { position.x, position.y, position.z });
    const auto r = juce::Matrix3D<float>::rotation (
        { rotation.x, rotation.y, rotation.z });
    const auto s = scaleMatrix (scale.x, scale.y, scale.z);
    return t * r * s;
}

juce::Matrix3D<float> cylinderModel (V3 a, V3 b, float radius)
{
    const auto delta = b - a;
    const auto h = juce::jmax (0.001f, length (delta));
    const auto up = normalise (delta);

    const auto helper = std::abs (up.y) < 0.92f
                      ? V3 { 0.0f, 1.0f, 0.0f }
                      : V3 { 1.0f, 0.0f, 0.0f };

    auto right = normalise (cross (helper, up));
    auto forward = normalise (cross (up, right));
    const auto centre = (a + b) * 0.5f;

    const float sx = radius * 2.0f;
    const float sz = radius * 2.0f;

    const float m[16]
    {
        right.x * sx,   right.y * sx,   right.z * sx,   0.0f,
        up.x * h,       up.y * h,       up.z * h,       0.0f,
        forward.x * sz, forward.y * sz, forward.z * sz, 0.0f,
        centre.x,       centre.y,       centre.z,       1.0f
    };

    return juce::Matrix3D<float> (m);
}

float smooth01 (float x)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    return x * x * (3.0f - 2.0f * x);
}

struct Rig
{
    V3 pelvis;
    V3 chest;
    V3 neck;
    V3 head;

    V3 hipL;
    V3 hipR;
    V3 kneeL;
    V3 kneeR;
    V3 ankleL;
    V3 ankleR;

    V3 shoulderL;
    V3 shoulderR;
    V3 elbowL;
    V3 elbowR;
    V3 handL;
    V3 handR;

    float yaw = 0.0f;
};

Rig makeRig (int pose, float phase)
{
    float bob = 0.0f;
    float walkAmount = 0.0f;
    float walkWave = std::sin (phase * 2.0f * pi);

    if (pose == 1)
    {
        walkAmount = 1.0f;
        bob = 0.018f * std::sin (phase * 4.0f * pi);
    }
    else if (pose == 2)
    {
        walkAmount = std::sin (phase * pi);
        bob = 0.012f * std::sin (phase * 4.0f * pi) * walkAmount;
    }

    Rig r;
    r.pelvis = { 0.0f, 1.02f + bob, 0.0f };
    r.chest  = { 0.0f, 1.43f + bob, 0.0f };
    r.neck   = { 0.0f, 1.65f + bob, -0.005f };
    r.head   = { 0.0f, 1.83f + bob, -0.020f };

    r.hipL   = r.pelvis + V3 { -0.115f, 0.0f, 0.0f };
    r.hipR   = r.pelvis + V3 {  0.115f, 0.0f, 0.0f };
    r.kneeL  = { -0.115f, 0.58f + bob, 0.0f };
    r.kneeR  = {  0.115f, 0.58f + bob, 0.0f };
    r.ankleL = { -0.115f, 0.13f, 0.0f };
    r.ankleR = {  0.115f, 0.13f, 0.0f };

    r.shoulderL = r.chest + V3 { -0.245f, 0.06f, 0.0f };
    r.shoulderR = r.chest + V3 {  0.245f, 0.06f, 0.0f };
    r.elbowL = { -0.275f, 1.18f + bob, 0.015f };
    r.elbowR = {  0.275f, 1.18f + bob, 0.015f };
    r.handL  = { -0.225f, 0.91f + bob, -0.01f };
    r.handR  = {  0.225f, 0.91f + bob, -0.01f };

    if (walkAmount > 0.001f)
    {
        const auto stepL = walkWave * walkAmount;
        const auto stepR = -stepL;

        r.kneeL.z  += 0.16f * stepL;
        r.ankleL.z += 0.31f * stepL;
        r.kneeR.z  += 0.16f * stepR;
        r.ankleR.z += 0.31f * stepR;

        r.ankleL.y += 0.055f * juce::jmax (0.0f, -stepL);
        r.ankleR.y += 0.055f * juce::jmax (0.0f, -stepR);

        r.elbowL.z -= 0.13f * stepL;
        r.handL.z  -= 0.23f * stepL;
        r.elbowR.z -= 0.13f * stepR;
        r.handR.z  -= 0.23f * stepR;
    }

    if (pose == 1 || pose == 2)
        r.yaw = -0.5f * pi;
    else if (pose == 3)
        r.yaw = pi * smooth01 (phase);
    else if (pose == 4)
        r.yaw = pi;
    else if (pose == 5)
    {
        r.yaw = -0.30f;
        r.pelvis = { 0.0f, 0.60f, 0.06f };
        r.chest  = { 0.0f, 1.00f, -0.02f };
        r.neck   = { 0.0f, 1.20f, -0.02f };
        r.head   = { 0.0f, 1.38f, -0.04f };

        r.hipL = r.pelvis + V3 { -0.12f, 0.0f, 0.0f };
        r.hipR = r.pelvis + V3 {  0.12f, 0.0f, 0.0f };
        r.kneeL  = { -0.13f, 0.50f, -0.38f };
        r.kneeR  = {  0.13f, 0.50f, -0.38f };
        r.ankleL = { -0.13f, 0.13f, -0.52f };
        r.ankleR = {  0.13f, 0.13f, -0.52f };

        r.shoulderL = r.chest + V3 { -0.23f, 0.05f, 0.0f };
        r.shoulderR = r.chest + V3 {  0.23f, 0.05f, 0.0f };
        r.elbowL = { -0.24f, 0.76f, -0.18f };
        r.elbowR = {  0.24f, 0.76f, -0.18f };
        r.handL  = { -0.16f, 0.58f, -0.34f };
        r.handR  = {  0.16f, 0.58f, -0.34f };
    }
    else if (pose == 6)
    {
        r.yaw = 0.18f;
        r.pelvis = { 0.0f, 0.66f, 0.03f };
        r.chest  = { 0.0f, 1.08f, -0.05f };
        r.neck   = { 0.0f, 1.27f, -0.07f };
        r.head   = { 0.0f, 1.44f, -0.09f };

        r.hipL = r.pelvis + V3 { -0.12f, 0.0f, 0.0f };
        r.hipR = r.pelvis + V3 {  0.12f, 0.0f, 0.0f };
        r.kneeL  = { -0.16f, 0.36f, -0.18f };
        r.kneeR  = {  0.16f, 0.36f, -0.18f };
        r.ankleL = { -0.18f, 0.12f, 0.08f };
        r.ankleR = {  0.18f, 0.12f, 0.08f };

        r.shoulderL = r.chest + V3 { -0.23f, 0.05f, 0.0f };
        r.shoulderR = r.chest + V3 {  0.23f, 0.05f, 0.0f };
        r.elbowL = { -0.29f, 0.80f, -0.10f };
        r.elbowR = {  0.29f, 0.80f, -0.10f };
        r.handL  = { -0.20f, 0.60f, -0.19f };
        r.handR  = {  0.20f, 0.60f, -0.19f };
    }
    else if (pose == 7)
    {
        r.yaw = -0.15f;
        r.pelvis = { 0.0f, 0.39f, 0.08f };
        r.chest  = { 0.0f, 0.80f, -0.03f };
        r.neck   = { 0.0f, 0.99f, -0.07f };
        r.head   = { 0.0f, 1.16f, -0.10f };

        r.hipL = r.pelvis + V3 { -0.12f, 0.0f, 0.0f };
        r.hipR = r.pelvis + V3 {  0.12f, 0.0f, 0.0f };
        r.kneeL  = { -0.16f, 0.69f, -0.31f };
        r.kneeR  = {  0.16f, 0.69f, -0.31f };
        r.ankleL = { -0.16f, 0.18f, -0.42f };
        r.ankleR = {  0.16f, 0.18f, -0.42f };

        r.shoulderL = r.chest + V3 { -0.23f, 0.05f, 0.0f };
        r.shoulderR = r.chest + V3 {  0.23f, 0.05f, 0.0f };
        r.elbowL = { -0.29f, 0.68f, -0.19f };
        r.elbowR = {  0.29f, 0.68f, -0.19f };
        r.handL  = { -0.18f, 0.62f, -0.34f };
        r.handR  = {  0.18f, 0.62f, -0.34f };
    }
    else if (pose == 8)
    {
        r.yaw = -0.35f;
        r.pelvis = { 0.08f, 0.24f, 0.02f };
        r.chest  = { -0.34f, 0.28f, 0.00f };
        r.neck   = { -0.62f, 0.29f, -0.01f };
        r.head   = { -0.82f, 0.31f, -0.02f };

        r.hipL = r.pelvis + V3 { -0.06f, 0.05f, -0.08f };
        r.hipR = r.pelvis + V3 {  0.06f, 0.03f,  0.08f };
        r.kneeL  = { 0.38f, 0.35f, -0.18f };
        r.kneeR  = { 0.34f, 0.31f,  0.16f };
        r.ankleL = { 0.70f, 0.12f, -0.07f };
        r.ankleR = { 0.66f, 0.11f,  0.10f };

        r.shoulderL = r.chest + V3 { -0.03f, 0.08f, -0.14f };
        r.shoulderR = r.chest + V3 { -0.03f, 0.06f,  0.14f };
        r.elbowL = { -0.60f, 0.22f, -0.16f };
        r.elbowR = { -0.57f, 0.20f,  0.18f };
        r.handL  = { -0.78f, 0.17f, -0.08f };
        r.handR  = { -0.74f, 0.17f,  0.08f };
    }

    return r;
}

const char* vertexShaderSource = R"GLSL(
#ifdef GL_ES
precision highp float;
precision highp int;
#endif

attribute vec3 position;
attribute vec3 normal;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

varying vec3 vNormal;
varying vec3 vWorldPos;
varying float vDepth;

void main()
{
    vec4 world = uModel * vec4 (position, 1.0);
    vec4 view = uView * world;

    vWorldPos = world.xyz;
    vNormal = normalize (mat3 (uModel) * normal);
    vDepth = max (0.0, -view.z);

    gl_Position = uProjection * view;
}
)GLSL";

const char* fragmentShaderSource = R"GLSL(
#ifdef GL_ES
precision highp float;
precision highp int;
#endif

varying vec3 vNormal;
varying vec3 vWorldPos;
varying float vDepth;

uniform vec4 uColour;
uniform vec2 uResolution;
uniform float uTime;
uniform float uMaterial;

float hash21 (vec2 p)
{
    p = fract (p * vec2 (123.34, 456.21));
    p += dot (p, p + 45.32);
    return fract (p.x * p.y);
}

void main()
{
    vec3 n = normalize (vNormal);
    vec3 lightDir = normalize (vec3 (-0.48, 0.78, -0.39));
    vec3 fillDir = normalize (vec3 (0.52, 0.28, -0.72));

    float key = max (dot (n, lightDir), 0.0);
    float fill = max (dot (n, fillDir), 0.0);
    float hemi = 0.46 + 0.18 * max (n.y, 0.0);

    vec3 viewDir = normalize (vec3 (0.0, 1.0, 4.8) - vWorldPos);
    vec3 halfDir = normalize (lightDir + viewDir);

    float specPower = mix (20.0, 54.0, step (2.5, uMaterial));
    float spec = pow (max (dot (n, halfDir), 0.0), specPower);
    spec *= mix (0.055, 0.15, step (2.5, uMaterial));

    float rim = pow (1.0 - max (dot (n, viewDir), 0.0), 3.0) * 0.10;

    vec3 base = uColour.rgb;
    vec3 col = base * (hemi + key * 0.66 + fill * 0.10);
    col += vec3 (spec + rim);

    if (uMaterial > 0.5 && uMaterial < 1.5)
    {
        float grit = hash21 (floor (vWorldPos.xz * 120.0));
        col *= 0.92 + grit * 0.13;
    }

    float fog = 1.0 - exp (-0.022 * vDepth * vDepth);
    vec3 fogColour = vec3 (0.57, 0.59, 0.60);
    col = mix (col, fogColour, fog);

    float luma = dot (col, vec3 (0.299, 0.587, 0.114));
    col = mix (col, vec3 (luma), 0.84);
    col *= vec3 (0.985, 1.0, 1.012);

    col = clamp ((col - 0.5) * 1.08 + 0.5, 0.0, 1.0);

    vec2 uv = gl_FragCoord.xy / max (uResolution, vec2 (1.0));
    vec2 q = uv * 2.0 - 1.0;
    float vignette = 1.0 - 0.18 * dot (q, q);
    col *= clamp (vignette, 0.72, 1.0);

    float grain = hash21 (gl_FragCoord.xy
        + vec2 (floor (uTime * 24.0) * 13.7, fract (uTime) * 77.3));
    col += (grain - 0.5) * 0.022;

    gl_FragColor = vec4 (clamp (col, 0.0, 1.0), uColour.a);
}
)GLSL";

}

struct Realtime3DPoseComponent::Mesh
{
    struct Vertex
    {
        float x, y, z;
        float nx, ny, nz;
    };

    std::vector<Vertex> vertices;
    std::vector<unsigned short> indices;
    unsigned int vbo = 0;
    unsigned int ibo = 0;

    void upload()
    {
        using namespace ::juce::gl;

        glGenBuffers (1, &vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        glBufferData (GL_ARRAY_BUFFER,
                      static_cast<GLsizeiptr> (vertices.size() * sizeof (Vertex)),
                      vertices.data(),
                      GL_STATIC_DRAW);

        glGenBuffers (1, &ibo);
        glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData (GL_ELEMENT_ARRAY_BUFFER,
                      static_cast<GLsizeiptr> (indices.size() * sizeof (unsigned short)),
                      indices.data(),
                      GL_STATIC_DRAW);

        glBindBuffer (GL_ARRAY_BUFFER, 0);
        glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void release()
    {
        using namespace ::juce::gl;

        if (vbo != 0)
            glDeleteBuffers (1, &vbo);
        if (ibo != 0)
            glDeleteBuffers (1, &ibo);

        vbo = 0;
        ibo = 0;
    }
};

static std::unique_ptr<Realtime3DPoseComponent::Mesh> makeSphereMesh()
{
    auto mesh = std::make_unique<Realtime3DPoseComponent::Mesh>();

    constexpr int rings = 16;
    constexpr int sectors = 24;

    for (int y = 0; y <= rings; ++y)
    {
        const float v = static_cast<float> (y) / static_cast<float> (rings);
        const float theta = v * pi;
        const float sy = std::cos (theta);
        const float sr = std::sin (theta);

        for (int x = 0; x <= sectors; ++x)
        {
            const float u = static_cast<float> (x) / static_cast<float> (sectors);
            const float phi = u * 2.0f * pi;
            const float sx = sr * std::cos (phi);
            const float sz = sr * std::sin (phi);

            mesh->vertices.push_back ({ 0.5f * sx, 0.5f * sy, 0.5f * sz,
                                        sx, sy, sz });
        }
    }

    for (int y = 0; y < rings; ++y)
    {
        for (int x = 0; x < sectors; ++x)
        {
            const auto a = static_cast<unsigned short> (y * (sectors + 1) + x);
            const auto b = static_cast<unsigned short> ((y + 1) * (sectors + 1) + x);
            const auto c = static_cast<unsigned short> (b + 1);
            const auto d = static_cast<unsigned short> (a + 1);

            mesh->indices.insert (mesh->indices.end(), { a, b, d, d, b, c });
        }
    }

    return mesh;
}

static std::unique_ptr<Realtime3DPoseComponent::Mesh> makeCylinderMesh()
{
    auto mesh = std::make_unique<Realtime3DPoseComponent::Mesh>();
    constexpr int sectors = 24;

    for (int i = 0; i <= sectors; ++i)
    {
        const auto u = static_cast<float> (i) / static_cast<float> (sectors);
        const auto a = u * 2.0f * pi;
        const auto x = 0.5f * std::cos (a);
        const auto z = 0.5f * std::sin (a);
        const auto nx = std::cos (a);
        const auto nz = std::sin (a);

        mesh->vertices.push_back ({ x, -0.5f, z, nx, 0.0f, nz });
        mesh->vertices.push_back ({ x,  0.5f, z, nx, 0.0f, nz });
    }

    for (int i = 0; i < sectors; ++i)
    {
        const auto a = static_cast<unsigned short> (i * 2);
        const auto b = static_cast<unsigned short> (a + 1);
        const auto c = static_cast<unsigned short> (a + 2);
        const auto d = static_cast<unsigned short> (a + 3);
        mesh->indices.insert (mesh->indices.end(), { a, c, b, b, c, d });
    }

    const auto bottomCentre = static_cast<unsigned short> (mesh->vertices.size());
    mesh->vertices.push_back ({ 0.0f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f });

    const auto topCentre = static_cast<unsigned short> (mesh->vertices.size());
    mesh->vertices.push_back ({ 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f });

    const auto ringBase = static_cast<unsigned short> (mesh->vertices.size());

    for (int i = 0; i <= sectors; ++i)
    {
        const auto u = static_cast<float> (i) / static_cast<float> (sectors);
        const auto a = u * 2.0f * pi;
        const auto x = 0.5f * std::cos (a);
        const auto z = 0.5f * std::sin (a);
        mesh->vertices.push_back ({ x, -0.5f, z, 0.0f, -1.0f, 0.0f });
        mesh->vertices.push_back ({ x,  0.5f, z, 0.0f,  1.0f, 0.0f });
    }

    for (int i = 0; i < sectors; ++i)
    {
        const auto b0 = static_cast<unsigned short> (ringBase + i * 2);
        const auto b1 = static_cast<unsigned short> (b0 + 2);
        mesh->indices.insert (mesh->indices.end(), { bottomCentre, b1, b0 });

        const auto t0 = static_cast<unsigned short> (ringBase + i * 2 + 1);
        const auto t1 = static_cast<unsigned short> (t0 + 2);
        mesh->indices.insert (mesh->indices.end(), { topCentre, t0, t1 });
    }

    return mesh;
}

static std::unique_ptr<Realtime3DPoseComponent::Mesh> makeBoxMesh()
{
    auto mesh = std::make_unique<Realtime3DPoseComponent::Mesh>();

    const auto addFace = [&] (V3 n, V3 a, V3 b, V3 c, V3 d)
    {
        const auto base = static_cast<unsigned short> (mesh->vertices.size());

        mesh->vertices.push_back ({ a.x, a.y, a.z, n.x, n.y, n.z });
        mesh->vertices.push_back ({ b.x, b.y, b.z, n.x, n.y, n.z });
        mesh->vertices.push_back ({ c.x, c.y, c.z, n.x, n.y, n.z });
        mesh->vertices.push_back ({ d.x, d.y, d.z, n.x, n.y, n.z });

        mesh->indices.insert (mesh->indices.end(),
                              { base,
                                static_cast<unsigned short> (base + 1),
                                static_cast<unsigned short> (base + 2),
                                base,
                                static_cast<unsigned short> (base + 2),
                                static_cast<unsigned short> (base + 3) });
    };

    addFace ({ 0, 0, -1 }, { -0.5f,-0.5f,-0.5f }, { 0.5f,-0.5f,-0.5f }, { 0.5f,0.5f,-0.5f }, { -0.5f,0.5f,-0.5f });
    addFace ({ 0, 0,  1 }, {  0.5f,-0.5f, 0.5f }, { -0.5f,-0.5f, 0.5f }, { -0.5f,0.5f, 0.5f }, { 0.5f,0.5f,0.5f });
    addFace ({ -1,0, 0 }, { -0.5f,-0.5f, 0.5f }, { -0.5f,-0.5f,-0.5f }, { -0.5f,0.5f,-0.5f }, { -0.5f,0.5f,0.5f });
    addFace ({ 1, 0, 0 }, { 0.5f,-0.5f,-0.5f }, { 0.5f,-0.5f,0.5f }, { 0.5f,0.5f,0.5f }, { 0.5f,0.5f,-0.5f });
    addFace ({ 0,-1, 0 }, { -0.5f,-0.5f,0.5f }, { 0.5f,-0.5f,0.5f }, { 0.5f,-0.5f,-0.5f }, { -0.5f,-0.5f,-0.5f });
    addFace ({ 0, 1, 0 }, { -0.5f,0.5f,-0.5f }, { 0.5f,0.5f,-0.5f }, { 0.5f,0.5f,0.5f }, { -0.5f,0.5f,0.5f });

    return mesh;
}

static std::unique_ptr<Realtime3DPoseComponent::Mesh> makeSkirtMesh()
{
    auto mesh = std::make_unique<Realtime3DPoseComponent::Mesh>();
    constexpr int sectors = 28;
    constexpr float bottom = 0.5f;
    constexpr float top = 0.32f;

    for (int i = 0; i <= sectors; ++i)
    {
        const auto u = static_cast<float> (i) / static_cast<float> (sectors);
        const auto a = u * 2.0f * pi;
        const auto ca = std::cos (a);
        const auto sa = std::sin (a);

        const V3 n = normalise ({ ca, (bottom - top) * 0.65f, sa });

        mesh->vertices.push_back ({ bottom * ca, -0.5f, bottom * sa, n.x, n.y, n.z });
        mesh->vertices.push_back ({ top * ca, 0.5f, top * sa, n.x, n.y, n.z });
    }

    for (int i = 0; i < sectors; ++i)
    {
        const auto a = static_cast<unsigned short> (i * 2);
        const auto b = static_cast<unsigned short> (a + 1);
        const auto c = static_cast<unsigned short> (a + 2);
        const auto d = static_cast<unsigned short> (a + 3);
        mesh->indices.insert (mesh->indices.end(), { a, c, b, b, c, d });
    }

    return mesh;
}

static std::unique_ptr<Realtime3DPoseComponent::Mesh> makeDiscMesh()
{
    auto mesh = std::make_unique<Realtime3DPoseComponent::Mesh>();
    constexpr int sectors = 32;

    mesh->vertices.push_back ({ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f });

    for (int i = 0; i <= sectors; ++i)
    {
        const auto u = static_cast<float> (i) / static_cast<float> (sectors);
        const auto a = u * 2.0f * pi;
        mesh->vertices.push_back ({ 0.5f * std::cos (a), 0.0f, 0.5f * std::sin (a),
                                    0.0f, 1.0f, 0.0f });
    }

    for (int i = 0; i < sectors; ++i)
    {
        mesh->indices.insert (mesh->indices.end(),
                              { 0,
                                static_cast<unsigned short> (i + 1),
                                static_cast<unsigned short> (i + 2) });
    }

    return mesh;
}

Realtime3DPoseComponent::Realtime3DPoseComponent()
{
    setOpaque (true);
    setWantsKeyboardFocus (false);

    startMs = juce::Time::getMillisecondCounterHiRes();
    fpsWindowStartMs = startMs;

    startTimerHz (10);
}

Realtime3DPoseComponent::~Realtime3DPoseComponent()
{
    stopTimer();
    shutdownOpenGL();
}

juce::String Realtime3DPoseComponent::preprocessShader (juce::String source)
{
    return source;
}

void Realtime3DPoseComponent::initialise()
{
    using namespace ::juce::gl;

    shaderReady.store (false, std::memory_order_release);

    auto candidate = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);

    const auto vertex = juce::OpenGLHelpers::translateVertexShaderToV3 (
        preprocessShader (vertexShaderSource));
    const auto fragment = juce::OpenGLHelpers::translateFragmentShaderToV3 (
        preprocessShader (fragmentShaderSource));

    if (! candidate->addVertexShader (vertex)
        || ! candidate->addFragmentShader (fragment)
        || ! candidate->link())
    {
        juce::Logger::writeToLog (
            "FLOWER realtime-3D raster shader error: " + candidate->getLastError());
        return;
    }

    shader = std::move (candidate);
    shader->use();

    positionAttribute = glGetAttribLocation (shader->getProgramID(), "position");
    normalAttribute = glGetAttribLocation (shader->getProgramID(), "normal");

    if (positionAttribute < 0 || normalAttribute < 0)
    {
        juce::Logger::writeToLog ("FLOWER realtime-3D attribute lookup failed");
        shader.reset();
        return;
    }

    createMeshes();

    startMs = juce::Time::getMillisecondCounterHiRes();
    fpsWindowStartMs = startMs;
    fpsFrameCount = 0;

    shaderReady.store (true, std::memory_order_release);
}

void Realtime3DPoseComponent::shutdown()
{
    shaderReady.store (false, std::memory_order_release);

    destroyMeshes();
    shader.reset();

    positionAttribute = -1;
    normalAttribute = -1;
}

void Realtime3DPoseComponent::createMeshes()
{
    sphereMesh = makeSphereMesh();
    cylinderMesh = makeCylinderMesh();
    boxMesh = makeBoxMesh();
    skirtMesh = makeSkirtMesh();
    discMesh = makeDiscMesh();

    sphereMesh->upload();
    cylinderMesh->upload();
    boxMesh->upload();
    skirtMesh->upload();
    discMesh->upload();
}

void Realtime3DPoseComponent::destroyMeshes()
{
    if (sphereMesh != nullptr) sphereMesh->release();
    if (cylinderMesh != nullptr) cylinderMesh->release();
    if (boxMesh != nullptr) boxMesh->release();
    if (skirtMesh != nullptr) skirtMesh->release();
    if (discMesh != nullptr) discMesh->release();

    sphereMesh.reset();
    cylinderMesh.reset();
    boxMesh.reset();
    skirtMesh.reset();
    discMesh.reset();
}

void Realtime3DPoseComponent::updatePoseState (double elapsedSeconds)
{
    static constexpr std::array<double, poseCount> durations
    {
        3.0, 6.0, 3.0, 4.0, 3.0, 4.0, 4.0, 4.0, 5.0
    };

    double cycle = 0.0;
    for (const auto duration : durations)
        cycle += duration;

    double cursor = std::fmod (elapsedSeconds, cycle);
    int index = 0;

    while (index < poseCount - 1
           && cursor >= durations[static_cast<size_t> (index)])
    {
        cursor -= durations[static_cast<size_t> (index)];
        ++index;
    }

    const float phase = static_cast<float> (
        cursor / durations[static_cast<size_t> (index)]);

    currentPose.store (index, std::memory_order_relaxed);
    currentPhase.store (juce::jlimit (0.0f, 1.0f, phase),
                        std::memory_order_relaxed);
}

void Realtime3DPoseComponent::drawMesh (const Mesh& mesh,
                                        const juce::Matrix3D<float>& model,
                                        juce::Colour colour,
                                        float alpha,
                                        float material)
{
    using namespace ::juce::gl;

    if (shader == nullptr || mesh.vbo == 0 || mesh.ibo == 0)
        return;

    shader->setUniformMat4 ("uModel", model.mat, 1, GL_FALSE);
    shader->setUniform ("uColour",
                        colour.getFloatRed(),
                        colour.getFloatGreen(),
                        colour.getFloatBlue(),
                        alpha);
    shader->setUniform ("uMaterial", material);

    glBindBuffer (GL_ARRAY_BUFFER, mesh.vbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);

    glVertexAttribPointer (static_cast<GLuint> (positionAttribute),
                           3, GL_FLOAT, GL_FALSE,
                           static_cast<GLsizei> (sizeof (Mesh::Vertex)),
                           nullptr);

    glVertexAttribPointer (static_cast<GLuint> (normalAttribute),
                           3, GL_FLOAT, GL_FALSE,
                           static_cast<GLsizei> (sizeof (Mesh::Vertex)),
                           reinterpret_cast<const void*> (3 * sizeof (float)));

    glEnableVertexAttribArray (static_cast<GLuint> (positionAttribute));
    glEnableVertexAttribArray (static_cast<GLuint> (normalAttribute));

    glDrawElements (GL_TRIANGLES,
                    static_cast<GLsizei> (mesh.indices.size()),
                    GL_UNSIGNED_SHORT,
                    nullptr);

    glDisableVertexAttribArray (static_cast<GLuint> (positionAttribute));
    glDisableVertexAttribArray (static_cast<GLuint> (normalAttribute));

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Realtime3DPoseComponent::renderScene (float elapsedSeconds,
                                           int pose,
                                           float phase)
{
    using namespace ::juce::gl;

    if (shader == nullptr
        || sphereMesh == nullptr
        || cylinderMesh == nullptr
        || boxMesh == nullptr
        || skirtMesh == nullptr
        || discMesh == nullptr)
        return;

    shader->use();
    shader->setUniformMat4 ("uProjection", projectionMatrix.mat, 1, GL_FALSE);
    shader->setUniformMat4 ("uView", viewMatrix.mat, 1, GL_FALSE);
    shader->setUniform ("uTime", elapsedSeconds);

    juce::Rectangle<int> bounds;
    {
        const juce::ScopedLock lock (boundsLock);
        bounds = renderBounds;
    }

    const auto scale = static_cast<float> (openGLContext.getRenderingScale());
    shader->setUniform ("uResolution",
                        juce::jmax (1.0f, scale * static_cast<float> (bounds.getWidth())),
                        juce::jmax (1.0f, scale * static_cast<float> (bounds.getHeight())));

    // Rooftop environment.
    drawMesh (*boxMesh,
              modelTRS ({ 0.0f, -0.10f, 0.7f }, { 8.5f, 0.18f, 8.0f }),
              juce::Colour (0xff777a7b), 1.0f, 1.0f);

    drawMesh (*boxMesh,
              modelTRS ({ 0.0f, 0.36f, 3.05f }, { 8.5f, 0.72f, 0.16f }),
              juce::Colour (0xff66696a), 1.0f, 1.0f);

    drawMesh (*boxMesh,
              modelTRS ({ 1.78f, 1.02f, 2.25f }, { 1.48f, 2.04f, 1.36f }),
              juce::Colour (0xff777979), 1.0f, 1.0f);

    drawMesh (*boxMesh,
              modelTRS ({ 1.78f, 0.78f, 1.555f }, { 0.56f, 1.36f, 0.035f }),
              juce::Colour (0xff363737), 1.0f, 3.0f);

    drawMesh (*boxMesh,
              modelTRS ({ 1.99f, 0.91f, 1.53f }, { 0.040f, 0.065f, 0.040f }),
              juce::Colour (0xffb5b5b2), 1.0f, 2.0f);

    for (int i = -5; i <= 5; ++i)
    {
        const float x = static_cast<float> (i) * 0.72f;
        drawMesh (*boxMesh,
                  modelTRS ({ x, 1.18f, 2.87f }, { 0.035f, 1.55f, 0.035f }),
                  juce::Colour (0xff46494a), 1.0f, 3.0f);
    }

    for (const float y : { 0.72f, 1.28f, 1.68f })
        drawMesh (*boxMesh,
                  modelTRS ({ 0.0f, y, 2.87f }, { 7.25f, 0.030f, 0.030f }),
                  juce::Colour (0xff444748), 1.0f, 3.0f);

    // Character root rotation follows the pose sheet.
    const auto rig = makeRig (pose, phase);
    const auto root = juce::Matrix3D<float>::rotation ({ 0.0f, rig.yaw, 0.0f });

    // Contact shadow: cheap but representative raster-game technique.
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (GL_FALSE);

    V3 shadowPos { 0.0f, 0.006f, 0.02f };
    V3 shadowScale { 0.82f, 1.0f, 0.48f };

    if (pose == 8)
    {
        shadowPos = { -0.05f, 0.006f, -0.02f };
        shadowScale = { 1.85f, 1.0f, 0.62f };
    }
    else if (pose >= 5)
    {
        shadowScale = { 0.95f, 1.0f, 0.62f };
    }

    drawMesh (*discMesh,
              root * modelTRS (shadowPos, shadowScale),
              juce::Colour (0xff101112), 0.24f, 4.0f);

    glDepthMask (GL_TRUE);
    glDisable (GL_BLEND);

    const auto skin = juce::Colour (0xffc5c1ba);
    const auto blouse = juce::Colour (0xffd1d0ca);
    const auto dark = juce::Colour (0xff232426);
    const auto bag = juce::Colour (0xff333436);

    // Hair volumes behind the face.
    drawMesh (*sphereMesh,
              root * modelTRS (rig.head + V3 { 0.0f, 0.035f, 0.045f },
                               { 0.36f, 0.43f, 0.31f }),
              dark, 1.0f, 3.0f);

    drawMesh (*sphereMesh,
              root * modelTRS (rig.head + V3 { 0.0f, -0.17f, 0.085f },
                               { 0.37f, 0.54f, 0.20f }),
              dark, 1.0f, 3.0f);

    // Face/neck.
    drawMesh (*sphereMesh,
              root * modelTRS (rig.head + V3 { 0.0f, -0.01f, -0.055f },
                               { 0.27f, 0.33f, 0.23f }),
              skin, 1.0f, 2.0f);

    drawMesh (*cylinderMesh,
              root * cylinderModel (rig.neck + V3 { 0.0f, -0.055f, 0.0f },
                                    rig.neck + V3 { 0.0f, 0.045f, 0.0f },
                                    0.055f),
              skin, 1.0f, 2.0f);

    // Blouse body and collar/bow.
    drawMesh (*cylinderMesh,
              root * cylinderModel (rig.pelvis + V3 { 0.0f, 0.08f, 0.0f },
                                    rig.chest + V3 { 0.0f, 0.10f, 0.0f },
                                    0.205f),
              blouse, 1.0f, 0.0f);

    drawMesh (*boxMesh,
              root * modelTRS (rig.neck + V3 { 0.0f, -0.105f, -0.14f },
                               { 0.17f, 0.095f, 0.035f },
                               { 0.0f, 0.0f, 0.10f }),
              dark, 1.0f, 3.0f);

    // Pleated-skirt proxy; enough geometry to show material/light quality.
    drawMesh (*skirtMesh,
              root * modelTRS (rig.pelvis + V3 { 0.0f, -0.13f, 0.0f },
                               { 0.75f, 0.44f, 0.75f }),
              dark, 1.0f, 3.0f);

    // Legs and socks.
    drawMesh (*cylinderMesh, root * cylinderModel (rig.hipL, rig.kneeL, 0.078f), skin, 1.0f, 2.0f);
    drawMesh (*cylinderMesh, root * cylinderModel (rig.kneeL, rig.ankleL, 0.063f), skin, 1.0f, 2.0f);
    drawMesh (*cylinderMesh, root * cylinderModel (rig.hipR, rig.kneeR, 0.078f), skin, 1.0f, 2.0f);
    drawMesh (*cylinderMesh, root * cylinderModel (rig.kneeR, rig.ankleR, 0.063f), skin, 1.0f, 2.0f);

    drawMesh (*cylinderMesh,
              root * cylinderModel (rig.ankleL + V3 { 0.0f, 0.12f, 0.0f },
                                    rig.ankleL + V3 { 0.0f, 0.005f, 0.0f },
                                    0.071f),
              dark, 1.0f, 3.0f);
    drawMesh (*cylinderMesh,
              root * cylinderModel (rig.ankleR + V3 { 0.0f, 0.12f, 0.0f },
                                    rig.ankleR + V3 { 0.0f, 0.005f, 0.0f },
                                    0.071f),
              dark, 1.0f, 3.0f);

    drawMesh (*boxMesh,
              root * modelTRS (rig.ankleL + V3 { 0.0f, -0.035f, -0.075f },
                               { 0.18f, 0.10f, 0.29f },
                               { 0.02f, 0.0f, 0.0f }),
              dark, 1.0f, 3.0f);
    drawMesh (*boxMesh,
              root * modelTRS (rig.ankleR + V3 { 0.0f, -0.035f, -0.075f },
                               { 0.18f, 0.10f, 0.29f },
                               { 0.02f, 0.0f, 0.0f }),
              dark, 1.0f, 3.0f);

    // Sleeves, arms, hands.
    drawMesh (*cylinderMesh, root * cylinderModel (rig.shoulderL, rig.elbowL, 0.074f), blouse, 1.0f, 0.0f);
    drawMesh (*cylinderMesh, root * cylinderModel (rig.elbowL, rig.handL, 0.050f), skin, 1.0f, 2.0f);
    drawMesh (*sphereMesh, root * modelTRS (rig.handL, { 0.11f, 0.13f, 0.10f }), skin, 1.0f, 2.0f);

    drawMesh (*cylinderMesh, root * cylinderModel (rig.shoulderR, rig.elbowR, 0.074f), blouse, 1.0f, 0.0f);
    drawMesh (*cylinderMesh, root * cylinderModel (rig.elbowR, rig.handR, 0.050f), skin, 1.0f, 2.0f);
    drawMesh (*sphereMesh, root * modelTRS (rig.handR, { 0.11f, 0.13f, 0.10f }), skin, 1.0f, 2.0f);

    // Bag and strap.
    const V3 bagPos = rig.pelvis + V3 { -0.31f, 0.08f, 0.18f };
    drawMesh (*boxMesh,
              root * modelTRS (bagPos, { 0.38f, 0.47f, 0.15f },
                               { 0.0f, -0.10f, -0.05f }),
              bag, 1.0f, 3.0f);

    drawMesh (*cylinderMesh,
              root * cylinderModel (rig.shoulderL + V3 { 0.0f, 0.0f, 0.05f },
                                    bagPos + V3 { 0.0f, 0.18f, 0.0f },
                                    0.017f),
              bag, 1.0f, 3.0f);
}

void Realtime3DPoseComponent::render()
{
    using namespace ::juce::gl;

    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const double elapsedSeconds = (nowMs - startMs) / 1000.0;

    updatePoseState (elapsedSeconds);

    ++fpsFrameCount;
    const double fpsElapsed = nowMs - fpsWindowStartMs;

    if (fpsElapsed >= 1000.0)
    {
        measuredFps.store (
            static_cast<float> (static_cast<double> (fpsFrameCount) * 1000.0 / fpsElapsed),
            std::memory_order_relaxed);
        fpsWindowStartMs = nowMs;
        fpsFrameCount = 0;
    }

    juce::Rectangle<int> bounds;
    {
        const juce::ScopedLock lock (boundsLock);
        bounds = renderBounds;
    }

    const float renderScale = static_cast<float> (openGLContext.getRenderingScale());
    const int pixelWidth = juce::jmax (1, juce::roundToInt (
        renderScale * static_cast<float> (bounds.getWidth())));
    const int pixelHeight = juce::jmax (1, juce::roundToInt (
        renderScale * static_cast<float> (bounds.getHeight())));

    glViewport (0, 0, pixelWidth, pixelHeight);
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glDisable (GL_CULL_FACE);

    glClearColor (0.57f, 0.59f, 0.60f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (! shaderReady.load (std::memory_order_acquire) || shader == nullptr)
        return;

    const float aspect = static_cast<float> (pixelWidth)
                       / static_cast<float> (juce::jmax (1, pixelHeight));

    constexpr float halfHeight = 0.58f;
    const float halfWidth = halfHeight * aspect;

    projectionMatrix = juce::Matrix3D<float>::fromFrustum (
        -halfWidth, halfWidth,
        -halfHeight, halfHeight,
        1.0f, 30.0f);

    viewMatrix = juce::Matrix3D<float>::fromTranslation (
        { 0.0f, -1.00f, -4.85f });

    renderScene (static_cast<float> (elapsedSeconds),
                 currentPose.load (std::memory_order_relaxed),
                 currentPhase.load (std::memory_order_relaxed));

    glUseProgram (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Realtime3DPoseComponent::paint (juce::Graphics& g)
{
    const auto pose = juce::jlimit (
        0, poseCount - 1, currentPose.load (std::memory_order_relaxed));

    auto top = getLocalBounds().removeFromTop (58).reduced (12, 8);

    g.setColour (juce::Colours::black.withAlpha (0.46f));
    g.fillRoundedRectangle (top.toFloat(), 7.0f);

    g.setColour (juce::Colours::white.withAlpha (0.94f));
    g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
    g.drawText ("FLOWER / REALTIME 3D RASTER QUALITY TEST",
                top.removeFromTop (21),
                juce::Justification::centredLeft,
                false);

    g.setFont (juce::FontOptions (12.0f));
    const auto fps = measuredFps.load (std::memory_order_relaxed);

    g.drawText (
        juce::String (poseNames[pose])
            + "    FPS " + juce::String (fps, 1)
            + "    MESH / LIGHT / FOG / FILM",
        top,
        juce::Justification::centredLeft,
        false);

    if (! shaderReady.load (std::memory_order_acquire))
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
        g.drawFittedText (
            "3D RENDERER INITIALISING / ERROR",
            getLocalBounds().reduced (48),
            juce::Justification::centred,
            2);
    }
}

void Realtime3DPoseComponent::resized()
{
    const juce::ScopedLock lock (boundsLock);
    renderBounds = getLocalBounds();
}

void Realtime3DPoseComponent::timerCallback()
{
    repaint();
}
