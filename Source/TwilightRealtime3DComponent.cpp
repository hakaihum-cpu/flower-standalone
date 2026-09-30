#include "TwilightRealtime3DComponent.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float deg = pi / 180.0f;

enum Scene
{
    Idle = 0,
    WalkStart,
    Walk,
    WalkStop,
    Turn,
    Sit,
    Crouch,
    KneesUp,
    LieDown,
    SceneCount
};

constexpr const char* sceneNames[SceneCount]
{
    "IDLE",
    "WALK START",
    "WALK CYCLE",
    "WALK STOP",
    "DIRECTION / TURN",
    "SIT",
    "CROUCH",
    "KNEES UP",
    "LIE DOWN"
};

juce::Matrix3D<float> scaleMatrix (float x, float y, float z)
{
    return {
        x, 0.0f, 0.0f, 0.0f,
        0.0f, y, 0.0f, 0.0f,
        0.0f, 0.0f, z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

juce::Matrix3D<float> rotateXYZ (float x, float y, float z)
{
    return juce::Matrix3D<float>::rotation ({ x, y, z });
}

juce::Matrix3D<float> translate (float x, float y, float z)
{
    return juce::Matrix3D<float>::fromTranslation ({ x, y, z });
}

float smooth01 (float x)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    return x * x * (3.0f - 2.0f * x);
}

float lerp (float a, float b, float t)
{
    return a + (b - a) * t;
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
    vec4 clip = uProjection * view;

    // Late-90s console-like vertex snapping.  Geometry remains real 3D;
    // only projected coordinates are quantised before rasterisation.
    vec2 ndc = clip.xy / max (0.0001, clip.w);
    ndc = floor (ndc * 180.0 + 0.5) / 180.0;
    clip.xy = ndc * clip.w;

    vWorldPos = world.xyz;
    vNormal = normalize (mat3 (uModel) * normal);
    vDepth = max (0.0, -view.z);

    gl_Position = clip;
}
)GLSL";

const char* fragmentShaderSource = R"GLSL(
#ifdef GL_ES
precision mediump float;
precision mediump int;
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

    vec3 keyDir = normalize (vec3 (-0.36, 0.78, 0.50));
    vec3 fillDir = normalize (vec3 (0.60, 0.25, 0.18));

    float key = max (dot (n, keyDir), 0.0);
    float fill = max (dot (n, fillDir), 0.0);
    float hemi = 0.35 + 0.17 * max (n.y, 0.0);

    vec3 col = uColour.rgb * (hemi + key * 0.66 + fill * 0.10);

    if (uMaterial > 1.5)
    {
        vec3 viewDir = normalize (vec3 (0.0, 1.3, 5.0) - vWorldPos);
        vec3 halfDir = normalize (keyDir + viewDir);
        float spec = pow (max (dot (n, halfDir), 0.0), 28.0);
        col += vec3 (spec * 0.07);
    }

    if (uMaterial > 0.5 && uMaterial < 1.5)
    {
        float concrete = hash21 (floor (vWorldPos.xz * 62.0));
        col *= 0.92 + concrete * 0.11;
    }

    float fog = 1.0 - exp (-0.026 * vDepth * vDepth);
    vec3 fogColour = vec3 (0.53, 0.55, 0.56);
    col = mix (col, fogColour, clamp (fog, 0.0, 0.68));

    float luma = dot (col, vec3 (0.299, 0.587, 0.114));
    col = mix (col, vec3 (luma), 0.78);

    col = clamp ((col - 0.5) * 1.10 + 0.5, 0.0, 1.0);

    vec2 uv = gl_FragCoord.xy / max (uResolution, vec2 (1.0));
    vec2 q = uv * 2.0 - 1.0;

    float vignette = clamp (1.0 - dot (q, q) * 0.14, 0.72, 1.0);
    col *= vignette;

    float grain = hash21 (
        floor (gl_FragCoord.xy)
        + vec2 (floor (uTime * 16.0) * 7.0, floor (uTime * 11.0) * 13.0));

    float dither = (grain - 0.5) / 18.0;
    col = floor (clamp (col + dither, 0.0, 1.0) * 15.0) / 15.0;

    float scan = 0.988 + 0.012 * sin (gl_FragCoord.y * 3.14159265);
    col *= scan;

    gl_FragColor = vec4 (clamp (col, 0.0, 1.0), uColour.a);
}
)GLSL";

}

struct TwilightRealtime3DComponent::Mesh
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

struct TwilightRealtime3DComponent::Pose
{
    int scene = Idle;
    int key = 0;

    float rootX = -1.32f;
    float rootY = 1.02f;
    float rootZ = 0.0f;

    float rootPitch = 0.0f;
    float rootYaw = -90.0f * deg;
    float rootRoll = 0.0f;

    float spinePitch = 0.0f;
    float headPitch = 0.0f;

    float armPitchL = 0.0f;
    float armPitchR = 0.0f;
    float armRollL = -4.0f * deg;
    float armRollR = 4.0f * deg;
    float elbowL = 8.0f * deg;
    float elbowR = 8.0f * deg;

    float hipL = 0.0f;
    float hipR = 0.0f;
    float kneeL = 0.0f;
    float kneeR = 0.0f;
    float ankleL = 0.0f;
    float ankleR = 0.0f;
};

struct TwilightRealtime3DComponent::RigMatrices
{
    juce::Matrix3D<float> root;
    juce::Matrix3D<float> torso;
    juce::Matrix3D<float> neck;
    juce::Matrix3D<float> head;

    juce::Matrix3D<float> upperArmL;
    juce::Matrix3D<float> elbowL;
    juce::Matrix3D<float> upperArmR;
    juce::Matrix3D<float> elbowR;

