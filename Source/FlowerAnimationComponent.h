#pragma once

#include <JuceHeader.h>
#include <array>

class FlowerAnimationComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    static constexpr int studentCount = 8;
    static constexpr int walkFrameCount = 8;

    FlowerAnimationComponent();
    ~FlowerAnimationComponent() override;

    bool loadDirectory (const juce::File& directory);
    bool loadEmbeddedAtlas (const void* data, size_t size);
    bool loadDecodedAtlas (const juce::Image& atlas);
    bool loadHighResWalkStrip (const void* data, size_t size, int studentIndex, bool walksRight);
    bool loadDecodedHighResWalkStrip (const juce::Image& strip, int studentIndex, bool walksRight);
    bool loadHighResStand (const void* data, size_t size, int studentIndex);
    bool hasCompleteHighResActorCore() const noexcept;
    bool hasVisualBank() const noexcept
    {
        const bool legacyActorBankReady = loadedStudentCount == studentCount && poseCoverageReady;
        return background.isValid() && (hasCompleteHighResActorCore() || legacyActorBankReady);
    }

    void setState (float density, float position, float spread, float hold, float size,
                   float pitch, float mix, float feedback,
                   bool reverse, bool recording, float recordProgress, bool loopActive);
    void paint (juce::Graphics& g) override;

