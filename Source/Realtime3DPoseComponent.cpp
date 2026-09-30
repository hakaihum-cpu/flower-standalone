#include "Realtime3DPoseComponent.h"

#include <array>
#include <cmath>

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

const char* vertexShaderSource = R"GLSL(
#ifdef GL_ES
precision highp float;
precision highp int;
#endif

attribute vec2 position;
varying vec2 vUv;

void main()
{
    vUv = position * 0.5 + 0.5;
    gl_Position = vec4 (position, 0.0, 1.0);
}
)GLSL";

const char* fragmentShaderSource = R"GLSL(
#ifdef GL_ES
precision highp float;
precision highp int;
#endif

varying #highp# vec2 vUv;

uniform #highp# vec2 uResolution;
uniform #highp# float uTime;
uniform #highp# float uState;
uniform #highp# float uPhase;

const float PI = 3.14159265359;

mat2 rot (float a)
{
    float s = sin (a);
    float c = cos (a);
    return mat2 (c, -s, s, c);
}

float hash21 (vec2 p)
{
    p = fract (p * vec2 (123.34, 456.21));
    p += dot (p, p + 45.32);
    return fract (p.x * p.y);
}

float noise2 (vec2 p)
{
    vec2 i = floor (p);
    vec2 f = fract (p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash21 (i);
    float b = hash21 (i + vec2 (1.0, 0.0));
    float c = hash21 (i + vec2 (0.0, 1.0));
    float d = hash21 (i + vec2 (1.0, 1.0));

    return mix (mix (a, b, f.x), mix (c, d, f.x), f.y);
}

float fbm (vec2 p)
{
    float v = 0.0;
    float a = 0.5;

    for (int i = 0; i < 4; ++i)
    {
        v += a * noise2 (p);
        p = rot (0.53) * p * 2.02 + 17.1;
        a *= 0.5;
    }

    return v;
}

float sdSphere (vec3 p, float r)
{
    return length (p) - r;
}

float sdEllipsoid (vec3 p, vec3 r)
{
    float k0 = length (p / r);
    float k1 = length (p / (r * r));
    return k0 * (k0 - 1.0) / max (k1, 0.0001);
}

float sdCapsule (vec3 p, vec3 a, vec3 b, float r)
{
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp (dot (pa, ba) / max (dot (ba, ba), 0.0001), 0.0, 1.0);
    return length (pa - ba * h) - r;
}

float sdRoundBox (vec3 p, vec3 b, float r)
{
    vec3 q = abs (p) - b;
    return length (max (q, 0.0)) + min (max (q.x, max (q.y, q.z)), 0.0) - r;
}

float sdCappedCone (vec3 p, float h, float r1, float r2)
{
    vec2 q = vec2 (length (p.xz), p.y);
    vec2 k1 = vec2 (r2, h);
    vec2 k2 = vec2 (r2 - r1, 2.0 * h);
    vec2 ca = vec2 (q.x - min (q.x, (q.y < 0.0) ? r1 : r2), abs (q.y) - h);
    vec2 cb = q - k1 + k2 * clamp (dot (k1 - q, k2) / max (dot (k2, k2), 0.0001), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt (min (dot (ca, ca), dot (cb, cb)));
}

vec2 opU (vec2 a, vec2 b)
{
    return (a.x < b.x) ? a : b;
}

vec2 addShape (vec2 result, float d, float material)
{
    return opU (result, vec2 (d, material));
}

vec2 mapCharacter (vec3 p)
{
    float state = uState;
    float phase = uPhase;

    if (state > 7.5)
    {
        vec2 r = vec2 (100.0, 0.0);

        vec3 head = vec3 (-0.80, 0.31, 0.03);
        vec3 neck = vec3 (-0.63, 0.31, 0.02);
        vec3 chest = vec3 (-0.40, 0.32, 0.00);
        vec3 pelvis = vec3 (0.02, 0.31, 0.02);

        vec3 kneeL = vec3 (0.36, 0.42, -0.22);
        vec3 ankleL = vec3 (0.62, 0.18, -0.08);
        vec3 kneeR = vec3 (0.31, 0.36, 0.17);
        vec3 ankleR = vec3 (0.56, 0.14, 0.12);

        vec3 shoulderL = vec3 (-0.42, 0.37, -0.18);
        vec3 elbowL = vec3 (-0.62, 0.22, -0.16);
        vec3 handL = vec3 (-0.77, 0.18, -0.08);
        vec3 shoulderR = vec3 (-0.42, 0.37, 0.18);
        vec3 elbowR = vec3 (-0.57, 0.20, 0.18);
        vec3 handR = vec3 (-0.72, 0.17, 0.08);

        r = addShape (r, sdEllipsoid (p - head, vec3 (0.14, 0.17, 0.13)), 1.0);
        r = addShape (r, sdEllipsoid (p - (head + vec3 (0.015, 0.045, 0.055)), vec3 (0.17, 0.20, 0.16)), 3.0);
        r = addShape (r, sdCapsule (p, neck, chest, 0.18), 2.0);
        r = addShape (r, sdCapsule (p, chest, pelvis, 0.22), 2.0);

        r = addShape (r, sdCapsule (p, pelvis + vec3 (-0.11, 0.0, 0.0), kneeL, 0.09), 1.0);
        r = addShape (r, sdCapsule (p, kneeL, ankleL, 0.075), 1.0);
        r = addShape (r, sdCapsule (p, pelvis + vec3 (0.11, 0.0, 0.0), kneeR, 0.09), 1.0);
        r = addShape (r, sdCapsule (p, kneeR, ankleR, 0.075), 1.0);

        r = addShape (r, sdCapsule (p, shoulderL, elbowL, 0.075), 2.0);
        r = addShape (r, sdCapsule (p, elbowL, handL, 0.055), 1.0);
        r = addShape (r, sdCapsule (p, shoulderR, elbowR, 0.075), 2.0);
        r = addShape (r, sdCapsule (p, elbowR, handR, 0.055), 1.0);

        r = addShape (r, sdEllipsoid (p - ankleL - vec3 (-0.05, -0.02, -0.08), vec3 (0.15, 0.065, 0.11)), 3.0);
        r = addShape (r, sdEllipsoid (p - ankleR - vec3 (-0.05, -0.02, -0.08), vec3 (0.15, 0.065, 0.11)), 3.0);

        vec3 bag = vec3 (-0.15, 0.23, 0.25);
        r = addShape (r, sdRoundBox (p - bag, vec3 (0.23, 0.16, 0.08), 0.05), 4.0);
        r = addShape (r, sdCapsule (p, shoulderR, bag + vec3 (0.0, 0.10, 0.0), 0.018), 4.0);

        return r;
    }

    float yaw = 0.0;
    if (state > 2.5 && state < 3.5)
        yaw = PI * smoothstep (0.05, 0.95, phase);
    else if (state > 3.5 && state < 4.5)
        yaw = PI;

    vec3 q = p;
    q.xz = rot (-yaw) * q.xz;

    float bob = 0.0;
    float walkAmount = 0.0;
    float walkWave = sin (phase * 2.0 * PI);

    if (state > 0.5 && state < 1.5)
    {
        walkAmount = 1.0;
        bob = 0.018 * sin (phase * 4.0 * PI);
    }
    else if (state > 1.5 && state < 2.5)
    {
        walkAmount = sin (phase * PI);
        bob = 0.012 * sin (phase * 4.0 * PI) * walkAmount;
    }

    vec3 pelvis = vec3 (0.0, 1.02 + bob, 0.0);
    vec3 chest = vec3 (0.0, 1.43 + bob, 0.0);
    vec3 neck = vec3 (0.0, 1.65 + bob, -0.005);
    vec3 head = vec3 (0.0, 1.82 + bob, -0.018);

    vec3 hipL = pelvis + vec3 (-0.115, 0.00, 0.0);
    vec3 hipR = pelvis + vec3 ( 0.115, 0.00, 0.0);
    vec3 kneeL = vec3 (-0.115, 0.58 + bob, 0.0);
    vec3 kneeR = vec3 ( 0.115, 0.58 + bob, 0.0);
    vec3 ankleL = vec3 (-0.115, 0.13, 0.0);
    vec3 ankleR = vec3 ( 0.115, 0.13, 0.0);

    vec3 shoulderL = chest + vec3 (-0.245, 0.06, 0.0);
    vec3 shoulderR = chest + vec3 ( 0.245, 0.06, 0.0);
    vec3 elbowL = vec3 (-0.275, 1.18 + bob, 0.015);
    vec3 elbowR = vec3 ( 0.275, 1.18 + bob, 0.015);
    vec3 handL = vec3 (-0.225, 0.91 + bob, -0.01);
    vec3 handR = vec3 ( 0.225, 0.91 + bob, -0.01);

    if (walkAmount > 0.001)
    {
        float stepA = walkWave * walkAmount;
        float stepB = -stepA;

        kneeL.z += 0.17 * stepA;
        ankleL.z += 0.30 * stepA;
        kneeR.z += 0.17 * stepB;
        ankleR.z += 0.30 * stepB;

        ankleL.y += 0.055 * max (0.0, -stepA);
        ankleR.y += 0.055 * max (0.0, -stepB);

        elbowL.z -= 0.14 * stepA;
        handL.z -= 0.24 * stepA;
        elbowR.z -= 0.14 * stepB;
        handR.z -= 0.24 * stepB;

        chest.z += 0.012 * sin (phase * 4.0 * PI);
    }

    if (state > 4.5 && state < 5.5)
    {
        pelvis = vec3 (0.0, 0.60, 0.06);
        chest = vec3 (0.0, 1.00, -0.02);
        neck = vec3 (0.0, 1.20, -0.02);
        head = vec3 (0.0, 1.37, -0.04);

        hipL = pelvis + vec3 (-0.12, 0.0, 0.0);
        hipR = pelvis + vec3 ( 0.12, 0.0, 0.0);
        kneeL = vec3 (-0.13, 0.50, -0.38);
        kneeR = vec3 ( 0.13, 0.50, -0.38);
        ankleL = vec3 (-0.13, 0.13, -0.52);
        ankleR = vec3 ( 0.13, 0.13, -0.52);

        shoulderL = chest + vec3 (-0.23, 0.05, 0.0);
        shoulderR = chest + vec3 ( 0.23, 0.05, 0.0);
        elbowL = vec3 (-0.24, 0.76, -0.18);
        elbowR = vec3 ( 0.24, 0.76, -0.18);
        handL = vec3 (-0.16, 0.58, -0.34);
        handR = vec3 ( 0.16, 0.58, -0.34);
    }
    else if (state > 5.5 && state < 6.5)
    {
        pelvis = vec3 (0.0, 0.66, 0.03);
        chest = vec3 (0.0, 1.08, -0.05);
        neck = vec3 (0.0, 1.27, -0.07);
        head = vec3 (0.0, 1.43, -0.09);

        hipL = pelvis + vec3 (-0.12, 0.0, 0.0);
        hipR = pelvis + vec3 ( 0.12, 0.0, 0.0);
        kneeL = vec3 (-0.16, 0.36, -0.18);
        kneeR = vec3 ( 0.16, 0.36, -0.18);
        ankleL = vec3 (-0.18, 0.12, 0.08);
        ankleR = vec3 ( 0.18, 0.12, 0.08);

        shoulderL = chest + vec3 (-0.23, 0.05, 0.0);
        shoulderR = chest + vec3 ( 0.23, 0.05, 0.0);
        elbowL = vec3 (-0.29, 0.80, -0.10);
        elbowR = vec3 ( 0.29, 0.80, -0.10);
        handL = vec3 (-0.20, 0.60, -0.19);
        handR = vec3 ( 0.20, 0.60, -0.19);
    }
    else if (state > 6.5 && state < 7.5)
    {
        pelvis = vec3 (0.0, 0.39, 0.08);
        chest = vec3 (0.0, 0.80, -0.03);
        neck = vec3 (0.0, 0.99, -0.07);
        head = vec3 (0.0, 1.15, -0.10);

        hipL = pelvis + vec3 (-0.12, 0.0, 0.0);
        hipR = pelvis + vec3 ( 0.12, 0.0, 0.0);
        kneeL = vec3 (-0.16, 0.69, -0.31);
        kneeR = vec3 ( 0.16, 0.69, -0.31);
        ankleL = vec3 (-0.16, 0.18, -0.42);
        ankleR = vec3 ( 0.16, 0.18, -0.42);

        shoulderL = chest + vec3 (-0.23, 0.05, 0.0);
        shoulderR = chest + vec3 ( 0.23, 0.05, 0.0);
        elbowL = vec3 (-0.29, 0.68, -0.19);
        elbowR = vec3 ( 0.29, 0.68, -0.19);
        handL = vec3 (-0.18, 0.62, -0.34);
        handR = vec3 ( 0.18, 0.62, -0.34);
    }

    vec2 r = vec2 (100.0, 0.0);

    // Hair volume sits mostly behind the face so the skin remains visible.
    r = addShape (r, sdEllipsoid (q - (head + vec3 (0.0, 0.04, 0.055)), vec3 (0.185, 0.215, 0.155)), 3.0);
    r = addShape (r, sdEllipsoid (q - (head + vec3 (0.0, -0.13, 0.105)), vec3 (0.19, 0.30, 0.10)), 3.0);

    // Face and neck.
    r = addShape (r, sdEllipsoid (q - (head + vec3 (0.0, -0.012, -0.055)), vec3 (0.135, 0.165, 0.115)), 1.0);
    r = addShape (r, sdCapsule (q, neck - vec3 (0.0, 0.04, 0.0), neck + vec3 (0.0, 0.05, 0.0), 0.065), 1.0);

    // Blouse/torso.
    r = addShape (r, sdCapsule (q, chest + vec3 (0.0, 0.10, 0.0), pelvis + vec3 (0.0, 0.08, 0.0), 0.235), 2.0);

    // Dark bow at the collar.
    r = addShape (r, sdRoundBox (q - (neck + vec3 (0.0, -0.10, -0.155)), vec3 (0.095, 0.065, 0.025), 0.025), 3.0);

    // Skirt as a tapered volume following the pelvis.
    vec3 skirtCentre = pelvis + vec3 (0.0, -0.13, 0.0);
    vec3 skirtP = q - skirtCentre;
    r = addShape (r, sdCappedCone (skirtP, 0.22, 0.30, 0.20), 3.0);

    // Legs.
    r = addShape (r, sdCapsule (q, hipL, kneeL, 0.090), 1.0);
    r = addShape (r, sdCapsule (q, kneeL, ankleL, 0.070), 1.0);
    r = addShape (r, sdCapsule (q, hipR, kneeR, 0.090), 1.0);
    r = addShape (r, sdCapsule (q, kneeR, ankleR, 0.070), 1.0);

    // Ankle socks and chunky shoes.
    r = addShape (r, sdCapsule (q, ankleL + vec3 (0.0, 0.08, 0.0), ankleL - vec3 (0.0, 0.015, 0.0), 0.077), 3.0);
    r = addShape (r, sdCapsule (q, ankleR + vec3 (0.0, 0.08, 0.0), ankleR - vec3 (0.0, 0.015, 0.0), 0.077), 3.0);
    r = addShape (r, sdEllipsoid (q - (ankleL + vec3 (0.0, -0.045, -0.085)), vec3 (0.105, 0.060, 0.165)), 3.0);
    r = addShape (r, sdEllipsoid (q - (ankleR + vec3 (0.0, -0.045, -0.085)), vec3 (0.105, 0.060, 0.165)), 3.0);

    // Sleeves, forearms and hands.
    r = addShape (r, sdCapsule (q, shoulderL, elbowL, 0.080), 2.0);
    r = addShape (r, sdCapsule (q, elbowL, handL, 0.055), 1.0);
    r = addShape (r, sdSphere (q - handL, 0.067), 1.0);
    r = addShape (r, sdCapsule (q, shoulderR, elbowR, 0.080), 2.0);
    r = addShape (r, sdCapsule (q, elbowR, handR, 0.055), 1.0);
    r = addShape (r, sdSphere (q - handR, 0.067), 1.0);

    // School bag and strap.
    vec3 bag = pelvis + vec3 (-0.31, 0.08, 0.18);
    r = addShape (r, sdRoundBox (q - bag, vec3 (0.22, 0.25, 0.085), 0.055), 4.0);
    r = addShape (r, sdCapsule (q, shoulderL + vec3 (0.0, 0.0, 0.06), bag + vec3 (0.0, 0.18, 0.0), 0.018), 4.0);

    return r;
}

vec2 mapScene (vec3 p)
{
    vec2 r = mapCharacter (p);

    // Rooftop concrete.
    r = addShape (r, p.y, 5.0);

    // Back parapet.
    r = addShape (r, sdRoundBox (p - vec3 (0.0, 0.38, 3.20), vec3 (4.7, 0.38, 0.13), 0.03), 6.0);

    // Small rooftop utility room.
    r = addShape (r, sdRoundBox (p - vec3 (1.92, 1.05, 2.30), vec3 (0.78, 1.05, 0.72), 0.035), 6.0);
    r = addShape (r, sdRoundBox (p - vec3 (1.92, 0.84, 1.565), vec3 (0.33, 0.70, 0.025), 0.015), 8.0);
    r = addShape (r, sdRoundBox (p - vec3 (2.18, 1.13, 1.525), vec3 (0.025, 0.035, 0.025), 0.01), 7.0);

    // Fence posts and rails.
    vec3 postP = p - vec3 (0.0, 1.10, 2.92);
    postP.x = mod (postP.x + 0.40, 0.80) - 0.40;
    r = addShape (r, sdRoundBox (postP, vec3 (0.022, 0.78, 0.022), 0.006), 7.0);

    r = addShape (r, sdRoundBox (p - vec3 (0.0, 0.72, 2.92), vec3 (4.4, 0.018, 0.018), 0.005), 7.0);
    r = addShape (r, sdRoundBox (p - vec3 (0.0, 1.28, 2.92), vec3 (4.4, 0.018, 0.018), 0.005), 7.0);
    r = addShape (r, sdRoundBox (p - vec3 (0.0, 1.70, 2.92), vec3 (4.4, 0.018, 0.018), 0.005), 7.0);

    return r;
}

vec3 calcNormal (vec3 p)
{
    const float e = 0.0018;
    vec2 h = vec2 (1.0, -1.0) * 0.5773;

    return normalize (
        h.xyy * mapScene (p + h.xyy * e).x +
        h.yyx * mapScene (p + h.yyx * e).x +
        h.yxy * mapScene (p + h.yxy * e).x +
        h.xxx * mapScene (p + h.xxx * e).x);
}

float softShadow (vec3 ro, vec3 rd, float mint, float maxt)
{
    float res = 1.0;
    float t = mint;

    for (int i = 0; i < 18; ++i)
    {
        float h = mapScene (ro + rd * t).x;
        res = min (res, 12.0 * h / max (t, 0.001));
        t += clamp (h, 0.018, 0.22);
        if (res < 0.02 || t > maxt)
            break;
    }

    return clamp (res, 0.0, 1.0);
}

float ambientOcclusion (vec3 p, vec3 n)
{
    float occ = 0.0;
    float weight = 1.0;

    for (int i = 1; i <= 4; ++i)
    {
        float h = 0.045 * float (i);
        float d = mapScene (p + n * h).x;
        occ += (h - d) * weight;
        weight *= 0.55;
    }

    return clamp (1.0 - occ * 2.2, 0.15, 1.0);
}

vec3 materialColour (float m)
{
    if (m < 1.5) return vec3 (0.62, 0.60, 0.58);
    if (m < 2.5) return vec3 (0.82, 0.81, 0.79);
    if (m < 3.5) return vec3 (0.075, 0.073, 0.070);
    if (m < 4.5) return vec3 (0.12, 0.115, 0.105);
    if (m < 5.5) return vec3 (0.43, 0.44, 0.43);
    if (m < 6.5) return vec3 (0.34, 0.35, 0.34);
    if (m < 7.5) return vec3 (0.18, 0.19, 0.19);
    return vec3 (0.095, 0.095, 0.09);
}

vec3 skyColour (vec3 rd)
{
    float h = clamp (rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix (vec3 (0.36, 0.39, 0.41), vec3 (0.66, 0.68, 0.68), h);

    vec2 cp = vec2 (rd.x / max (0.18, rd.y + 0.55), rd.z / max (0.18, rd.y + 0.55));
    float clouds = fbm (cp * 1.6 + vec2 (uTime * 0.005, 0.0));
    clouds = smoothstep (0.48, 0.72, clouds);
    sky = mix (sky, vec3 (0.78), clouds * 0.42);

    return sky;
}

void main()
{
    vec2 frag = vUv * uResolution;
    vec2 uv = (frag * 2.0 - uResolution.xy) / max (uResolution.y, 1.0);

    float state = uState;
    float cameraYaw = -0.08 + 0.035 * sin (uTime * 0.22);

    if (state > 0.5 && state < 2.5)
        cameraYaw = -0.46;
    else if (state > 3.5 && state < 4.5)
        cameraYaw = 0.08;
    else if (state > 7.5)
        cameraYaw = -0.34;

    float cameraRadius = (state > 7.5) ? 4.45 : 4.10;
    vec3 ro = vec3 (sin (cameraYaw) * cameraRadius, 1.46, -cos (cameraYaw) * cameraRadius);
    vec3 ta = (state > 7.5) ? vec3 (-0.05, 0.42, 0.0) : vec3 (0.0, 0.98, 0.0);

    vec3 forward = normalize (ta - ro);
    vec3 right = normalize (cross (forward, vec3 (0.0, 1.0, 0.0)));
    vec3 up = cross (right, forward);
    vec3 rd = normalize (forward + uv.x * right * 0.74 + uv.y * up * 0.74);

    vec3 col = skyColour (rd);

    float t = 0.03;
    float material = -1.0;
    bool hit = false;

    for (int i = 0; i < 84; ++i)
    {
        vec3 p = ro + rd * t;
        vec2 scene = mapScene (p);
        float d = scene.x;

        if (d < 0.0017)
        {
            hit = true;
            material = scene.y;
            break;
        }

        t += d * 0.78;

        if (t > 12.0)
            break;
    }

    if (hit)
    {
        vec3 p = ro + rd * t;
        vec3 n = calcNormal (p);

        vec3 lightDir = normalize (vec3 (-0.45, 0.78, -0.38));
        float diffuse = max (dot (n, lightDir), 0.0);
        float shadow = softShadow (p + n * 0.008, lightDir, 0.025, 6.0);
        float ao = ambientOcclusion (p, n);

        vec3 base = materialColour (material);
        float ambient = 0.30 + 0.16 * max (n.y, 0.0);
        float key = diffuse * shadow * 0.86;

        vec3 halfVector = normalize (lightDir - rd);
        float specPower = (material < 2.5) ? 34.0 : 18.0;
        float spec = pow (max (dot (n, halfVector), 0.0), specPower);
        spec *= shadow * ((material < 4.5) ? 0.18 : 0.10);

        float rim = pow (1.0 - max (dot (n, -rd), 0.0), 3.0) * 0.12;

        col = base * (ambient + key) * ao;
        col += vec3 (spec + rim);

        // Concrete micro-variation keeps the rooftop from looking flat.
        if (material > 4.5 && material < 6.5)
        {
            float grit = hash21 (floor (p.xz * 145.0));
            col *= 0.92 + grit * 0.14;
        }

        float fog = 1.0 - exp (-0.018 * t * t);
        col = mix (col, skyColour (rd) * 0.82, fog);
    }

    // Directional haze and highlight bloom approximation.
    vec3 sunDir = normalize (vec3 (0.45, 0.52, 0.72));
    float sunGlow = pow (max (dot (rd, sunDir), 0.0), 48.0);
    col += vec3 (0.12) * sunGlow;

    // Near-monochrome late-1990s/early-2000s game-event treatment.
    float luma = dot (col, vec3 (0.299, 0.587, 0.114));
    col = mix (col, vec3 (luma), 0.88);
    col *= vec3 (0.98, 1.0, 1.015);

    // Gentle contrast curve.
    col = clamp ((col - 0.5) * 1.10 + 0.5, 0.0, 1.0);

    // Film grain + subtle scan structure.
    float grain = hash21 (frag + vec2 (fract (uTime) * 911.7, floor (uTime * 24.0)));
    col += (grain - 0.5) * 0.035;

    float scan = sin (frag.y * 1.55) * 0.007;
    col -= scan;

    float vignette = 1.0 - 0.28 * dot (uv * 0.68, uv * 0.68);
    col *= clamp (vignette, 0.66, 1.0);

    gl_FragColor = vec4 (clamp (col, 0.0, 1.0), 1.0);
}
)GLSL";
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
    return source.replace ("#lowp#", juce::OpenGLHelpers::isOpenGLES() ? "lowp" : "")
                 .replace ("#mediump#", juce::OpenGLHelpers::isOpenGLES() ? "mediump" : "")
                 .replace ("#highp#", juce::OpenGLHelpers::isOpenGLES() ? "highp" : "");
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
            "FLOWER realtime-3D shader error: " + candidate->getLastError());
        return;
    }

    shader = std::move (candidate);
    shader->use();

    positionAttribute = std::make_unique<juce::OpenGLShaderProgram::Attribute> (
        *shader, "position");
    resolutionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (
        *shader, "uResolution");
    timeUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (
        *shader, "uTime");
    stateUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (
        *shader, "uState");
    phaseUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (
        *shader, "uPhase");

    const float fullscreenTriangle[]
    {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f
    };

    glGenBuffers (1, &fullscreenVbo);
    glBindBuffer (GL_ARRAY_BUFFER, fullscreenVbo);
    glBufferData (GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr> (sizeof (fullscreenTriangle)),
                  fullscreenTriangle,
                  GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    startMs = juce::Time::getMillisecondCounterHiRes();
    fpsWindowStartMs = startMs;
    fpsFrameCount = 0;

    shaderReady.store (true, std::memory_order_release);
}