    juce::Matrix3D<float> upperLegL;
    juce::Matrix3D<float> kneeL;
    juce::Matrix3D<float> upperLegR;
    juce::Matrix3D<float> kneeR;
};

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makeSphereMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    constexpr int rings = 14;
    constexpr int sectors = 20;

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

            mesh->vertices.push_back ({
                0.5f * sx, 0.5f * sy, 0.5f * sz,
                sx, sy, sz
            });
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

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makeSegmentMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    constexpr int sectors = 14;
    constexpr float topRadius = 0.50f;
    constexpr float bottomRadius = 0.42f;

    for (int i = 0; i <= sectors; ++i)
    {
        const float u = static_cast<float> (i) / static_cast<float> (sectors);
        const float a = u * 2.0f * pi;
        const float ca = std::cos (a);
        const float sa = std::sin (a);

        const float nx = ca;
        const float nz = sa;

        mesh->vertices.push_back ({
            topRadius * ca, 0.0f, topRadius * sa,
            nx, 0.05f, nz
        });

        mesh->vertices.push_back ({
            bottomRadius * ca, -1.0f, bottomRadius * sa,
            nx, -0.05f, nz
        });
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

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makeTorsoMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    const std::array<float, 4> y { 0.0f, 0.30f, 0.76f, 1.0f };
    const std::array<float, 4> halfW { 0.34f, 0.36f, 0.48f, 0.40f };
    const std::array<float, 4> halfD { 0.23f, 0.24f, 0.22f, 0.18f };

    for (int ring = 0; ring < 4; ++ring)
    {
        const float yy = y[static_cast<size_t> (ring)];
        const float w = halfW[static_cast<size_t> (ring)];
        const float d = halfD[static_cast<size_t> (ring)];

        mesh->vertices.push_back ({ -w, yy, -d, -0.7f, 0.0f, -0.7f });
        mesh->vertices.push_back ({  w, yy, -d,  0.7f, 0.0f, -0.7f });
        mesh->vertices.push_back ({  w, yy,  d,  0.7f, 0.0f,  0.7f });
        mesh->vertices.push_back ({ -w, yy,  d, -0.7f, 0.0f,  0.7f });
    }

    for (int ring = 0; ring < 3; ++ring)
    {
        const auto a = static_cast<unsigned short> (ring * 4);
        const auto b = static_cast<unsigned short> ((ring + 1) * 4);

        mesh->indices.insert (mesh->indices.end(), {
            static_cast<unsigned short> (a + 0), static_cast<unsigned short> (b + 0), static_cast<unsigned short> (a + 1),
            static_cast<unsigned short> (a + 1), static_cast<unsigned short> (b + 0), static_cast<unsigned short> (b + 1),

            static_cast<unsigned short> (a + 1), static_cast<unsigned short> (b + 1), static_cast<unsigned short> (a + 2),
            static_cast<unsigned short> (a + 2), static_cast<unsigned short> (b + 1), static_cast<unsigned short> (b + 2),

            static_cast<unsigned short> (a + 2), static_cast<unsigned short> (b + 2), static_cast<unsigned short> (a + 3),
            static_cast<unsigned short> (a + 3), static_cast<unsigned short> (b + 2), static_cast<unsigned short> (b + 3),

            static_cast<unsigned short> (a + 3), static_cast<unsigned short> (b + 3), static_cast<unsigned short> (a + 0),
            static_cast<unsigned short> (a + 0), static_cast<unsigned short> (b + 3), static_cast<unsigned short> (b + 0)
        });
    }

    return mesh;
}

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makePleatedSkirtMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    constexpr int sectors = 24;

    for (int i = 0; i <= sectors; ++i)
    {
        const float u = static_cast<float> (i) / static_cast<float> (sectors);
        const float a = u * 2.0f * pi;
        const float pleat = (i % 2 == 0) ? 1.0f : 0.88f;

        const float topR = 0.34f;
        const float bottomR = 0.64f * pleat;
        const float ca = std::cos (a);
        const float sa = std::sin (a);

        mesh->vertices.push_back ({
            topR * ca, 0.0f, topR * sa,
            ca, 0.18f, sa
        });

        mesh->vertices.push_back ({
            bottomR * ca, -1.0f, bottomR * sa,
            ca, 0.12f, sa
        });
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

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makeBoxMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    const auto addFace = [&] (float nx, float ny, float nz,
                              std::array<float, 3> a,
                              std::array<float, 3> b,
                              std::array<float, 3> c,
                              std::array<float, 3> d)
    {
        const auto base = static_cast<unsigned short> (mesh->vertices.size());

        mesh->vertices.push_back ({ a[0], a[1], a[2], nx, ny, nz });
        mesh->vertices.push_back ({ b[0], b[1], b[2], nx, ny, nz });
        mesh->vertices.push_back ({ c[0], c[1], c[2], nx, ny, nz });
        mesh->vertices.push_back ({ d[0], d[1], d[2], nx, ny, nz });

        mesh->indices.insert (mesh->indices.end(), {
            base,
            static_cast<unsigned short> (base + 1),
            static_cast<unsigned short> (base + 2),
            base,
            static_cast<unsigned short> (base + 2),
            static_cast<unsigned short> (base + 3)
        });
    };

    addFace (0, 0, 1, { -0.5f,-0.5f,0.5f }, { 0.5f,-0.5f,0.5f }, { 0.5f,0.5f,0.5f }, { -0.5f,0.5f,0.5f });
    addFace (0, 0,-1, { 0.5f,-0.5f,-0.5f }, { -0.5f,-0.5f,-0.5f }, { -0.5f,0.5f,-0.5f }, { 0.5f,0.5f,-0.5f });
    addFace (1, 0, 0, { 0.5f,-0.5f,0.5f }, { 0.5f,-0.5f,-0.5f }, { 0.5f,0.5f,-0.5f }, { 0.5f,0.5f,0.5f });
    addFace (-1,0, 0, { -0.5f,-0.5f,-0.5f }, { -0.5f,-0.5f,0.5f }, { -0.5f,0.5f,0.5f }, { -0.5f,0.5f,-0.5f });
    addFace (0, 1, 0, { -0.5f,0.5f,0.5f }, { 0.5f,0.5f,0.5f }, { 0.5f,0.5f,-0.5f }, { -0.5f,0.5f,-0.5f });
    addFace (0,-1, 0, { -0.5f,-0.5f,-0.5f }, { 0.5f,-0.5f,-0.5f }, { 0.5f,-0.5f,0.5f }, { -0.5f,-0.5f,0.5f });

    return mesh;
}

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makeShoeMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    const std::array<std::array<float, 3>, 8> p {{
        { -0.50f, 0.00f, -0.55f },
        {  0.50f, 0.00f, -0.55f },
        { -0.46f, 0.00f,  0.72f },
        {  0.46f, 0.00f,  0.72f },
        { -0.46f, 0.42f, -0.45f },
        {  0.46f, 0.42f, -0.45f },
        { -0.34f, 0.27f,  0.62f },
        {  0.34f, 0.27f,  0.62f }
    }};

    for (const auto& v : p)
        mesh->vertices.push_back ({ v[0], v[1], v[2], 0.0f, 1.0f, 0.0f });

    const unsigned short idx[] {
        0,1,5, 0,5,4,
        2,6,7, 2,7,3,
        0,4,6, 0,6,2,
        1,3,7, 1,7,5,
        4,5,7, 4,7,6,
        0,2,3, 0,3,1
    };

    mesh->indices.assign (std::begin (idx), std::end (idx));
    return mesh;
}

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makeHairBackMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    const std::array<std::array<float, 3>, 8> p {{
        { -0.46f,  0.34f, -0.12f },
        {  0.46f,  0.34f, -0.12f },
        { -0.54f, -1.00f, -0.08f },
        {  0.54f, -1.00f, -0.08f },
        { -0.36f,  0.34f,  0.08f },
        {  0.36f,  0.34f,  0.08f },
        { -0.43f, -1.00f,  0.06f },
        {  0.43f, -1.00f,  0.06f }
    }};

    for (const auto& v : p)
        mesh->vertices.push_back ({ v[0], v[1], v[2], 0.0f, 0.0f, -1.0f });

    const unsigned short idx[] {
        0,2,1, 1,2,3,
        4,5,6, 5,7,6,
        0,4,2, 4,6,2,
        1,3,5, 5,3,7,
        0,1,4, 1,5,4,
        2,6,3, 3,6,7
    };

    mesh->indices.assign (std::begin (idx), std::end (idx));
    return mesh;
}

