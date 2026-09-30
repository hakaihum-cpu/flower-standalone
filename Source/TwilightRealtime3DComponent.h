#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>

class TwilightRealtime3DComponent final : public juce::OpenGLAppComponent,
                                          private juce::Timer
{
public:
    struct Mesh;

    TwilightRealtime3DComponent();
    ~TwilightRealtime3DComponent() override;

    void initialise() override;
    void shutdown() override;
    void render() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Pose;
    struct RigMatrices;

    void timerCallback() override;
    void createMeshes();
    void destroyMeshes();

    Pose evaluatePose (double elapsedSeconds) const;
    RigMatrices buildRig (const Pose& pose) const;

    void renderScene (const Pose& pose, float elapsedSeconds);
    void renderCharacter (const Pose& pose, const RigMatrices& rig);
    void renderEnvironment (float elapsedSeconds);

    void drawMesh (const Mesh& mesh,
                   const juce::Matrix3D<float>& model,
                   juce::Colour colour,
                   float material = 0.0f,
                   float alpha = 1.0f);

    std::unique_ptr<juce::OpenGLShaderProgram> shader;

    std::unique_ptr<Mesh> sphereMesh;
    std::unique_ptr<Mesh> segmentMesh;
    std::unique_ptr<Mesh> torsoMesh;
    std::unique_ptr<Mesh> skirtMesh;
    std::unique_ptr<Mesh> boxMesh;
    std::unique_ptr<Mesh> shoeMesh;
    std::unique_ptr<Mesh> hairBackMesh;
    std::unique_ptr<Mesh> quadMesh;

    int positionAttribute = -1;
    int normalAttribute = -1;

    juce::Matrix3D<float> projectionMatrix;
    juce::Matrix3D<float> viewMatrix;

    juce::CriticalSection boundsLock;
    juce::Rectangle<int> renderBounds;

    std::atomic<float> measuredFps { 0.0f };
    std::atomic<int> currentScene { 0 };
    std::atomic<int> currentKey { 0 };
    std::atomic<bool> shaderReady { false };

    double startMs = 0.0;
    double fpsWindowStartMs = 0.0;
    int fpsFrameCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwilightRealtime3DComponent)
};
