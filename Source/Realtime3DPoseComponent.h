#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

class Realtime3DPoseComponent final : public juce::OpenGLAppComponent,
                                      private juce::Timer
{
public:
    Realtime3DPoseComponent();
    ~Realtime3DPoseComponent() override;

    void initialise() override;
    void shutdown() override;
    void render() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Mesh;

    static juce::String preprocessShader (juce::String source);
    void timerCallback() override;
    void updatePoseState (double elapsedSeconds);

    void createMeshes();
    void destroyMeshes();
    void renderScene (float elapsedSeconds, int pose, float phase);
    void drawMesh (const Mesh& mesh,
                   const juce::Matrix3D<float>& model,
                   juce::Colour colour,
                   float alpha = 1.0f,
                   float material = 0.0f);

    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<Mesh> sphereMesh;
    std::unique_ptr<Mesh> cylinderMesh;
    std::unique_ptr<Mesh> boxMesh;
    std::unique_ptr<Mesh> skirtMesh;
    std::unique_ptr<Mesh> discMesh;

    int positionAttribute = -1;
    int normalAttribute = -1;

    juce::Matrix3D<float> projectionMatrix;
    juce::Matrix3D<float> viewMatrix;

    juce::CriticalSection boundsLock;
    juce::Rectangle<int> renderBounds;

    std::atomic<float> measuredFps { 0.0f };
    std::atomic<int> currentPose { 0 };
    std::atomic<float> currentPhase { 0.0f };
    std::atomic<bool> shaderReady { false };

    double startMs = 0.0;
    double fpsWindowStartMs = 0.0;
    int fpsFrameCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Realtime3DPoseComponent)
};