static std::unique_ptr<TwilightRealtime3DComponent::Mesh> makeQuadMesh()
{
    auto mesh = std::make_unique<TwilightRealtime3DComponent::Mesh>();

    mesh->vertices = {
        { -0.5f,-0.5f,0.0f, 0.0f,0.0f,1.0f },
        {  0.5f,-0.5f,0.0f, 0.0f,0.0f,1.0f },
        {  0.5f, 0.5f,0.0f, 0.0f,0.0f,1.0f },
        { -0.5f, 0.5f,0.0f, 0.0f,0.0f,1.0f }
    };

    mesh->indices = { 0,1,2, 0,2,3 };
    return mesh;
}

static TwilightRealtime3DComponent::Pose makeNeutralPose()
{
    TwilightRealtime3DComponent::Pose p;
    return p;
}

static TwilightRealtime3DComponent::Pose blendPose (
    const TwilightRealtime3DComponent::Pose& a,
    const TwilightRealtime3DComponent::Pose& b,
    float t)
{
    TwilightRealtime3DComponent::Pose p = b;

    p.rootX = lerp (a.rootX, b.rootX, t);
    p.rootY = lerp (a.rootY, b.rootY, t);
    p.rootZ = lerp (a.rootZ, b.rootZ, t);

    p.rootPitch = lerp (a.rootPitch, b.rootPitch, t);
    p.rootYaw = lerp (a.rootYaw, b.rootYaw, t);
    p.rootRoll = lerp (a.rootRoll, b.rootRoll, t);

    p.spinePitch = lerp (a.spinePitch, b.spinePitch, t);
    p.headPitch = lerp (a.headPitch, b.headPitch, t);

    p.armPitchL = lerp (a.armPitchL, b.armPitchL, t);
    p.armPitchR = lerp (a.armPitchR, b.armPitchR, t);
    p.armRollL = lerp (a.armRollL, b.armRollL, t);
    p.armRollR = lerp (a.armRollR, b.armRollR, t);
    p.elbowL = lerp (a.elbowL, b.elbowL, t);
    p.elbowR = lerp (a.elbowR, b.elbowR, t);

    p.hipL = lerp (a.hipL, b.hipL, t);
    p.hipR = lerp (a.hipR, b.hipR, t);
    p.kneeL = lerp (a.kneeL, b.kneeL, t);
    p.kneeR = lerp (a.kneeR, b.kneeR, t);
    p.ankleL = lerp (a.ankleL, b.ankleL, t);
    p.ankleR = lerp (a.ankleR, b.ankleR, t);

    return p;
}

static TwilightRealtime3DComponent::Pose makeWalkKey (int key)
{
    static constexpr float hipL[8] {
         27.0f, 17.0f,  5.0f, -12.0f,
        -27.0f,-17.0f, -5.0f,  12.0f
    };

    static constexpr float hipR[8] {
        -27.0f,-17.0f, -5.0f,  12.0f,
         27.0f, 17.0f,  5.0f, -12.0f
    };

    static constexpr float kneeL[8] {
         5.0f,  4.0f,  2.0f, 12.0f,
        34.0f, 26.0f, 12.0f,  6.0f
    };

    static constexpr float kneeR[8] {
        34.0f, 26.0f, 12.0f,  6.0f,
         5.0f,  4.0f,  2.0f, 12.0f
    };

    static constexpr float armL[8] {
        -18.0f,-12.0f,-5.0f,  8.0f,
         18.0f, 12.0f, 5.0f, -8.0f
    };

    static constexpr float armR[8] {
         18.0f, 12.0f, 5.0f, -8.0f,
        -18.0f,-12.0f,-5.0f,  8.0f
    };

    const int k = juce::jlimit (0, 7, key);

    auto p = makeNeutralPose();
    p.scene = Walk;
    p.key = k;
    p.rootYaw = -90.0f * deg;
    p.rootY = 1.02f + ((k == 1 || k == 5) ? 0.018f : 0.0f);

    p.hipL = hipL[k] * deg;
    p.hipR = hipR[k] * deg;
    p.kneeL = kneeL[k] * deg;
    p.kneeR = kneeR[k] * deg;

    p.armPitchL = armL[k] * deg;
    p.armPitchR = armR[k] * deg;
    p.elbowL = 9.0f * deg;
    p.elbowR = 9.0f * deg;

    p.spinePitch = -2.0f * deg;
    p.headPitch = 2.0f * deg;

    return p;
}

