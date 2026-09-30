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
    static juce::String preprocessShader (juce::String source);
    void timerCallback() override;
    void updatePoseState (double elapsedSeconds);

    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> positionAttribute;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> resolutionUniform;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> timeUniform;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> stateUniform;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> phaseUniform;

    juce::gl::GLuint fullscreenVbo = 0;

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