private:
    enum class Pose : int
    {
        Stand,
        HandsBehind,
        ArmsFolded,
        OneKnee,
        TurnLeft,
        TurnRight,
        LookBack,
        LookDown,
        LookUp,
        Rail,
        LeanWall,
        Crouch,
        SitFloor,
        SitKneesUp,
        SitLeanBack,
        HalfSquat,
        TalkLeft,
        TalkRight,
        TalkClose,
        Listen,
        Whisper,
        HoldLeft,
        HoldRight,
        HandOnShoulder,
        ReachToward,
        Point,
        PullHand,
        PushReady,
        PushContact,
        PushRecoil,
        StepIn,
        StepOut,
        WalkLeft,
        WalkRight,
        TurnInPlace,
        JumpLow,
        JumpHigh,
        Land,
        Stumble,
        FallBack,
        KneelDown,
        RiseFromFloor,
        LevitateLow,
        AscendHigh,
        Suspended,
        AscendArmsLoose,
        AscendReaching,
        BodyTiltUnnatural,
        ArmsHeldOddly,
        HeadTurnBodyStill,
        FreezeMidStep,
        FreezeMidJump,
        HangingPose,
        MistDisperseStart,
        MistDisperseMid,
        MistDisperseEnd,
        ReformStart,
        ReformMid,
        Count
    };

    enum class SceneType : int
    {
        Empty,
        Line,
        Spread,
        Cluster,
        HoldHands,
        Conversation,
        SitGroup,
        SingleMove,
        GroupJump,
        StaggeredJump,
        AscendSingle,
        AscendGroup,
        Suspended,
        DisperseSingle,
        DisperseGroup,
        Reform,
        PushTension,
        PushContact,
        FallOrSit,
        OddPose,
        Freeze,
        Appear,
        Disappear,
        LookBack,
        MixedTableau
    };

    enum class ActionKind : int
    {
        None,
        Step,
        Jump,
        Ascend,
        Disperse,
        Reform,
        Fall
    };

    enum class ActorVariation : int
    {
        None,
        Fade,
        Disperse,
        Overlap,
        UpperBodyWrong,
        Distant,
        Silhouette,
        Invert,
        Noise,
        Freeze
    };

    struct StudentAsset
    {
        juce::String id;
        std::array<juce::Image, static_cast<size_t> (Pose::Count)> poses {};

        bool has (Pose pose) const noexcept
        {
            return poses[static_cast<size_t> (pose)].isValid();
        }

        const juce::Image& imageFor (Pose preferred) const noexcept
        {
            const auto& chosen = poses[static_cast<size_t> (preferred)];
            if (chosen.isValid())
                return chosen;

            return poses[static_cast<size_t> (Pose::Stand)];
        }
    };

    struct StudentState
    {
        bool visible = false;
        bool targetVisible = false;
        bool exiting = false;
        bool sceneLocked = false;
        int slot = 0;
        Pose pose = Pose::Stand;
        Pose basePose = Pose::Stand;
        Pose ambientPose = Pose::Stand;
        Pose restPose = Pose::Stand;
        ActionKind action = ActionKind::None;
        ActorVariation variation = ActorVariation::None;
        int variationTicks = 0;
        int variationTotalTicks = 0;
        float variationSeed = 0.0f;
        int delayTicks = 0;
        int phaseTicks = 0;
        int phaseOffset = 0;
        int ambientTicks = 0;
        int decisionTicks = 0;
        int relation = -1;
        int motionDirection = 1;
        float xOffset = 0.0f;
        float ambientXOffset = 0.0f;
        float currentX = 0.5f;
        float targetX = 0.5f;
        float previousTargetX = 0.5f;
        float moveRate = 0.01f;
        float heightScale = 1.0f;
        float floorOffset = 0.0f;
    };

    static const char* poseFileName (Pose pose) noexcept;
    bool loadStudent (StudentAsset& student, const juce::File& directory, int index);
    bool validatePoseCoverage() const noexcept;
    int countPoses (const StudentAsset& student) const noexcept;

    int densityToCount (float density) const noexcept;
    void timerCallback() override;
    void advanceActors();
    void reconcileActorPopulation();
    void assignActorGoal (int index, bool entering);
    Pose chooseActorRestPose (int index);
    ActorVariation chooseActorVariation() noexcept;
    void maybeStartActorVariation (StudentState& state);
    int currentTargetActorCount() const noexcept;
    SceneType sanitiseSceneForCount (SceneType scene, int count) const noexcept;
    SceneType chooseNextScene();
    Pose poseForStudent (int visibleIndex, int count) const noexcept;
    float verticalOffsetForPose (Pose pose, int visibleIndex) const noexcept;
    void resetStudentStates();
    void configureScene (SceneType nextScene);
    void updateAmbientBehaviours();
    void updateStudentActions();
    void advanceSceneIfNeeded();
    void updateVisibleCount();
    void drawStudent (juce::Graphics& g,
                      const StudentAsset& student,
                      Pose pose,
                      float centreX,
                      float baselineY,
                      float targetHeight) const;
    void drawActorImage (juce::Graphics& g,
                         const juce::Image& image,
                         float centreX,
                         float baselineY,
                         float targetHeight,
                         float opacity = 1.0f) const;
    void drawActorVariation (juce::Graphics& g,
                             const juce::Image& image,
                             const StudentState& state,
                             int actorIndex,
                             float centreX,
                             float baselineY,
                             float targetHeight,
                             juce::Rectangle<float> stage) const;
    juce::Image makeActorEffectImage (const juce::Image& source,
                                      ActorVariation variation,
                                      int actorIndex) const;
    juce::Image makeHighResIdentityVariant (const juce::Image& source,
                                            int actorIndex) const;
    void deriveHighResActorCoreFromPrimary();
    void drawHighResPoseVariation (juce::Graphics& g,
                                   const juce::Image& image,
                                   const StudentState& state,
                                   Pose pose,
                                   int actorIndex,
                                   float centreX,
                                   float baselineY,
                                   float targetHeight,
                                   juce::Rectangle<float> stage) const;

    juce::Image background;
    std::array<StudentAsset, studentCount> students {};
    std::array<StudentState, studentCount> studentStates {};
    std::array<std::array<juce::Image, walkFrameCount>, studentCount> highResWalkRight {};
    std::array<std::array<juce::Image, walkFrameCount>, studentCount> highResWalkLeft {};
    std::array<juce::Image, studentCount> highResStand {};
    std::array<bool, studentCount> highResWalkRightReady {};
    std::array<bool, studentCount> highResWalkLeftReady {};
    std::array<bool, studentCount> highResStandReady {};
    std::array<int, 3> sceneActors { -1, -1, -1 };
    juce::Random random;
    int loadedStudentCount = 0;
    bool poseCoverageReady = false;

    SceneType scene = SceneType::Empty;
    SceneType previousScene = SceneType::Empty;
    int sceneTicks = 0;
    int tick = 0;
    int visibleCount = 0;
    int previousVisibleCount = 0;
    int sceneActorCount = 0;
    int lastPrimaryActor = -1;
    int completionFreezeTicks = 0;
    int actorTick = 0;
    int populationCooldownTicks = 0;
    int populationDecisionTicks = 0;
    int actorPopulationTarget = 0;

    float densityValue = 0.0f;
    float positionValue = 0.5f;
    float spreadValue = 0.0f;
    float holdValue = 0.0f;
    float sizeValue = 0.08f;
    float pitchValue = 0.0f;
    float mixValue = 0.0f;
    float feedbackValue = 0.0f;
    float recordProgressValue = 0.0f;
    bool reverseValue = false;
    bool recordingValue = false;
    bool loopActiveValue = false;
    bool previousRecordingValue = false;
    bool previousLoopActiveValue = false;
};