static TwilightRealtime3DComponent::Pose makeSitPose()
{
    auto p = makeNeutralPose();
    p.scene = Sit;
    p.rootX = 0.35f;
    p.rootY = 0.61f;
    p.rootYaw = -18.0f * deg;

    p.spinePitch = 5.0f * deg;
    p.headPitch = 11.0f * deg;

    p.hipL = -82.0f * deg;
    p.hipR = -82.0f * deg;
    p.kneeL = 86.0f * deg;
    p.kneeR = 86.0f * deg;

    p.armPitchL = -24.0f * deg;
    p.armPitchR = -24.0f * deg;
    p.elbowL = 58.0f * deg;
    p.elbowR = 58.0f * deg;
    p.armRollL = 10.0f * deg;
    p.armRollR = -10.0f * deg;

    return p;
}

static TwilightRealtime3DComponent::Pose makeCrouchPose()
{
    auto p = makeNeutralPose();
    p.scene = Crouch;
    p.rootX = -0.10f;
    p.rootY = 0.72f;
    p.rootYaw = 10.0f * deg;

    p.spinePitch = 18.0f * deg;
    p.headPitch = 18.0f * deg;

    p.hipL = -49.0f * deg;
    p.hipR = -49.0f * deg;
    p.kneeL = 102.0f * deg;
    p.kneeR = 102.0f * deg;

    p.armPitchL = -18.0f * deg;
    p.armPitchR = -18.0f * deg;
    p.elbowL = 58.0f * deg;
    p.elbowR = 58.0f * deg;

    return p;
}

static TwilightRealtime3DComponent::Pose makeKneesPose()
{
    auto p = makeNeutralPose();
    p.scene = KneesUp;
    p.rootX = -0.42f;
    p.rootY = 0.48f;
    p.rootYaw = -8.0f * deg;

    p.spinePitch = 20.0f * deg;
    p.headPitch = 22.0f * deg;

    p.hipL = -118.0f * deg;
    p.hipR = -118.0f * deg;
    p.kneeL = 126.0f * deg;
    p.kneeR = 126.0f * deg;

    p.armPitchL = -63.0f * deg;
    p.armPitchR = -63.0f * deg;
    p.elbowL = 104.0f * deg;
    p.elbowR = 104.0f * deg;
    p.armRollL = 12.0f * deg;
    p.armRollR = -12.0f * deg;

    return p;
}

static TwilightRealtime3DComponent::Pose makeLiePose()
{
    auto p = makeNeutralPose();
    p.scene = LieDown;
    p.rootX = -0.35f;
    p.rootY = 0.33f;
    p.rootZ = 0.0f;
    p.rootYaw = -18.0f * deg;
    p.rootRoll = 88.0f * deg;

    p.spinePitch = 4.0f * deg;
    p.headPitch = 10.0f * deg;

    p.hipL = 10.0f * deg;
    p.hipR = -8.0f * deg;
    p.kneeL = 26.0f * deg;
    p.kneeR = 38.0f * deg;

    p.armPitchL = -35.0f * deg;
    p.armPitchR = -18.0f * deg;
    p.elbowL = 42.0f * deg;
    p.elbowR = 66.0f * deg;

    return p;
}

TwilightRealtime3DComponent::TwilightRealtime3DComponent()
{
    setOpaque (true);
    setWantsKeyboardFocus (false);

    startMs = juce::Time::getMillisecondCounterHiRes();
    fpsWindowStartMs = startMs;

    startTimerHz (10);
}

TwilightRealtime3DComponent::~TwilightRealtime3DComponent()
{
    stopTimer();
    shutdownOpenGL();
}

void TwilightRealtime3DComponent::initialise()
{
    using namespace ::juce::gl;

    shaderReady.store (false, std::memory_order_release);

    auto candidate = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);

    const auto vertex =
        juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShaderSource);

    const auto fragment =
        juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentShaderSource);

    if (! candidate->addVertexShader (vertex)
        || ! candidate->addFragmentShader (fragment)
        || ! candidate->link())
    {
        juce::Logger::writeToLog (
            "FLOWER TW3D shader error: " + candidate->getLastError());
        return;
    }

    shader = std::move (candidate);
    shader->use();

    positionAttribute =
        glGetAttribLocation (shader->getProgramID(), "position");

    normalAttribute =
        glGetAttribLocation (shader->getProgramID(), "normal");

    if (positionAttribute < 0 || normalAttribute < 0)
    {
        juce::Logger::writeToLog ("FLOWER TW3D attribute lookup failed");
        shader.reset();
        return;
    }

    createMeshes();

    startMs = juce::Time::getMillisecondCounterHiRes();
    fpsWindowStartMs = startMs;
    fpsFrameCount = 0;

    shaderReady.store (true, std::memory_order_release);
}

void TwilightRealtime3DComponent::shutdown()
{
    shaderReady.store (false, std::memory_order_release);

    destroyMeshes();
    shader.reset();

    positionAttribute = -1;
    normalAttribute = -1;
}

void TwilightRealtime3DComponent::createMeshes()
{
    sphereMesh = makeSphereMesh();
    segmentMesh = makeSegmentMesh();
    torsoMesh = makeTorsoMesh();
    skirtMesh = makePleatedSkirtMesh();
    boxMesh = makeBoxMesh();
    shoeMesh = makeShoeMesh();
    hairBackMesh = makeHairBackMesh();
    quadMesh = makeQuadMesh();

    sphereMesh->upload();
    segmentMesh->upload();
    torsoMesh->upload();
    skirtMesh->upload();
    boxMesh->upload();
    shoeMesh->upload();
    hairBackMesh->upload();
    quadMesh->upload();
}