void Realtime3DPoseComponent::shutdown()
{
    using namespace ::juce::gl;

    shaderReady.store (false, std::memory_order_release);

    phaseUniform.reset();
    stateUniform.reset();
    timeUniform.reset();
    resolutionUniform.reset();
    positionAttribute.reset();
    shader.reset();

    if (fullscreenVbo != 0)
    {
        glDeleteBuffers (1, &fullscreenVbo);
        fullscreenVbo = 0;
    }
}

void Realtime3DPoseComponent::updatePoseState (double elapsedSeconds)
{
    static constexpr std::array<double, poseCount> durations
    {
        3.0, // idle/front
        6.0, // walk cycle
        3.0, // walk start/stop
        4.0, // turn
        3.0, // back
        4.0, // sit
        4.0, // crouch
        4.0, // knees up
        5.0  // lie down
    };

    double cycle = 0.0;
    for (const auto duration : durations)
        cycle += duration;

    double cursor = std::fmod (elapsedSeconds, cycle);
    int index = 0;

    while (index < poseCount - 1 && cursor >= durations[static_cast<size_t> (index)])
    {
        cursor -= durations[static_cast<size_t> (index)];
        ++index;
    }

    const float phase = static_cast<float> (
        cursor / durations[static_cast<size_t> (index)]);

    currentPose.store (index, std::memory_order_relaxed);
    currentPhase.store (juce::jlimit (0.0f, 1.0f, phase), std::memory_order_relaxed);
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

    const float scale = static_cast<float> (openGLContext.getRenderingScale());
    const int pixelWidth = juce::jmax (1, juce::roundToInt (scale * static_cast<float> (bounds.getWidth())));
    const int pixelHeight = juce::jmax (1, juce::roundToInt (scale * static_cast<float> (bounds.getHeight())));

    glViewport (0, 0, pixelWidth, pixelHeight);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_BLEND);

    juce::OpenGLHelpers::clear (juce::Colour (0xff111214));

    if (! shaderReady.load (std::memory_order_acquire)
        || shader == nullptr
        || positionAttribute == nullptr
        || fullscreenVbo == 0)
        return;

    shader->use();

    resolutionUniform->set (
        static_cast<float> (pixelWidth),
        static_cast<float> (pixelHeight));
    timeUniform->set (static_cast<float> (elapsedSeconds));
    stateUniform->set (static_cast<float> (currentPose.load (std::memory_order_relaxed)));
    phaseUniform->set (currentPhase.load (std::memory_order_relaxed));

    glBindBuffer (GL_ARRAY_BUFFER, fullscreenVbo);
    glVertexAttribPointer (
        positionAttribute->attributeID,
        2,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei> (2 * sizeof (float)),
        nullptr);
    glEnableVertexAttribArray (positionAttribute->attributeID);

    glDrawArrays (GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray (positionAttribute->attributeID);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
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
    g.drawText ("FLOWER / REALTIME 3D QUALITY PROTOTYPE",
                top.removeFromTop (21),
                juce::Justification::centredLeft,
                false);

    g.setFont (juce::FontOptions (12.0f));
    const auto fps = measuredFps.load (std::memory_order_relaxed);
    g.drawText (
        juce::String (poseNames[pose])
            + "    FPS " + juce::String (fps, 1)
            + "    HQ SDF / SOFT SHADOW / AO / FOG / FILM",
        top,
        juce::Justification::centredLeft,
        false);

    if (! shaderReady.load (std::memory_order_acquire))
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
        g.drawFittedText (
            "3D SHADER INITIALISING / ERROR",
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