void TwilightRealtime3DComponent::destroyMeshes()
{
    if (sphereMesh != nullptr) sphereMesh->release();
    if (segmentMesh != nullptr) segmentMesh->release();
    if (torsoMesh != nullptr) torsoMesh->release();
    if (skirtMesh != nullptr) skirtMesh->release();
    if (boxMesh != nullptr) boxMesh->release();
    if (shoeMesh != nullptr) shoeMesh->release();
    if (hairBackMesh != nullptr) hairBackMesh->release();
    if (quadMesh != nullptr) quadMesh->release();

    sphereMesh.reset();
    segmentMesh.reset();
    torsoMesh.reset();
    skirtMesh.reset();
    boxMesh.reset();
    shoeMesh.reset();
    hairBackMesh.reset();
    quadMesh.reset();
}

TwilightRealtime3DComponent::Pose
TwilightRealtime3DComponent::evaluatePose (double elapsedSeconds) const
{
    constexpr double idleDuration = 2.0;
    constexpr double startDuration = 1.0;
    constexpr double walkDuration = 5.0;
    constexpr double stopDuration = 1.0;
    constexpr double turnDuration = 2.0;
    constexpr double sitDuration = 3.0;
    constexpr double crouchDuration = 3.0;
    constexpr double kneesDuration = 3.5;
    constexpr double lieDuration = 4.0;

    constexpr double cycle =
        idleDuration + startDuration + walkDuration + stopDuration
        + turnDuration + sitDuration + crouchDuration
        + kneesDuration + lieDuration;

    double t = std::fmod (elapsedSeconds, cycle);

    auto neutral = makeNeutralPose();

    if (t < idleDuration)
    {
        neutral.scene = Idle;
        neutral.key = 0;
        neutral.rootX = -1.32f;
        neutral.rootYaw = 0.0f;
        return neutral;
    }

    t -= idleDuration;

    if (t < startDuration)
    {
        const float p =
            smooth01 (static_cast<float> (t / startDuration));

        const int key = juce::jlimit (
            0, 3, static_cast<int> (std::floor (p * 4.0f)));

        auto walking = makeWalkKey (key);
        walking.scene = WalkStart;
        walking.rootX = lerp (-1.32f, -1.12f, p);

        neutral.rootX = -1.32f;
        neutral.rootYaw = -90.0f * deg;

        auto result = blendPose (neutral, walking, p);
        result.scene = WalkStart;
        result.key = key;
        return result;
    }

    t -= startDuration;

    if (t < walkDuration)
    {
        const float p =
            static_cast<float> (t / walkDuration);

        const int key =
            static_cast<int> (std::floor (t * 16.0)) % 8;

        auto walking = makeWalkKey (key);
        walking.scene = Walk;
        walking.rootX = lerp (-1.12f, 1.08f, p);

        return walking;
    }

    t -= walkDuration;

    if (t < stopDuration)
    {
        const float p =
            smooth01 (static_cast<float> (t / stopDuration));

        const int key = 4 + juce::jlimit (
            0, 3, static_cast<int> (std::floor (p * 4.0f)));

        auto walking = makeWalkKey (key);
        walking.scene = WalkStop;
        walking.rootX = lerp (1.08f, 1.20f, p);

        neutral.rootX = 1.20f;
        neutral.rootYaw = -90.0f * deg;

        auto result = blendPose (walking, neutral, p);
        result.scene = WalkStop;
        result.key = key;
        return result;
    }

    t -= stopDuration;

    if (t < turnDuration)
    {
        const float raw =
            static_cast<float> (t / turnDuration);

        const int key = juce::jlimit (
            0, 7, static_cast<int> (std::floor (raw * 8.0f)));

        neutral.scene = Turn;
        neutral.key = key;
        neutral.rootX = 1.20f;
        neutral.rootYaw =
            lerp (-90.0f * deg, 90.0f * deg,
                  static_cast<float> (key) / 7.0f);

        return neutral;
    }

    t -= turnDuration;

    if (t < sitDuration)
    {
        const float p =
            smooth01 (juce::jlimit (
                0.0f, 1.0f,
                static_cast<float> (t / 0.85)));

        neutral.rootX = 0.35f;
        neutral.rootYaw = -18.0f * deg;

        auto result = blendPose (neutral, makeSitPose(), p);
        result.scene = Sit;
        result.key = juce::jlimit (0, 7, static_cast<int> (p * 7.99f));
        return result;
    }

    t -= sitDuration;

    if (t < crouchDuration)
    {
        const float p =
            smooth01 (juce::jlimit (
                0.0f, 1.0f,
                static_cast<float> (t / 0.85)));

        auto result =
            blendPose (makeSitPose(), makeCrouchPose(), p);

        result.scene = Crouch;
        result.key = juce::jlimit (0, 7, static_cast<int> (p * 7.99f));
        return result;
    }

    t -= crouchDuration;

    if (t < kneesDuration)
    {
        const float p =
            smooth01 (juce::jlimit (
                0.0f, 1.0f,
                static_cast<float> (t / 0.95)));

        auto result =
            blendPose (makeCrouchPose(), makeKneesPose(), p);

        result.scene = KneesUp;
        result.key = juce::jlimit (0, 7, static_cast<int> (p * 7.99f));
        return result;
    }

    t -= kneesDuration;

    {
        const float p =
            smooth01 (juce::jlimit (
                0.0f, 1.0f,
                static_cast<float> (t / 1.10)));

        auto result =
            blendPose (makeKneesPose(), makeLiePose(), p);

        result.scene = LieDown;
        result.key = juce::jlimit (0, 7, static_cast<int> (p * 7.99f));
        return result;
    }
}

TwilightRealtime3DComponent::RigMatrices
TwilightRealtime3DComponent::buildRig (const Pose& pose) const
{
    RigMatrices r;

    constexpr float torsoHeight = 0.58f;
    constexpr float upperArmLength = 0.33f;
    constexpr float upperLegLength = 0.47f;

    r.root =
        translate (pose.rootX, pose.rootY, pose.rootZ)
        * rotateXYZ (pose.rootPitch, pose.rootYaw, pose.rootRoll);

    r.torso =
        r.root
        * rotateXYZ (pose.spinePitch, 0.0f, 0.0f);

    r.neck =
        r.torso
        * translate (0.0f, torsoHeight, 0.0f);

    r.head =
        r.neck
        * rotateXYZ (pose.headPitch, 0.0f, 0.0f)
        * translate (0.0f, 0.17f, 0.0f);

    r.upperArmL =
        r.torso
        * translate (-0.285f, 0.50f, 0.0f)
        * rotateXYZ (pose.armPitchL, 0.0f, pose.armRollL);

    r.elbowL =
        r.upperArmL
        * translate (0.0f, -upperArmLength, 0.0f)
        * rotateXYZ (pose.elbowL, 0.0f, 0.0f);

    r.upperArmR =
        r.torso
        * translate (0.285f, 0.50f, 0.0f)
        * rotateXYZ (pose.armPitchR, 0.0f, pose.armRollR);

    r.elbowR =
        r.upperArmR
        * translate (0.0f, -upperArmLength, 0.0f)
        * rotateXYZ (pose.elbowR, 0.0f, 0.0f);

    r.upperLegL =
        r.root
        * translate (-0.125f, -0.06f, 0.0f)
        * rotateXYZ (pose.hipL, 0.0f, 0.0f);

    r.kneeL =
        r.upperLegL
        * translate (0.0f, -upperLegLength, 0.0f)
        * rotateXYZ (pose.kneeL, 0.0f, 0.0f);

    r.upperLegR =
        r.root
        * translate (0.125f, -0.06f, 0.0f)
        * rotateXYZ (pose.hipR, 0.0f, 0.0f);

    r.kneeR =
        r.upperLegR
        * translate (0.0f, -upperLegLength, 0.0f)
        * rotateXYZ (pose.kneeR, 0.0f, 0.0f);

    return r;
}

void TwilightRealtime3DComponent::drawMesh (
    const Mesh& mesh,
    const juce::Matrix3D<float>& model,
    juce::Colour colour,
    float material,
    float alpha)
{
    using namespace ::juce::gl;

    if (shader == nullptr || mesh.vbo == 0 || mesh.ibo == 0)
        return;

    shader->setUniformMat4 ("uModel", model.mat, 1, GL_FALSE);

    shader->setUniform (
        "uColour",
        colour.getFloatRed(),
        colour.getFloatGreen(),
        colour.getFloatBlue(),
        alpha);

    shader->setUniform ("uMaterial", material);

    glBindBuffer (GL_ARRAY_BUFFER, mesh.vbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);

    glVertexAttribPointer (
        static_cast<GLuint> (positionAttribute),
        3, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei> (sizeof (Mesh::Vertex)),
        nullptr);

    glVertexAttribPointer (
        static_cast<GLuint> (normalAttribute),
        3, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei> (sizeof (Mesh::Vertex)),
        reinterpret_cast<const void*> (3 * sizeof (float)));

    glEnableVertexAttribArray (
        static_cast<GLuint> (positionAttribute));

    glEnableVertexAttribArray (
        static_cast<GLuint> (normalAttribute));

    glDrawElements (
        GL_TRIANGLES,
        static_cast<GLsizei> (mesh.indices.size()),
        GL_UNSIGNED_SHORT,
        nullptr);

    glDisableVertexAttribArray (
        static_cast<GLuint> (positionAttribute));

    glDisableVertexAttribArray (
        static_cast<GLuint> (normalAttribute));
}

void TwilightRealtime3DComponent::renderCharacter (
    const Pose& pose,
    const RigMatrices& rig)
{
    if (sphereMesh == nullptr
        || segmentMesh == nullptr
        || torsoMesh == nullptr
        || skirtMesh == nullptr
        || boxMesh == nullptr
        || shoeMesh == nullptr
        || hairBackMesh == nullptr
        || quadMesh == nullptr)
        return;

    const auto skin = juce::Colour (0xffc9c1b8);
    const auto blouse = juce::Colour (0xffddd9d0);
    const auto dark = juce::Colour (0xff1b1c1f);
    const auto hair = juce::Colour (0xff171719);
    const auto bag = juce::Colour (0xff242529);

    constexpr float upperArmLength = 0.33f;
    constexpr float forearmLength = 0.31f;
    constexpr float upperLegLength = 0.47f;
    constexpr float lowerLegLength = 0.45f;

    // Torso / white short-sleeve blouse.
    drawMesh (
        *torsoMesh,
        rig.torso * scaleMatrix (0.92f, 0.58f, 0.92f),
        blouse,
        0.0f);

    // Sailor-like dark collar and bow.
    drawMesh (
        *boxMesh,
        rig.torso
            * translate (0.0f, 0.49f, 0.115f)
            * rotateXYZ (0.10f, 0.0f, 0.0f)
            * scaleMatrix (0.30f, 0.07f, 0.028f),
        dark,
        2.0f);

    drawMesh (
        *quadMesh,
        rig.torso
            * translate (0.0f, 0.43f, 0.145f)
            * rotateXYZ (0.0f, 0.0f, 0.12f)
            * scaleMatrix (0.18f, 0.15f, 1.0f),
        dark,
        2.0f);

    // Pleated skirt: separate real mesh, not a sprite.
    drawMesh (
        *skirtMesh,
        rig.root
            * translate (0.0f, 0.02f, 0.0f)
            * scaleMatrix (0.58f, 0.34f, 0.54f),
        dark,
        2.0f);

    // Head and long straight hair.
    drawMesh (
        *hairBackMesh,
        rig.head
            * translate (0.0f, -0.03f, -0.035f)
            * scaleMatrix (0.40f, 0.57f, 0.34f),
        hair,
        2.0f);

    drawMesh (
        *sphereMesh,
        rig.head
            * translate (0.0f, 0.03f, -0.015f)
            * scaleMatrix (0.35f, 0.42f, 0.32f),
        hair,
        2.0f);

    drawMesh (
        *sphereMesh,
        rig.head
            * translate (0.0f, -0.005f, 0.055f)
            * scaleMatrix (0.282f, 0.338f, 0.244f),
        skin,
        1.0f);

    // Fringe / bangs.
    drawMesh (
        *boxMesh,
        rig.head
            * translate (0.0f, 0.09f, 0.178f)
            * rotateXYZ (-0.12f, 0.0f, 0.0f)
            * scaleMatrix (0.29f, 0.12f, 0.030f),
        hair,
        2.0f);

    // Eyes: tiny 3D planes attached to the head.  These rotate with the skull.
    drawMesh (
        *quadMesh,
        rig.head
            * translate (-0.060f, 0.015f, 0.183f)
            * scaleMatrix (0.032f, 0.020f, 1.0f),
        juce::Colour (0xff18181a),
        2.0f);

    drawMesh (
        *quadMesh,
        rig.head
            * translate (0.060f, 0.015f, 0.183f)
            * scaleMatrix (0.032f, 0.020f, 1.0f),
        juce::Colour (0xff18181a),
        2.0f);

    // Upper arms: blouse sleeves.
    drawMesh (
        *segmentMesh,
        rig.upperArmL
            * scaleMatrix (0.15f, upperArmLength, 0.15f),
        blouse,
        0.0f);

    drawMesh (
        *segmentMesh,
        rig.upperArmR
            * scaleMatrix (0.15f, upperArmLength, 0.15f),
        blouse,
        0.0f);

    // Forearms / hands.
    drawMesh (
        *segmentMesh,
        rig.elbowL
            * scaleMatrix (0.102f, forearmLength, 0.102f),
        skin,
        1.0f);

    drawMesh (
        *segmentMesh,
        rig.elbowR
            * scaleMatrix (0.102f, forearmLength, 0.102f),
        skin,
        1.0f);

    drawMesh (
        *sphereMesh,
        rig.elbowL
            * translate (0.0f, -forearmLength - 0.025f, 0.0f)
            * scaleMatrix (0.105f, 0.145f, 0.085f),
        skin,
        1.0f);

    drawMesh (
        *sphereMesh,
        rig.elbowR
            * translate (0.0f, -forearmLength - 0.025f, 0.0f)
            * scaleMatrix (0.105f, 0.145f, 0.085f),
        skin,
        1.0f);

    // Legs.
    drawMesh (
        *segmentMesh,
        rig.upperLegL
            * scaleMatrix (0.145f, upperLegLength, 0.145f),
        skin,
        1.0f);

    drawMesh (
        *segmentMesh,
        rig.upperLegR
            * scaleMatrix (0.145f, upperLegLength, 0.145f),
        skin,
        1.0f);

    drawMesh (
        *segmentMesh,
        rig.kneeL
            * scaleMatrix (0.115f, lowerLegLength, 0.115f),
        skin,
        1.0f);

    drawMesh (
        *segmentMesh,
        rig.kneeR
            * scaleMatrix (0.115f, lowerLegLength, 0.115f),
        skin,
        1.0f);

    const auto ankleL =
        rig.kneeL
        * translate (0.0f, -lowerLegLength, 0.0f)
        * rotateXYZ (pose.ankleL, 0.0f, 0.0f);

    const auto ankleR =
        rig.kneeR
        * translate (0.0f, -lowerLegLength, 0.0f)
        * rotateXYZ (pose.ankleR, 0.0f, 0.0f);

    // Black socks.
    drawMesh (
        *segmentMesh,
        ankleL
            * translate (0.0f, 0.15f, 0.0f)
            * scaleMatrix (0.126f, 0.18f, 0.126f),
        dark,
        2.0f);

    drawMesh (
        *segmentMesh,
        ankleR
            * translate (0.0f, 0.15f, 0.0f)
            * scaleMatrix (0.126f, 0.18f, 0.126f),
        dark,
        2.0f);

    // Chunky black shoes.
    drawMesh (
        *shoeMesh,
        ankleL
            * translate (0.0f, -0.045f, 0.10f)
            * scaleMatrix (0.18f, 0.14f, 0.30f),
        dark,
        2.0f);

    drawMesh (
        *shoeMesh,
        ankleR
            * translate (0.0f, -0.045f, 0.10f)
            * scaleMatrix (0.18f, 0.14f, 0.30f),
        dark,
        2.0f);

    // Shoulder bag and strap.
    const auto bagFrame =
        rig.torso
        * translate (-0.39f, 0.17f, -0.015f)
        * rotateXYZ (0.02f, 0.08f, -0.08f);

    drawMesh (
        *boxMesh,
        bagFrame
            * scaleMatrix (0.34f, 0.42f, 0.14f),
        bag,
        2.0f);

    drawMesh (
        *boxMesh,
        rig.torso
            * translate (-0.17f, 0.34f, 0.035f)
            * rotateXYZ (0.05f, 0.05f, -0.48f)
            * scaleMatrix (0.035f, 0.74f, 0.026f),
        bag,
        2.0f);
}

void TwilightRealtime3DComponent::renderEnvironment (float)
{
    if (boxMesh == nullptr)
        return;

    const auto concrete = juce::Colour (0xff747779);
    const auto concreteDark = juce::Colour (0xff5e6163);
    const auto metal = juce::Colour (0xff343638);
    const auto door = juce::Colour (0xff404244);

    // Rooftop floor.
    drawMesh (
        *boxMesh,
        translate (0.0f, -0.13f, -0.35f)
            * scaleMatrix (7.5f, 0.18f, 6.0f),
        concrete,
        1.0f);

    // Rear parapet.
    drawMesh (
        *boxMesh,
        translate (0.0f, 0.30f, -2.30f)
            * scaleMatrix (7.5f, 0.72f, 0.16f),
        concreteDark,
        1.0f);

    // Utility-room block, pushed behind the actor.
    drawMesh (
        *boxMesh,
        translate (1.82f, 0.92f, -2.02f)
            * scaleMatrix (1.42f, 1.98f, 1.16f),
        juce::Colour (0xff797b7b),
        1.0f);

    drawMesh (
        *boxMesh,
        translate (1.55f, 0.70f, -1.40f)
            * scaleMatrix (0.54f, 1.30f, 0.05f),
        door,
        2.0f);

    // Railing remains behind the character so it never hides the pose study.
    for (int i = -6; i <= 6; ++i)
    {
        drawMesh (
            *boxMesh,
            translate (static_cast<float> (i) * 0.58f, 0.98f, -1.48f)
                * scaleMatrix (0.030f, 1.52f, 0.030f),
            metal,
            2.0f);
    }

    for (float y : { 0.58f, 1.06f, 1.48f })
    {
        drawMesh (
            *boxMesh,
            translate (0.0f, y, -1.48f)
                * scaleMatrix (7.2f, 0.026f, 0.026f),
            metal,
            2.0f);
    }
}

void TwilightRealtime3DComponent::renderScene (
    const Pose& pose,
    float elapsedSeconds)
{
    using namespace ::juce::gl;

    shader->use();

    shader->setUniformMat4 (
        "uProjection",
        projectionMatrix.mat,
        1,
        GL_FALSE);

    shader->setUniformMat4 (
        "uView",
        viewMatrix.mat,
        1,
        GL_FALSE);

    shader->setUniform ("uTime", elapsedSeconds);

    juce::Rectangle<int> bounds;
    {
        const juce::ScopedLock lock (boundsLock);
        bounds = renderBounds;
    }

    const float renderScale =
        static_cast<float> (openGLContext.getRenderingScale());

    shader->setUniform (
        "uResolution",
        juce::jmax (1.0f, renderScale * static_cast<float> (bounds.getWidth())),
        juce::jmax (1.0f, renderScale * static_cast<float> (bounds.getHeight())));

    renderEnvironment (elapsedSeconds);

    const auto rig = buildRig (pose);
    renderCharacter (pose, rig);
}

void TwilightRealtime3DComponent::render()
{
    using namespace ::juce::gl;

    const double nowMs =
        juce::Time::getMillisecondCounterHiRes();

    const double elapsedSeconds =
        (nowMs - startMs) / 1000.0;

    const auto pose = evaluatePose (elapsedSeconds);

    currentScene.store (pose.scene, std::memory_order_relaxed);
    currentKey.store (pose.key, std::memory_order_relaxed);

    ++fpsFrameCount;

    const double fpsElapsed =
        nowMs - fpsWindowStartMs;

    if (fpsElapsed >= 1000.0)
    {
        measuredFps.store (
            static_cast<float> (
                static_cast<double> (fpsFrameCount)
                * 1000.0
                / fpsElapsed),
            std::memory_order_relaxed);

        fpsWindowStartMs = nowMs;
        fpsFrameCount = 0;
    }

    juce::Rectangle<int> bounds;
    {
        const juce::ScopedLock lock (boundsLock);
        bounds = renderBounds;
    }

    const float renderScale =
        static_cast<float> (openGLContext.getRenderingScale());

    const int pixelWidth =
        juce::jmax (
            1,
            juce::roundToInt (
                renderScale
                * static_cast<float> (bounds.getWidth())));

    const int pixelHeight =
        juce::jmax (
            1,
            juce::roundToInt (
                renderScale
                * static_cast<float> (bounds.getHeight())));

    glViewport (0, 0, pixelWidth, pixelHeight);

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glDisable (GL_CULL_FACE);

    glClearColor (0.52f, 0.54f, 0.55f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (! shaderReady.load (std::memory_order_acquire)
        || shader == nullptr)
        return;

    const float aspect =
        static_cast<float> (pixelWidth)
        / static_cast<float> (juce::jmax (1, pixelHeight));

    constexpr float halfHeight = 0.56f;
    const float halfWidth = halfHeight * aspect;

    projectionMatrix =
        juce::Matrix3D<float>::fromFrustum (
            -halfWidth,
             halfWidth,
            -halfHeight,
             halfHeight,
             1.0f,
             30.0f);

    // Fixed-camera composition: mild perspective, side-on rooftop view.
    viewMatrix =
        translate (0.0f, -0.92f, -4.70f)
        * rotateXYZ (-0.035f, 0.0f, 0.0f);

    renderScene (
        pose,
        static_cast<float> (elapsedSeconds));

    glUseProgram (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void TwilightRealtime3DComponent::paint (juce::Graphics& g)
{
    const int scene =
        juce::jlimit (
            0,
            SceneCount - 1,
            currentScene.load (std::memory_order_relaxed));

    const int key =
        currentKey.load (std::memory_order_relaxed);

    auto top =
        getLocalBounds().removeFromTop (58).reduced (12, 8);

    g.setColour (juce::Colours::black.withAlpha (0.56f));
    g.fillRoundedRectangle (top.toFloat(), 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));

    g.drawText (
        "FLOWER / TWILIGHT / REALTIME 3D CHARACTER",
        top.removeFromTop (21),
        juce::Justification::centredLeft,
        false);

    g.setFont (juce::FontOptions (11.0f));

    g.drawText (
        juce::String (sceneNames[scene])
            + "   KEY " + juce::String (key + 1)
            + "   FPS " + juce::String (
                measuredFps.load (std::memory_order_relaxed), 1)
            + "   BONE RIG / POLYGON / NO SPRITES",
        top,
        juce::Justification::centredLeft,
        false);

    if (! shaderReady.load (std::memory_order_acquire))
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));

        g.drawFittedText (
            "REALTIME 3D RENDERER INITIALISING / ERROR",
            getLocalBounds().reduced (48),
            juce::Justification::centred,
            2);
    }
}

void TwilightRealtime3DComponent::resized()
{
    const juce::ScopedLock lock (boundsLock);
    renderBounds = getLocalBounds();
}

void TwilightRealtime3DComponent::timerCallback()
{
    repaint();
}
