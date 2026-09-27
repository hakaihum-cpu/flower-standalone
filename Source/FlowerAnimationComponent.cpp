#include "FlowerAnimationComponent.h"
#include <cmath>

const char* FlowerAnimationComponent::poseFileName (Pose pose) noexcept
{
    switch (pose)
    {
        case Pose::Stand:              return "stand.png";
        case Pose::HandsBehind:        return "hands_behind.png";
        case Pose::ArmsFolded:         return "arms_folded.png";
        case Pose::OneKnee:            return "one_knee.png";
        case Pose::TurnLeft:           return "turn_left.png";
        case Pose::TurnRight:          return "turn_right.png";
        case Pose::LookBack:           return "look_back.png";
        case Pose::LookDown:           return "look_down.png";
        case Pose::LookUp:             return "look_up.png";
        case Pose::Rail:               return "rail.png";
        case Pose::LeanWall:           return "lean_wall.png";
        case Pose::Crouch:             return "crouch.png";
        case Pose::SitFloor:           return "sit_floor.png";
        case Pose::SitKneesUp:         return "sit_knees_up.png";
        case Pose::SitLeanBack:        return "sit_lean_back.png";
        case Pose::HalfSquat:          return "half_squat.png";
        case Pose::TalkLeft:           return "talk_left.png";
        case Pose::TalkRight:          return "talk_right.png";
        case Pose::TalkClose:          return "talk_close.png";
        case Pose::Listen:             return "listen.png";
        case Pose::Whisper:            return "whisper.png";
        case Pose::HoldLeft:           return "hold_left.png";
        case Pose::HoldRight:          return "hold_right.png";
        case Pose::HandOnShoulder:     return "hand_on_shoulder.png";
        case Pose::ReachToward:        return "reach_toward.png";
        case Pose::Point:              return "point.png";
        case Pose::PullHand:           return "pull_hand.png";
        case Pose::PushReady:          return "push_ready.png";
        case Pose::PushContact:        return "push_contact.png";
        case Pose::PushRecoil:         return "push_recoil.png";
        case Pose::StepIn:             return "step_in.png";
        case Pose::StepOut:            return "step_out.png";
        case Pose::WalkLeft:           return "walk_left.png";
        case Pose::WalkRight:          return "walk_right.png";
        case Pose::TurnInPlace:        return "turn_in_place.png";
        case Pose::JumpLow:            return "jump_low.png";
        case Pose::JumpHigh:           return "jump_high.png";
        case Pose::Land:               return "land.png";
        case Pose::Stumble:            return "stumble.png";
        case Pose::FallBack:           return "fall_back.png";
        case Pose::KneelDown:          return "kneel_down.png";
        case Pose::RiseFromFloor:      return "rise_from_floor.png";
        case Pose::LevitateLow:        return "levitate_low.png";
        case Pose::AscendHigh:         return "ascend_high.png";
        case Pose::Suspended:          return "suspended.png";
        case Pose::AscendArmsLoose:    return "ascend_arms_loose.png";
        case Pose::AscendReaching:     return "ascend_reaching.png";
        case Pose::BodyTiltUnnatural:  return "body_tilt_unnatural.png";
        case Pose::ArmsHeldOddly:      return "arms_held_oddly.png";
        case Pose::HeadTurnBodyStill:  return "head_turn_body_still.png";
        case Pose::FreezeMidStep:      return "freeze_mid_step.png";
        case Pose::FreezeMidJump:      return "freeze_mid_jump.png";
        case Pose::HangingPose:        return "hanging_pose.png";
        case Pose::MistDisperseStart:  return "mist_disperse_start.png";
        case Pose::MistDisperseMid:    return "mist_disperse_mid.png";
        case Pose::MistDisperseEnd:    return "mist_disperse_end.png";
        case Pose::ReformStart:        return "reform_start.png";
        case Pose::ReformMid:          return "reform_mid.png";
        case Pose::Count:              break;
    }

    return "stand.png";
}

bool FlowerAnimationComponent::loadStudent (StudentAsset& student,
                                             const juce::File& studentsDirectory,
                                             int index)
{
    student = {};
    student.id = juce::String::formatted ("student_%02d", index + 1);
    const auto directory = studentsDirectory.getChildFile (student.id);

    if (! directory.isDirectory())
        return false;

    for (int poseIndex = 0; poseIndex < static_cast<int> (Pose::Count); ++poseIndex)
    {
        const auto pose = static_cast<Pose> (poseIndex);
        const auto file = directory.getChildFile (poseFileName (pose));

        if (! file.existsAsFile())
            continue;

        auto image = juce::ImageFileFormat::loadFrom (file);
        if (! image.isValid())
            continue;

       #if JUCE_ANDROID
        constexpr int maxHeight = 900;
        if (image.getHeight() > maxHeight)
        {
            const float scale = static_cast<float> (maxHeight) / static_cast<float> (image.getHeight());
            const int width = juce::jmax (1, juce::roundToInt (static_cast<float> (image.getWidth()) * scale));
            image = image.rescaled (width, maxHeight, juce::Graphics::mediumResamplingQuality);
        }
       #endif

        student.poses[static_cast<size_t> (pose)] = image;
    }

    return student.has (Pose::Stand);
}

int FlowerAnimationComponent::countPoses (const StudentAsset& student) const noexcept
{
    int count = 0;
    for (int poseIndex = 0; poseIndex < static_cast<int> (Pose::Count); ++poseIndex)
        if (student.has (static_cast<Pose> (poseIndex)))
            ++count;
    return count;
}

bool FlowerAnimationComponent::validatePoseCoverage() const noexcept
{
    if (loadedStudentCount != studentCount)
        return false;

    // Legacy pose coverage remains available only as a compatibility fallback.
    // Actor v3 high-resolution assets are loaded separately and are enabled
    // only when the complete per-student/direction bank is present.
    for (const auto& student : students)
        if (! student.has (Pose::Stand))
            return false;

    return true;
}

bool FlowerAnimationComponent::loadEmbeddedAtlas (const void* data, size_t size)
{
    background = {};
    students = {};
    loadedStudentCount = 0;
    poseCoverageReady = false;
    resetStudentStates();

    // Reject malformed embedded PNG data before it reaches the platform decoder.
    if (data == nullptr || size < 24)
    {
        repaint();
        return false;
    }

    const auto* bytes = static_cast<const unsigned char*> (data);
    static constexpr unsigned char pngSignature[8] { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
    for (int i = 0; i < 8; ++i)
        if (bytes[i] != pngSignature[i])
        {
            repaint();
            return false;
        }

    const auto readBE32 = [] (const unsigned char* p) noexcept
    {
        return (static_cast<unsigned int> (p[0]) << 24)
             | (static_cast<unsigned int> (p[1]) << 16)
             | (static_cast<unsigned int> (p[2]) << 8)
             |  static_cast<unsigned int> (p[3]);
    };

    const auto encodedWidth = readBE32 (bytes + 16);
    const auto encodedHeight = readBE32 (bytes + 20);
    const bool legacyAtlas = encodedWidth == 256u && encodedHeight == 624u;
    const bool recoveredAtlas = encodedWidth == 256u && encodedHeight == 1584u;
    if (! legacyAtlas && ! recoveredAtlas)
    {
        repaint();
        return false;
    }

    auto atlas = juce::ImageFileFormat::loadFrom (data, size);
    if (! atlas.isValid())
    {
        repaint();
        return false;
    }

    const int atlasRows = recoveredAtlas ? 24 : 8;
    const int atlasWidth = atlas.getWidth();
    const int atlasHeight = atlas.getHeight();
    const int backgroundHeight = juce::roundToInt (static_cast<float> (atlasWidth) * 9.0f / 16.0f);
    const int cellWidth = atlasWidth / studentCount;
    const int cellHeight = (atlasHeight - backgroundHeight) / atlasRows;

    if (atlasWidth < studentCount || backgroundHeight <= 0 || cellWidth <= 0 || cellHeight <= 0
        || (atlasHeight - backgroundHeight) % atlasRows != 0
        || backgroundHeight + cellHeight * atlasRows > atlasHeight)
    {
        repaint();
        return false;
    }

    background = atlas.getClippedImage ({ 0, 0, atlasWidth, backgroundHeight });

    // Recovered 24-row mapping is derived from the production pose bank by
    // image hash. Keep the legacy 8-row mapping as a safe fallback until the
    // recovered atlas is present on every target.
    const auto rowForPose = [atlasRows] (Pose pose) noexcept
    {
        if (atlasRows == 8)
        {
            switch (pose)
            {
                case Pose::StepIn:
                case Pose::StepOut:
                case Pose::WalkLeft:
                case Pose::WalkRight:
                case Pose::TurnInPlace:
                    return 1;

                case Pose::Stumble:
                case Pose::Land:
                case Pose::RiseFromFloor:
                    return 2;

                case Pose::SitFloor:
                case Pose::SitKneesUp:
                case Pose::SitLeanBack:
                case Pose::KneelDown:
                    return 3;

                case Pose::Crouch:
                case Pose::HalfSquat:
                case Pose::FallBack:
                case Pose::FreezeMidStep:
                    return 4;

                case Pose::TalkLeft:
                case Pose::TalkRight:
                case Pose::TalkClose:
                case Pose::Listen:
                case Pose::Whisper:
                case Pose::HoldLeft:
                case Pose::HoldRight:
                case Pose::HandOnShoulder:
                case Pose::ReachToward:
                case Pose::Point:
                case Pose::PullHand:
                case Pose::PushReady:
                case Pose::PushContact:
                case Pose::PushRecoil:
                    return 5;

                case Pose::JumpLow:
                case Pose::JumpHigh:
                case Pose::LevitateLow:
                case Pose::AscendHigh:
                case Pose::Suspended:
                case Pose::AscendArmsLoose:
                case Pose::AscendReaching:
                case Pose::BodyTiltUnnatural:
                case Pose::ArmsHeldOddly:
                case Pose::FreezeMidJump:
                case Pose::HangingPose:
                    return 6;

                case Pose::MistDisperseStart:
                case Pose::MistDisperseMid:
                case Pose::MistDisperseEnd:
                case Pose::ReformStart:
                case Pose::ReformMid:
                    return 7;

                default:
                    return 0;
            }
        }

        switch (pose)
        {
            case Pose::ArmsFolded:
            case Pose::LeanWall:
            case Pose::Listen:
            case Pose::LookUp:
            case Pose::TalkRight:
            case Pose::TurnRight:
                return 0;

            case Pose::ArmsHeldOddly:
                return 1;

            case Pose::AscendArmsLoose:
            case Pose::AscendHigh:
            case Pose::FreezeMidJump:
            case Pose::JumpLow:
            case Pose::LevitateLow:
                return 2;

            case Pose::AscendReaching:
            case Pose::HangingPose:
            case Pose::JumpHigh:
            case Pose::Suspended:
                return 3;

            case Pose::BodyTiltUnnatural:
            case Pose::HeadTurnBodyStill:
                return 4;

            case Pose::Crouch:
            case Pose::KneelDown:
            case Pose::Land:
            case Pose::OneKnee:
                return 5;

            case Pose::FallBack:
                return 6;

            case Pose::FreezeMidStep:
            case Pose::PushContact:
            case Pose::PushReady:
            case Pose::Stumble:
            case Pose::TurnInPlace:
                return 7;

            case Pose::HalfSquat:
                return 8;

            case Pose::HandOnShoulder:
            case Pose::HandsBehind:
            case Pose::LookBack:
            case Pose::LookDown:
            case Pose::Point:
            case Pose::Rail:
            case Pose::TalkClose:
            case Pose::TalkLeft:
            case Pose::TurnLeft:
            case Pose::Whisper:
                return 9;

            case Pose::HoldLeft:
            case Pose::PullHand:
                return 10;

            case Pose::HoldRight:
            case Pose::ReachToward:
                return 11;

            case Pose::MistDisperseEnd:
                return 12;

            case Pose::MistDisperseMid:
                return 13;

            case Pose::MistDisperseStart:
                return 14;

            case Pose::PushRecoil:
                return 15;

            case Pose::ReformMid:
                return 16;

            case Pose::ReformStart:
                return 17;

            case Pose::RiseFromFloor:
                return 18;

            case Pose::SitFloor:
            case Pose::SitLeanBack:
                return 19;

            case Pose::SitKneesUp:
                return 20;

            case Pose::Stand:
                return 21;

            case Pose::StepIn:
            case Pose::WalkLeft:
                return 22;

            case Pose::StepOut:
            case Pose::WalkRight:
                return 23;

            default:
                return 21;
        }
    };

    for (int studentIndex = 0; studentIndex < studentCount; ++studentIndex)
    {
        auto& student = students[static_cast<size_t> (studentIndex)];
        student = {};
        student.id = juce::String::formatted ("student_%02d", studentIndex + 1);

        std::array<juce::Image, 24> rowImages {};
        for (int row = 0; row < atlasRows; ++row)
        {
            const juce::Rectangle<int> source (studentIndex * cellWidth,
                                               backgroundHeight + row * cellHeight,
                                               cellWidth,
                                               cellHeight);
            rowImages[static_cast<size_t> (row)] = atlas.getClippedImage (source);
        }

        for (int poseIndex = 0; poseIndex < static_cast<int> (Pose::Count); ++poseIndex)
        {
            const auto pose = static_cast<Pose> (poseIndex);
            const int row = rowForPose (pose);
            student.poses[static_cast<size_t> (pose)] = rowImages[static_cast<size_t> (row)];
        }

        if (student.has (Pose::Stand))
            ++loadedStudentCount;
    }

    poseCoverageReady = validatePoseCoverage();
    repaint();
    return hasVisualBank();
}

bool FlowerAnimationComponent::loadHighResWalkStrip (const void* data,
                                                              size_t size,
                                                              int studentIndex,
                                                              bool walksRight)
{
    if (studentIndex < 0 || studentIndex >= studentCount || data == nullptr || size == 0)
        return false;

    auto strip = juce::ImageFileFormat::loadFrom (data, size);
    if (! strip.isValid() || strip.getWidth() % walkFrameCount != 0)
        return false;

    const int frameWidth = strip.getWidth() / walkFrameCount;
    const int frameHeight = strip.getHeight();
    if (frameWidth <= 0 || frameHeight <= 0)
        return false;

    auto& bank = walksRight
               ? highResWalkRight[static_cast<size_t> (studentIndex)]
               : highResWalkLeft[static_cast<size_t> (studentIndex)];
    auto& ready = walksRight
                ? highResWalkRightReady[static_cast<size_t> (studentIndex)]
                : highResWalkLeftReady[static_cast<size_t> (studentIndex)];

    for (int frame = 0; frame < walkFrameCount; ++frame)
    {
        bank[static_cast<size_t> (frame)] =
            strip.getClippedImage ({ frame * frameWidth, 0, frameWidth, frameHeight });

        if (! bank[static_cast<size_t> (frame)].isValid())
        {
            ready = false;
            return false;
        }
    }

    ready = true;

    // The approved direction QA uses an exact geometric mirror for the
    // opposite travel direction. Derive the missing opposite bank here so
    // identity, proportions and image quality remain exactly consistent.
    auto& oppositeBank = walksRight
                       ? highResWalkLeft[static_cast<size_t> (studentIndex)]
                       : highResWalkRight[static_cast<size_t> (studentIndex)];
    auto& oppositeReady = walksRight
                        ? highResWalkLeftReady[static_cast<size_t> (studentIndex)]
                        : highResWalkRightReady[static_cast<size_t> (studentIndex)];

    if (! oppositeReady)
    {
        for (int frame = 0; frame < walkFrameCount; ++frame)
        {
            const auto& source = bank[static_cast<size_t> (frame)];
            juce::Image mirrored (juce::Image::ARGB,
                                  source.getWidth(),
                                  source.getHeight(),
                                  true);
            juce::Graphics mg (mirrored);
            mg.addTransform (juce::AffineTransform (-1.0f, 0.0f,
                                                     static_cast<float> (source.getWidth()),
                                                     0.0f, 1.0f, 0.0f));
            mg.drawImageAt (source, 0, 0);
            oppositeBank[static_cast<size_t> (frame)] = std::move (mirrored);
        }

        oppositeReady = true;
    }

    repaint();
    return true;
}

bool FlowerAnimationComponent::loadHighResStand (const void* data,
                                                        size_t size,
                                                        int studentIndex)
{
    if (studentIndex < 0 || studentIndex >= studentCount || data == nullptr || size == 0)
        return false;

    auto image = juce::ImageFileFormat::loadFrom (data, size);
    if (! image.isValid())
        return false;

    highResStand[static_cast<size_t> (studentIndex)] = image;
    highResStandReady[static_cast<size_t> (studentIndex)] = true;
    repaint();
    return true;
}

bool FlowerAnimationComponent::hasCompleteHighResActorCore() const noexcept
{
    for (int i = 0; i < studentCount; ++i)
        if (! highResWalkRightReady[static_cast<size_t> (i)]
            || ! highResWalkLeftReady[static_cast<size_t> (i)])
            return false;

    return true;
}

bool FlowerAnimationComponent::loadDirectory (const juce::File& directory)
{
    background = {};
    students = {};
    loadedStudentCount = 0;
    poseCoverageReady = false;
    resetStudentStates();

    if (! directory.isDirectory())
    {
        repaint();
        return false;
    }

    const auto backgroundFile = directory.getChildFile ("background").getChildFile ("rooftop_master.png");
    if (backgroundFile.existsAsFile())
        background = juce::ImageFileFormat::loadFrom (backgroundFile);

    if (! background.isValid())
    {
        repaint();
        return false;
    }

   #if JUCE_ANDROID
    constexpr int maxBackgroundWidth = 1280;
    if (background.getWidth() > maxBackgroundWidth)
    {
        const float scale = static_cast<float> (maxBackgroundWidth) / static_cast<float> (background.getWidth());
        const int height = juce::jmax (1, juce::roundToInt (static_cast<float> (background.getHeight()) * scale));
        background = background.rescaled (maxBackgroundWidth, height, juce::Graphics::mediumResamplingQuality);
    }
   #endif

    const auto studentsDirectory = directory.getChildFile ("students");
    for (int i = 0; i < studentCount; ++i)
        if (loadStudent (students[static_cast<size_t> (i)], studentsDirectory, i))
            ++loadedStudentCount;

    poseCoverageReady = validatePoseCoverage();
    repaint();
    return hasVisualBank();
}

FlowerAnimationComponent::FlowerAnimationComponent()
{
    startTimerHz (16);
}

FlowerAnimationComponent::~FlowerAnimationComponent()
{
    stopTimer();
}

int FlowerAnimationComponent::densityToCount (float density) const noexcept
{
    return juce::jlimit (0, studentCount,
                         juce::roundToInt (juce::jlimit (0.0f, 1.0f, density)
                                           * static_cast<float> (studentCount)));
}

FlowerAnimationComponent::SceneType
FlowerAnimationComponent::sanitiseSceneForCount (SceneType candidate, int count) const noexcept
{
    if (count <= 0)
        return SceneType::Empty;

    if (count == 1)
    {
        switch (candidate)
        {
            case SceneType::Line:
            case SceneType::SingleMove:
            case SceneType::AscendSingle:
            case SceneType::Suspended:
            case SceneType::DisperseSingle:
            case SceneType::Reform:
            case SceneType::FallOrSit:
            case SceneType::OddPose:
            case SceneType::Freeze:
            case SceneType::Appear:
            case SceneType::Disappear:
            case SceneType::LookBack:
                return candidate;

            default:
                return SceneType::LookBack;
        }
    }

    if (count == 2 && candidate == SceneType::Cluster)
        return SceneType::Conversation;

    return candidate;
}

void FlowerAnimationComponent::resetStudentStates()
{
    scene = SceneType::Empty;
    previousScene = SceneType::Empty;
    sceneTicks = 0;
    tick = 0;
    actorTick = 0;
    populationCooldownTicks = 0;
    populationDecisionTicks = 0;
    actorPopulationTarget = 0;
    visibleCount = 0;
    previousVisibleCount = 0;
    sceneActorCount = 0;
    lastPrimaryActor = -1;
    completionFreezeTicks = 0;
    sceneActors.fill (-1);

    static constexpr Pose basePoses[]
    {
        Pose::Stand, Pose::TurnLeft, Pose::HandsBehind, Pose::OneKnee,
        Pose::LookDown, Pose::TurnRight, Pose::Stand, Pose::Rail
    };

    static constexpr float identityHeightScale[]
    {
        1.00f, 1.02f, 0.99f, 1.01f, 0.94f, 1.06f, 1.00f, 1.02f
    };

    static constexpr float floorOffsets[]
    {
         0.000f, -0.004f,  0.005f,  0.002f,
         0.007f, -0.006f,  0.003f, -0.002f
    };

    static constexpr float homePositions[]
    {
        0.14f, 0.27f, 0.38f, 0.48f, 0.57f, 0.67f, 0.77f, 0.87f
    };

    for (int i = 0; i < studentCount; ++i)
    {
        auto& state = studentStates[static_cast<size_t> (i)];
        state = {};
        state.slot = i;
        state.phaseOffset = (i * 3) % 7;
        state.basePose = basePoses[i % static_cast<int> (std::size (basePoses))];
        state.ambientPose = state.basePose;
        state.restPose = state.basePose;
        state.pose = state.basePose;
        state.ambientTicks = 18 + i * 5 + random.nextInt (29);
        state.decisionTicks = 12 + i * 4 + random.nextInt (41);
        state.motionDirection = (i % 2 == 0) ? -1 : 1;
        state.currentX = homePositions[i];
        state.targetX = state.currentX;
        state.previousTargetX = state.currentX;
        state.moveRate = 0.006f + random.nextFloat() * 0.006f;
        state.heightScale = identityHeightScale[i];
        state.floorOffset = floorOffsets[i];
    }
}

int FlowerAnimationComponent::currentTargetActorCount() const noexcept
{
    if (recordingValue)
    {
        const int finalCount = densityToCount (densityValue);
        if (finalCount <= 0 || recordProgressValue <= 0.0f)
            return 0;

        return juce::jlimit (1, finalCount,
                            juce::roundToInt (std::ceil (recordProgressValue
                                                     * static_cast<float> (finalCount))));
    }

    return loopActiveValue ? densityToCount (densityValue) : 0;
}

FlowerAnimationComponent::Pose FlowerAnimationComponent::chooseActorRestPose (int index)
{
    static constexpr Pose quietPool[]
    {
        Pose::Stand, Pose::HandsBehind, Pose::ArmsFolded, Pose::OneKnee,
        Pose::TurnLeft, Pose::TurnRight, Pose::LookBack, Pose::LookDown,
        Pose::LookUp, Pose::Rail, Pose::LeanWall, Pose::Crouch,
        Pose::SitFloor, Pose::SitKneesUp
    };

    static constexpr Pose activePool[]
    {
        Pose::Stand, Pose::OneKnee, Pose::TurnLeft, Pose::TurnRight,
        Pose::LookBack, Pose::Point, Pose::HalfSquat, Pose::Crouch,
        Pose::StepIn, Pose::TurnInPlace
    };

    const float pitchActivity = juce::jlimit (0.0f, 1.0f, (pitchValue + 12.0f) / 24.0f);
    const float activity = juce::jlimit (0.0f, 1.0f, pitchActivity * 0.72f + mixValue * 0.28f);
    const bool chooseActive = random.nextFloat() < (0.18f + activity * 0.52f);
    const auto& student = students[static_cast<size_t> (juce::jlimit (0, studentCount - 1, index))];

    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const Pose candidate = chooseActive
            ? activePool[random.nextInt (static_cast<int> (std::size (activePool)))]
            : quietPool[random.nextInt (static_cast<int> (std::size (quietPool)))];

        if (student.has (candidate))
            return candidate;
    }

    return Pose::Stand;
}

FlowerAnimationComponent::ActorVariation FlowerAnimationComponent::chooseActorVariation() noexcept
{
    const float pitchActivity = juce::jlimit (0.0f, 1.0f, (pitchValue + 12.0f) / 24.0f);
    const float uncannyDrive = juce::jlimit (0.0f, 1.0f,
                                             mixValue * 0.48f
                                           + feedbackValue * 0.34f
                                           + pitchActivity * 0.18f);

    const int roll = random.nextInt (100);

    if (feedbackValue > 0.62f && roll < 22)
        return ActorVariation::Overlap;

    if (pitchActivity > 0.72f && roll < 32)
        return ActorVariation::UpperBodyWrong;

    if (roll < 14) return ActorVariation::Fade;
    if (roll < 28) return ActorVariation::Disperse;
    if (roll < 42) return ActorVariation::Overlap;
    if (roll < 54) return ActorVariation::UpperBodyWrong;
    if (roll < 64) return ActorVariation::Distant;
    if (roll < 74) return ActorVariation::Silhouette;
    if (roll < 84) return ActorVariation::Invert;
    if (roll < 94) return ActorVariation::Noise;

    return uncannyDrive > 0.50f ? ActorVariation::Freeze : ActorVariation::Fade;
}

void FlowerAnimationComponent::maybeStartActorVariation (StudentState& state)
{
    if (state.exiting || state.variation != ActorVariation::None || state.variationTicks > 0)
        return;

    const float pitchActivity = juce::jlimit (0.0f, 1.0f, (pitchValue + 12.0f) / 24.0f);
    const float drive = juce::jlimit (0.0f, 1.0f,
                                      mixValue * 0.52f
                                    + feedbackValue * 0.30f
                                    + pitchActivity * 0.18f);

    // Effects are occasional independent actor events, never a global cue.
    if (random.nextFloat() > 0.10f + drive * 0.36f)
        return;

    state.variation = chooseActorVariation();
    state.variationTicks = 8 + random.nextInt (24)
                         + juce::roundToInt (holdValue * 36.0f);
    state.variationTotalTicks = state.variationTicks;
    state.variationSeed = random.nextFloat();
}

void FlowerAnimationComponent::assignActorGoal (int index, bool entering)
{
    if (index < 0 || index >= studentCount)
        return;

    auto& state = studentStates[static_cast<size_t> (index)];

    const float centre = juce::jmap (positionValue, 0.0f, 1.0f, 0.20f, 0.80f);
    const float halfSpread = juce::jmap (spreadValue, 0.0f, 1.0f, 0.055f, 0.34f);
    float offset = (random.nextFloat() * 2.0f - 1.0f) * halfSpread;

    if (reverseValue)
        offset = -offset;

    const float freshTarget = juce::jlimit (0.07f, 0.93f, centre + offset);
    const float oldTarget = state.targetX;
    const float echoTarget = state.previousTargetX;
    const bool echoOldTarget = ! entering
                            && feedbackValue > 0.02f
                            && random.nextFloat() < feedbackValue * 0.58f;

    state.previousTargetX = oldTarget;
    state.targetX = echoOldTarget ? echoTarget : freshTarget;

    const float visualMixScale = juce::jmap (mixValue, 0.0f, 1.0f, 0.72f, 1.18f);
    state.moveRate = juce::jmap (juce::jlimit (0.008f, 0.50f, sizeValue),
                                 0.008f, 0.50f, 0.0045f, 0.020f)
                   * visualMixScale
                   * (0.78f + random.nextFloat() * 0.44f);
    state.restPose = chooseActorRestPose (index);
    state.ambientPose = state.restPose;
    state.decisionTicks = 14 + random.nextInt (58)
                        + juce::roundToInt (holdValue * 110.0f)
                        + ((index * 7) % 19);

    if (entering)
    {
        const float enterRightChance = reverseValue ? 0.72f : 0.28f;
        const bool enterFromRight = random.nextFloat() < enterRightChance;
        state.currentX = enterFromRight ? 1.04f : -0.04f;
        state.pose = enterFromRight ? Pose::WalkLeft : Pose::WalkRight;
    }
}

void FlowerAnimationComponent::reconcileActorPopulation()
{
    const int baseTarget = juce::jlimit (0, studentCount, currentTargetActorCount());

    if (recordingValue || ! loopActiveValue)
    {
        actorPopulationTarget = baseTarget;
        populationDecisionTicks = 0;
    }
    else if (populationDecisionTicks > 0)
    {
        --populationDecisionTicks;
    }
    else
    {
        // DENSITY is the centre of a population range, not a hard cast count.
        // The cast drifts slowly around it so the rooftop never becomes a
        // static 4-person/6-person tableau.
        const int radiusDown = baseTarget <= 1 ? 1 : 2;
        const int radiusUp = baseTarget >= 7 ? 1 : 2;
        const int low = juce::jlimit (0, studentCount, baseTarget - radiusDown);
        const int high = juce::jlimit (0, studentCount, baseTarget + radiusUp);
        actorPopulationTarget = low + random.nextInt (juce::jmax (1, high - low + 1));
        populationDecisionTicks = 28 + random.nextInt (76)
                                + juce::roundToInt (holdValue * 70.0f);
    }

    const int target = actorPopulationTarget;

    int active = 0;
    for (const auto& state : studentStates)
        if (state.visible && ! state.exiting)
            ++active;

    visibleCount = active;

    if (populationCooldownTicks > 0)
    {
        --populationCooldownTicks;
        return;
    }

    if (active < target)
    {
        std::array<int, studentCount> candidates {};
        int count = 0;
        for (int i = 0; i < studentCount; ++i)
            if (! studentStates[static_cast<size_t> (i)].visible)
                candidates[static_cast<size_t> (count++)] = i;

        if (count > 0)
        {
            const int index = candidates[static_cast<size_t> (random.nextInt (count))];
            auto& state = studentStates[static_cast<size_t> (index)];
            state.visible = true;
            state.targetVisible = true;
            state.exiting = false;
            state.action = ActionKind::None;
            state.variation = ActorVariation::None;
            state.variationTicks = 0;
            state.variationTotalTicks = 0;
            state.variationSeed = random.nextFloat();
            state.relation = -1;
            assignActorGoal (index, true);
            populationCooldownTicks = 3 + random.nextInt (6);
        }
    }
    else if (active > target)
    {
        std::array<int, studentCount> candidates {};
        int count = 0;
        for (int i = 0; i < studentCount; ++i)
        {
            const auto& state = studentStates[static_cast<size_t> (i)];
            if (state.visible && ! state.exiting)
                candidates[static_cast<size_t> (count++)] = i;
        }

        if (count > 0)
        {
            const int index = candidates[static_cast<size_t> (random.nextInt (count))];
            auto& state = studentStates[static_cast<size_t> (index)];
            state.targetVisible = false;
            state.exiting = true;
            const bool leaveRight = reverseValue ? state.currentX < 0.5f : state.currentX >= 0.5f;
            state.targetX = leaveRight ? 1.05f : -0.05f;
            state.moveRate = 0.010f + juce::jlimit (0.0f, 0.50f, sizeValue) * 0.030f;
            state.pose = leaveRight ? Pose::WalkRight : Pose::WalkLeft;
            populationCooldownTicks = 4 + random.nextInt (8);
        }
    }
}

void FlowerAnimationComponent::advanceActors()
{
    reconcileActorPopulation();

    const float pitchActivity = juce::jlimit (0.0f, 1.0f, (pitchValue + 12.0f) / 24.0f);
    const float activity = juce::jlimit (0.0f, 1.0f, pitchActivity * 0.72f + mixValue * 0.28f);

    for (int i = 0; i < studentCount; ++i)
    {
        auto& state = studentStates[static_cast<size_t> (i)];
        if (! state.visible)
            continue;

        if (state.variationTicks > 0)
        {
            --state.variationTicks;

            if (state.variation == ActorVariation::Freeze)
            {
                state.pose = Pose::FreezeMidStep;

                if (state.variationTicks <= 0)
                    state.variation = ActorVariation::None;

                continue;
            }

            if (state.variationTicks <= 0)
                state.variation = ActorVariation::None;
        }
        else if (state.variation == ActorVariation::None
              && ((actorTick + state.phaseOffset * 5) % 32) == 0)
        {
            maybeStartActorVariation (state);

            if (state.variation == ActorVariation::Freeze)
                continue;
        }

        const float delta = state.targetX - state.currentX;
        const float distance = std::abs (delta);

        if (distance > 0.004f)
        {
            const float step = juce::jmin (distance, state.moveRate);
            state.currentX += (delta < 0.0f ? -step : step);
            state.motionDirection = delta < 0.0f ? -1 : 1;
            state.pose = state.motionDirection < 0 ? Pose::WalkLeft : Pose::WalkRight;
        }
        else
        {
            state.currentX = state.targetX;

            if (state.exiting)
            {
                state.visible = false;
                state.targetVisible = false;
                state.exiting = false;
                state.variation = ActorVariation::None;
                state.variationTicks = 0;
                state.variationTotalTicks = 0;
                state.pose = state.basePose;
                continue;
            }

            state.pose = state.restPose;

            if (state.decisionTicks > 0)
                --state.decisionTicks;
            else
            {
                // Every actor decides independently. Higher pitch makes a new
                // movement more likely; high HOLD increases idle persistence.
                const float moveChance = 0.22f + activity * 0.58f;
                state.restPose = chooseActorRestPose (i);

                if (random.nextFloat() < moveChance)
                    assignActorGoal (i, false);
                else
                    state.decisionTicks = 18 + random.nextInt (72)
                                        + juce::roundToInt (holdValue * 125.0f);
            }
        }
    }

    visibleCount = 0;
    for (const auto& state : studentStates)
        if (state.visible)
            ++visibleCount;

    ++actorTick;
}

void FlowerAnimationComponent::timerCallback()
{
    if (! hasVisualBank())
        return;

    if (completionFreezeTicks > 0)
        --completionFreezeTicks;
    else
        advanceActors();

    repaint();
}

FlowerAnimationComponent::SceneType FlowerAnimationComponent::chooseNextScene()
{
    if (visibleCount <= 0)
        return SceneType::Empty;

    if (visibleCount == 1)
    {
        static constexpr SceneType singleScenes[]
        {
            SceneType::Line, SceneType::LookBack, SceneType::SingleMove,
            SceneType::FallOrSit, SceneType::OddPose, SceneType::AscendSingle,
            SceneType::DisperseSingle, SceneType::Freeze
        };

        SceneType candidate = scene;
        for (int attempt = 0; attempt < 8 && candidate == scene; ++attempt)
            candidate = singleScenes[random.nextInt (static_cast<int> (std::size (singleScenes)))];

        return sanitiseSceneForCount (candidate, visibleCount);
    }

    if (scene == SceneType::PushTension && random.nextInt (100) < 55)
        return SceneType::PushContact;

    if (scene == SceneType::DisperseSingle && random.nextInt (100) < 45)
        return SceneType::Reform;

    // Weight toward scenes where only one or two people are the focus.
    // Group actions still exist, but they are deliberately uncommon; most of
    // the visible variation now comes from each student's own ambient timing.
    static constexpr SceneType quietScenes[]
    {
        SceneType::Line, SceneType::Spread, SceneType::Conversation,
        SceneType::SitGroup, SceneType::Cluster, SceneType::HoldHands,
        SceneType::MixedTableau, SceneType::MixedTableau, SceneType::MixedTableau,
        SceneType::LookBack, SceneType::LookBack, SceneType::Line
    };

    static constexpr SceneType activeScenes[]
    {
        SceneType::SingleMove, SceneType::SingleMove, SceneType::SingleMove,
        SceneType::FallOrSit, SceneType::FallOrSit,
        SceneType::StaggeredJump, SceneType::GroupJump
    };

    static constexpr SceneType uncannyScenes[]
    {
        SceneType::AscendSingle, SceneType::AscendSingle, SceneType::AscendSingle,
        SceneType::Suspended,
        SceneType::DisperseSingle, SceneType::DisperseSingle,
        SceneType::Reform, SceneType::PushTension,
        SceneType::OddPose, SceneType::Freeze,
        SceneType::AscendGroup, SceneType::DisperseGroup
    };

    SceneType candidate = scene;
    for (int attempt = 0; attempt < 12; ++attempt)
    {
        const int quietBias = holdValue > 0.70f ? 82 : 62;
        const int activeLimit = holdValue > 0.70f ? 94 : 91;
        const int roll = random.nextInt (100);

        if (roll < quietBias)
            candidate = quietScenes[random.nextInt (static_cast<int> (std::size (quietScenes)))];
        else if (roll < activeLimit)
            candidate = activeScenes[random.nextInt (static_cast<int> (std::size (activeScenes)))];
        else
            candidate = uncannyScenes[random.nextInt (static_cast<int> (std::size (uncannyScenes)))];

        candidate = sanitiseSceneForCount (candidate, visibleCount);
        if (candidate != scene)
            break;
    }

    return candidate == scene ? SceneType::Line : candidate;
}

void FlowerAnimationComponent::configureScene (SceneType nextScene)
{
    const auto oldScene = scene;
    const auto oldActors = sceneActors;

    previousScene = scene;
    scene = sanitiseSceneForCount (nextScene, visibleCount);
    sceneTicks = 0;
    sceneActors.fill (-1);
    sceneActorCount = 0;

    if (scene == SceneType::Empty || visibleCount <= 0)
        return;

    std::array<int, studentCount> visible {};
    int n = 0;

    for (int i = 0; i < studentCount; ++i)
    {
        auto& state = studentStates[static_cast<size_t> (i)];
        if (! state.visible)
            continue;

        visible[static_cast<size_t> (n++)] = i;

        const bool wasSceneLocked = state.sceneLocked;
        state.sceneLocked = false;
        state.relation = -1;

        // Scene actors are released back to their own ambient state. Ambient
        // actions belonging to non-actors keep running across scene changes,
        // so a scene boundary is no longer a hidden global animation cue.
        if (wasSceneLocked)
        {
            state.action = ActionKind::None;
            state.delayTicks = state.phaseOffset + random.nextInt (4);
            state.phaseTicks = 0;
            state.xOffset = 0.0f;
            state.restPose = state.ambientPose;
            state.pose = state.ambientPose;
        }
        else if (state.action == ActionKind::None)
        {
            state.xOffset = 0.0f;
            state.restPose = state.ambientPose;
            state.pose = state.ambientPose;
        }
    }

    if (n <= 0)
        return;

    // Randomised participant order prevents recurring left-to-right group
    // choreography from reading as one synchronised animation.
    auto shuffled = visible;
    for (int k = n - 1; k > 0; --k)
    {
        const int j = random.nextInt (k + 1);
        const int temp = shuffled[static_cast<size_t> (k)];
        shuffled[static_cast<size_t> (k)] = shuffled[static_cast<size_t> (j)];
        shuffled[static_cast<size_t> (j)] = temp;
    }

    const auto isVisible = [&] (int index)
    {
        return index >= 0 && index < studentCount
            && studentStates[static_cast<size_t> (index)].visible;
    };

    auto choosePrimary = [&]
    {
        int chosen = visible[static_cast<size_t> (random.nextInt (n))];
        if (n > 1 && chosen == lastPrimaryActor)
        {
            for (int k = 0; k < n; ++k)
                if (visible[static_cast<size_t> (k)] != lastPrimaryActor)
                {
                    chosen = visible[static_cast<size_t> (k)];
                    break;
                }
        }
        return chosen;
    };

    int a = choosePrimary();
    int aPos = 0;
    for (int k = 0; k < n; ++k)
        if (visible[static_cast<size_t> (k)] == a)
            aPos = k;

    int b = n > 1 ? visible[static_cast<size_t> ((aPos + 1) % n)] : a;
    int cActor = n > 2 ? visible[static_cast<size_t> ((aPos + 2) % n)] : b;

    if (scene == SceneType::Reform && oldScene == SceneType::DisperseSingle && isVisible (oldActors[0]))
        a = oldActors[0];

    if (scene == SceneType::PushContact && oldScene == SceneType::PushTension
        && isVisible (oldActors[0]) && isVisible (oldActors[1]))
    {
        a = oldActors[0];
        b = oldActors[1];
    }

    sceneActors[0] = a;
    sceneActorCount = 1;
    lastPrimaryActor = a;

    const auto lockActor = [&] (int index)
    {
        if (index < 0 || index >= studentCount)
            return;

        auto& state = studentStates[static_cast<size_t> (index)];
        state.sceneLocked = true;
        state.action = ActionKind::None;
        state.delayTicks = 0;
        state.phaseTicks = 0;
        state.xOffset = 0.0f;
        state.restPose = state.ambientPose;
        state.pose = state.ambientPose;
    };

    const auto setPair = [&] (int left, int right, Pose leftPose, Pose rightPose)
    {
        sceneActors[0] = left;
        sceneActors[1] = right;
        sceneActorCount = 2;

        auto& l = studentStates[static_cast<size_t> (left)];
        auto& r = studentStates[static_cast<size_t> (right)];
        lockActor (left);
        lockActor (right);
        l.pose = leftPose;
        r.pose = rightPose;
        l.restPose = leftPose;
        r.restPose = rightPose;
        l.relation = right;
        r.relation = left;
        l.xOffset += 0.16f;
        r.xOffset -= 0.16f;
    };

    const auto startAction = [&] (int index, ActionKind action, int delay)
    {
        auto& state = studentStates[static_cast<size_t> (index)];
        lockActor (index);
        state.action = action;
        state.delayTicks = juce::jmax (0, delay);
        state.phaseTicks = 0;
        state.motionDirection = random.nextInt (2) == 0 ? -1 : 1;
        state.pose = state.restPose;
    };

    switch (scene)
    {
        case SceneType::Empty:
            break;

        case SceneType::Line:
            break;

        case SceneType::Spread:
            lockActor (a);
            studentStates[static_cast<size_t> (a)].xOffset = -0.16f;
            if (n > 2)
            {
                lockActor (cActor);
                studentStates[static_cast<size_t> (cActor)].xOffset = 0.16f;
            }
            break;

        case SceneType::Cluster:
        case SceneType::Conversation:
            setPair (a, b, Pose::TalkRight, Pose::TalkLeft);
            if (n > 2)
            {
                sceneActors[2] = cActor;
                sceneActorCount = 3;
                auto& third = studentStates[static_cast<size_t> (cActor)];
                lockActor (cActor);
                third.pose = scene == SceneType::Cluster ? Pose::Listen : Pose::Whisper;
                third.restPose = third.pose;
                third.relation = a;
                third.xOffset -= 0.08f;
            }
            break;

        case SceneType::HoldHands:
            setPair (a, b, Pose::HoldRight, Pose::HoldLeft);
            break;

        case SceneType::SitGroup:
            lockActor (a);
            studentStates[static_cast<size_t> (a)].pose = Pose::SitFloor;
            studentStates[static_cast<size_t> (a)].restPose = Pose::SitFloor;
            if (n > 3)
            {
                lockActor (cActor);
                studentStates[static_cast<size_t> (cActor)].pose = Pose::Crouch;
                studentStates[static_cast<size_t> (cActor)].restPose = Pose::Crouch;
            }
            break;

        case SceneType::SingleMove:
            startAction (a, ActionKind::Step, 1 + random.nextInt (5));
            break;

        case SceneType::GroupJump:
        {
            const int participants = juce::jmin (n, 2 + random.nextInt (juce::jmin (3, n - 1)));
            for (int k = 0; k < participants; ++k)
            {
                const int actor = shuffled[static_cast<size_t> (k)];
                startAction (actor, ActionKind::Jump, 4 + random.nextInt (28));
            }
            break;
        }

        case SceneType::StaggeredJump:
        {
            const int participants = juce::jmin (n, 2 + random.nextInt (2));
            for (int k = 0; k < participants; ++k)
            {
                const int actor = shuffled[static_cast<size_t> (k)];
                startAction (actor, ActionKind::Jump, 6 + k * 8 + random.nextInt (18));
            }
            break;
        }

        case SceneType::AscendSingle:
            startAction (a, ActionKind::Ascend, 2 + random.nextInt (6));
            break;

        case SceneType::AscendGroup:
        {
            const int participants = juce::jmin (n, 2 + random.nextInt (juce::jmin (3, n - 1)));
            for (int k = 0; k < participants; ++k)
            {
                const int actor = shuffled[static_cast<size_t> (k)];
                startAction (actor, ActionKind::Ascend, 8 + random.nextInt (34));
            }
            break;
        }

        case SceneType::Suspended:
            lockActor (a);
            studentStates[static_cast<size_t> (a)].pose = Pose::Suspended;
            studentStates[static_cast<size_t> (a)].restPose = Pose::Suspended;
            if (n > 4)
            {
                lockActor (cActor);
                studentStates[static_cast<size_t> (cActor)].pose = Pose::HangingPose;
                studentStates[static_cast<size_t> (cActor)].restPose = Pose::HangingPose;
            }
            break;

        case SceneType::DisperseSingle:
            startAction (a, ActionKind::Disperse, 2 + random.nextInt (6));
            break;

        case SceneType::DisperseGroup:
        {
            const int participants = juce::jmin (n, 2 + random.nextInt (juce::jmin (3, n - 1)));
            for (int k = 0; k < participants; ++k)
            {
                const int actor = shuffled[static_cast<size_t> (k)];
                startAction (actor, ActionKind::Disperse, 10 + random.nextInt (38));
            }
            break;
        }

        case SceneType::Reform:
            startAction (a, ActionKind::Reform, 1 + random.nextInt (4));
            break;

        case SceneType::PushTension:
            setPair (a, b, Pose::PushReady, Pose::HeadTurnBodyStill);
            break;

        case SceneType::PushContact:
            setPair (a, b, Pose::PushContact, Pose::PushRecoil);
            break;

        case SceneType::FallOrSit:
            startAction (a, ActionKind::Fall, 2 + random.nextInt (7));
            break;

        case SceneType::OddPose:
            lockActor (a);
            studentStates[static_cast<size_t> (a)].pose
                = random.nextInt (2) == 0 ? Pose::BodyTiltUnnatural : Pose::ArmsHeldOddly;
            studentStates[static_cast<size_t> (a)].restPose
                = studentStates[static_cast<size_t> (a)].pose;
            break;

        case SceneType::Freeze:
            lockActor (a);
            studentStates[static_cast<size_t> (a)].pose = Pose::FreezeMidStep;
            studentStates[static_cast<size_t> (a)].restPose = Pose::FreezeMidStep;
            if (n > 3)
            {
                lockActor (b);
                studentStates[static_cast<size_t> (b)].pose = Pose::FreezeMidJump;
                studentStates[static_cast<size_t> (b)].restPose = Pose::FreezeMidJump;
            }
            break;

        case SceneType::Appear:
        {
            const int entering = visible[static_cast<size_t> (n - 1)];
            startAction (entering, ActionKind::Step, 0);
            break;
        }

        case SceneType::Disappear:
            lockActor (a);
            studentStates[static_cast<size_t> (a)].pose = Pose::StepOut;
            studentStates[static_cast<size_t> (a)].restPose = Pose::StepOut;
            break;

        case SceneType::LookBack:
            lockActor (a);
            studentStates[static_cast<size_t> (a)].pose = Pose::LookBack;
            studentStates[static_cast<size_t> (a)].restPose = Pose::LookBack;
            break;

        case SceneType::MixedTableau:
        {
            lockActor (a);
            studentStates[static_cast<size_t> (a)].pose = Pose::SitFloor;
            studentStates[static_cast<size_t> (a)].restPose = Pose::SitFloor;

            if (n > 2)
                setPair (b, cActor, Pose::TalkRight, Pose::TalkLeft);
            else if (n > 1)
            {
                lockActor (b);
                studentStates[static_cast<size_t> (b)].pose = Pose::LookBack;
                studentStates[static_cast<size_t> (b)].restPose = Pose::LookBack;
            }

            if (n > 3)
            {
                int odd = visible[static_cast<size_t> ((aPos + 3) % n)];
                if (odd == a || odd == b || odd == cActor)
                    odd = visible[static_cast<size_t> ((aPos + n - 1) % n)];
                lockActor (odd);
                studentStates[static_cast<size_t> (odd)].pose = Pose::ArmsHeldOddly;
                studentStates[static_cast<size_t> (odd)].restPose = Pose::ArmsHeldOddly;
            }
            break;
        }
    }
}

void FlowerAnimationComponent::updateAmbientBehaviours()
{
    static constexpr Pose ambientPools[studentCount][7]
    {
        { Pose::Stand,       Pose::HandsBehind, Pose::LookDown,  Pose::LookBack, Pose::OneKnee,    Pose::TurnLeft,  Pose::Rail },
        { Pose::ArmsFolded,  Pose::LookDown,    Pose::TurnRight, Pose::Stand,    Pose::Rail,       Pose::Listen,    Pose::LookUp },
        { Pose::TurnLeft,    Pose::LeanWall,    Pose::LookBack,  Pose::Stand,    Pose::HandsBehind,Pose::Crouch,    Pose::LookDown },
        { Pose::OneKnee,     Pose::HandsBehind, Pose::TurnRight, Pose::LookDown, Pose::HalfSquat,  Pose::Stand,     Pose::LookBack },
        { Pose::Stand,       Pose::ArmsFolded,  Pose::LookUp,    Pose::Rail,     Pose::Crouch,     Pose::LookDown,  Pose::TurnLeft },
        { Pose::TurnRight,   Pose::LookBack,    Pose::LookUp,    Pose::Stand,    Pose::HandsBehind,Pose::LeanWall,  Pose::OneKnee },
        { Pose::Stand,       Pose::LookDown,    Pose::OneKnee,   Pose::ArmsFolded,Pose::TurnLeft,  Pose::Rail,      Pose::LookBack },
        { Pose::Rail,        Pose::LookBack,    Pose::LeanWall,  Pose::HandsBehind,Pose::TurnRight,Pose::Stand,     Pose::SitKneesUp }
    };

    for (int i = 0; i < studentCount; ++i)
    {
        auto& state = studentStates[static_cast<size_t> (i)];

        if (! state.visible || state.sceneLocked || state.relation >= 0
            || state.action != ActionKind::None)
            continue;

        if (state.ambientTicks > 0)
        {
            --state.ambientTicks;
            continue;
        }

        Pose nextPose = state.ambientPose;
        for (int attempt = 0; attempt < 5 && nextPose == state.ambientPose; ++attempt)
            nextPose = ambientPools[i][random.nextInt (7)];

        state.ambientPose = nextPose;
        state.restPose = nextPose;
        state.pose = nextPose;

        // Tiny independent position changes stop the row from reading like
        // keyed animation while keeping everybody grounded in the same rooftop.
        const float wander = (random.nextFloat() - 0.5f) * 0.18f;
        state.ambientXOffset = juce::jlimit (-0.22f, 0.22f,
                                             state.ambientXOffset + wander);
        state.ambientTicks = 32 + random.nextInt (90) + ((i * 5) % 17)
                           + juce::roundToInt (holdValue * 70.0f);

        // A small number of ambient changes become short walks. Because each
        // student owns a separate timer and direction these never start as a
        // group cue. High HOLD intentionally suppresses this extra motion.
        const int walkChance = holdValue > 0.70f ? 6 : (holdValue > 0.45f ? 11 : 18);
        if (random.nextInt (100) < walkChance)
        {
            state.action = ActionKind::Step;
            state.delayTicks = random.nextInt (12);
            state.phaseTicks = 0;
            state.motionDirection = random.nextInt (2) == 0 ? -1 : 1;
        }
    }
}

void FlowerAnimationComponent::updateStudentActions()
{
    const int phaseSpan = juce::jlimit (2, 12, 2 + juce::roundToInt (sizeValue * 18.0f));

    for (int i = 0; i < studentCount; ++i)
    {
        auto& state = studentStates[static_cast<size_t> (i)];
        if (! state.visible || state.action == ActionKind::None)
            continue;

        if (state.delayTicks > 0)
        {
            --state.delayTicks;
            state.pose = state.restPose;
            continue;
        }

        const int rawPhase = state.phaseTicks / phaseSpan;
        ++state.phaseTicks;

        switch (state.action)
        {
            case ActionKind::None:
                break;

            case ActionKind::Step:
            {
                const int p = juce::jlimit (0, 4, rawPhase);
                const int q = reverseValue ? 4 - p : p;
                switch (q)
                {
                    case 0: state.pose = Pose::StepIn; state.xOffset = 0.0f; break;
                    case 1: state.pose = state.motionDirection < 0 ? Pose::WalkLeft : Pose::WalkRight;
                            state.xOffset = 0.30f * static_cast<float> (state.motionDirection); break;
                    case 2: state.pose = Pose::TurnInPlace;
                            state.xOffset = 0.18f * static_cast<float> (state.motionDirection); break;
                    case 3: state.pose = Pose::StepOut; state.xOffset = 0.0f; break;
                    default: state.pose = state.restPose; state.xOffset = 0.0f; state.action = ActionKind::None; break;
                }
                break;
            }

            case ActionKind::Jump:
            {
                const int p = juce::jlimit (0, 3, rawPhase);
                const int q = reverseValue ? 3 - p : p;
                switch (q)
                {
                    case 0: state.pose = Pose::JumpLow; break;
                    case 1: state.pose = Pose::JumpHigh; break;
                    case 2: state.pose = Pose::Land; break;
                    default: state.pose = state.restPose; state.action = ActionKind::None; break;
                }
                break;
            }

            case ActionKind::Ascend:
            {
                const int p = juce::jlimit (0, 3, rawPhase);
                const int q = reverseValue ? 3 - p : p;
                switch (q)
                {
                    case 0: state.pose = Pose::LevitateLow; break;
                    case 1: state.pose = Pose::AscendArmsLoose; break;
                    case 2: state.pose = Pose::AscendHigh; break;
                    default: state.pose = Pose::Suspended; break;
                }
                break;
            }

            case ActionKind::Disperse:
            {
                const int p = juce::jlimit (0, 2, rawPhase);
                const int q = reverseValue ? 2 - p : p;
                state.pose = q == 0 ? Pose::MistDisperseStart
                           : q == 1 ? Pose::MistDisperseMid
                                    : Pose::MistDisperseEnd;
                break;
            }

            case ActionKind::Reform:
            {
                const int p = juce::jlimit (0, 3, rawPhase);
                const int q = reverseValue ? 3 - p : p;
                switch (q)
                {
                    case 0: state.pose = Pose::MistDisperseEnd; break;
                    case 1: state.pose = Pose::ReformStart; break;
                    case 2: state.pose = Pose::ReformMid; break;
                    default: state.pose = Pose::Stand; break;
                }
                break;
            }

            case ActionKind::Fall:
            {
                const int p = juce::jlimit (0, 3, rawPhase);
                const int q = reverseValue ? 3 - p : p;
                switch (q)
                {
                    case 0: state.pose = Pose::Stumble; break;
                    case 1: state.pose = Pose::FallBack; break;
                    case 2: state.pose = Pose::KneelDown; break;
                    default: state.pose = Pose::SitFloor; break;
                }
                break;
            }
        }
    }
}

void FlowerAnimationComponent::updateVisibleCount()
{
    int target = 0;

    if (recordingValue)
    {
        const int finalCount = densityToCount (densityValue);
        if (finalCount > 0 && recordProgressValue > 0.0f)
            target = juce::jlimit (1, finalCount,
                                   juce::roundToInt (std::ceil (recordProgressValue
                                                                * static_cast<float> (finalCount))));
    }
    else if (loopActiveValue)
    {
        target = densityToCount (densityValue);
    }

    target = juce::jlimit (0, studentCount, target);

    if (target == visibleCount)
        return;

    previousVisibleCount = visibleCount;

    if (target > visibleCount)
    {
        for (int i = visibleCount; i < target; ++i)
        {
            auto& state = studentStates[static_cast<size_t> (i)];
            state.visible = true;
            state.sceneLocked = false;
            state.slot = i;
            state.relation = -1;
            state.xOffset = 0.0f;
            state.ambientPose = state.basePose;
            state.restPose = state.basePose;
            state.pose = Pose::StepIn;
            state.action = ActionKind::Step;
            state.motionDirection = random.nextInt (2) == 0 ? -1 : 1;
            state.delayTicks = 2 + random.nextInt (14) + (i - visibleCount) * 3;
            state.phaseTicks = 0;
            state.ambientTicks = 28 + random.nextInt (55) + i * 3;
        }
    }
    else
    {
        for (int i = target; i < visibleCount; ++i)
        {
            auto& state = studentStates[static_cast<size_t> (i)];
            state.visible = false;
            state.sceneLocked = false;
            state.relation = -1;
            state.action = ActionKind::None;
            state.delayTicks = 0;
            state.phaseTicks = 0;
            state.xOffset = 0.0f;
            state.ambientPose = state.basePose;
            state.restPose = state.basePose;
            state.pose = state.basePose;
            state.ambientTicks = 24 + random.nextInt (51) + i * 4;
        }

        for (int i = 0; i < target; ++i)
        {
            auto& state = studentStates[static_cast<size_t> (i)];
            if (state.relation >= target)
            {
                state.sceneLocked = false;
                state.relation = -1;
                state.action = ActionKind::None;
                state.restPose = state.ambientPose;
                state.pose = state.ambientPose;
                state.xOffset = 0.0f;
            }
        }
    }

    visibleCount = target;

    if (visibleCount <= 0)
    {
        previousScene = scene;
        scene = SceneType::Empty;
        sceneTicks = 0;
    }
    else if (scene == SceneType::Empty)
    {
        configureScene (recordingValue ? SceneType::Appear : SceneType::Line);
    }
}

void FlowerAnimationComponent::advanceSceneIfNeeded()
{
    if (recordingValue)
    {
        if (scene != SceneType::Appear)
            configureScene (SceneType::Appear);
        return;
    }

    if (! loopActiveValue || visibleCount <= 0)
    {
        if (scene != SceneType::Empty)
            configureScene (SceneType::Empty);
        return;
    }

    if (scene == SceneType::Empty)
    {
        configureScene (SceneType::Line);
        return;
    }

    const int holdTicks = 12 + juce::roundToInt (holdValue * 72.0f);
    if (sceneTicks < juce::jmax (1, holdTicks))
        return;

    configureScene (chooseNextScene());
}

FlowerAnimationComponent::Pose
FlowerAnimationComponent::poseForStudent (int visibleIndex, int count) const noexcept
{
    juce::ignoreUnused (count);
    if (visibleIndex < 0 || visibleIndex >= studentCount)
        return Pose::Stand;

    return studentStates[static_cast<size_t> (visibleIndex)].pose;
}

float FlowerAnimationComponent::verticalOffsetForPose (Pose pose, int visibleIndex) const noexcept
{
    juce::ignoreUnused (visibleIndex);

    switch (pose)
    {
        case Pose::JumpLow:            return -0.09f;
        case Pose::JumpHigh:           return -0.22f;
        case Pose::FreezeMidJump:      return -0.18f;
        case Pose::LevitateLow:        return -0.16f;
        case Pose::AscendArmsLoose:    return -0.30f;
        case Pose::AscendReaching:     return -0.38f;
        case Pose::AscendHigh:         return -0.48f;
        case Pose::Suspended:          return -0.36f;
        case Pose::HangingPose:        return -0.42f;
        default:                       return 0.0f;
    }
}

void FlowerAnimationComponent::setState (float density, float position, float spread, float hold, float size,
                                         float pitch, float mix, float feedback,
                                         bool reverse, bool recording, float recordProgress, bool loopActive)
{
    const bool loopJustCompleted = previousRecordingValue
                                && ! recording
                                && loopActive;

    densityValue = juce::jlimit (0.0f, 1.0f, density);
    positionValue = juce::jlimit (0.0f, 1.0f, position);
    spreadValue = juce::jlimit (0.0f, 1.0f, spread);
    holdValue = juce::jlimit (0.0f, 1.0f, hold);
    sizeValue = juce::jlimit (0.008f, 0.50f, size);
    pitchValue = juce::jlimit (-12.0f, 12.0f, pitch);
    mixValue = juce::jlimit (0.0f, 1.0f, mix);
    feedbackValue = juce::jlimit (0.0f, 1.0f, feedback);
    recordProgressValue = juce::jlimit (0.0f, 1.0f, recordProgress);
    reverseValue = reverse;
    recordingValue = recording;
    loopActiveValue = loopActive;

    if (loopJustCompleted)
        completionFreezeTicks = 10; // ~0.625 s at 16 fps

    previousRecordingValue = recording;
    previousLoopActiveValue = loopActive;
}

void FlowerAnimationComponent::drawActorImage (juce::Graphics& g,
                                                const juce::Image& image,
                                                float centreX,
                                                float baselineY,
                                                float targetHeight,
                                                float opacity) const
{
    if (! image.isValid())
        return;

    const float aspect = static_cast<float> (image.getWidth())
                       / static_cast<float> (juce::jmax (1, image.getHeight()));
    const float targetWidth = targetHeight * aspect;
    const float x = centreX - targetWidth * 0.5f;
    const float y = baselineY - targetHeight;

    juce::Graphics::ScopedSaveState state (g);
    g.setOpacity (juce::jlimit (0.0f, 1.0f, opacity));
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    g.drawImageWithin (image,
                       juce::roundToInt (x),
                       juce::roundToInt (y),
                       juce::jmax (1, juce::roundToInt (targetWidth)),
                       juce::jmax (1, juce::roundToInt (targetHeight)),
                       juce::RectanglePlacement::centred,
                       false);
}

juce::Image FlowerAnimationComponent::makeActorEffectImage (const juce::Image& source,
                                                             ActorVariation variation,
                                                             int actorIndex) const
{
    if (! source.isValid()
        || (variation != ActorVariation::Silhouette
         && variation != ActorVariation::Invert
         && variation != ActorVariation::Noise))
        return source;

    auto result = source.createCopy();
    juce::Image::BitmapData pixels (result, juce::Image::BitmapData::readWrite);

    for (int y = 0; y < result.getHeight(); ++y)
    {
        for (int x = 0; x < result.getWidth(); ++x)
        {
            const auto colour = pixels.getPixelColour (x, y);
            const auto alpha = colour.getAlpha();

            if (alpha == 0)
                continue;

            if (variation == ActorVariation::Silhouette)
            {
                pixels.setPixelColour (x, y, juce::Colour::fromRGBA (245, 245, 245, alpha));
            }
            else if (variation == ActorVariation::Invert)
            {
                pixels.setPixelColour (x, y,
                    juce::Colour::fromRGBA (static_cast<juce::uint8> (255 - colour.getRed()),
                                            static_cast<juce::uint8> (255 - colour.getGreen()),
                                            static_cast<juce::uint8> (255 - colour.getBlue()),
                                            alpha));
            }
            else
            {
                const unsigned int hash = static_cast<unsigned int> (x * 73856093)
                                        ^ static_cast<unsigned int> (y * 19349663)
                                        ^ static_cast<unsigned int> ((actorTick + actorIndex * 17) * 83492791);
                const int noise = static_cast<int> ((hash >> 24) & 0xff) - 128;
                const int amount = noise / 4;

                pixels.setPixelColour (x, y,
                    juce::Colour::fromRGBA (
                        static_cast<juce::uint8> (juce::jlimit (0, 255, static_cast<int> (colour.getRed()) + amount)),
                        static_cast<juce::uint8> (juce::jlimit (0, 255, static_cast<int> (colour.getGreen()) + amount)),
                        static_cast<juce::uint8> (juce::jlimit (0, 255, static_cast<int> (colour.getBlue()) + amount)),
                        alpha));
            }
        }
    }

    return result;
}

void FlowerAnimationComponent::drawActorVariation (juce::Graphics& g,
                                                    const juce::Image& image,
                                                    const StudentState& state,
                                                    int actorIndex,
                                                    float centreX,
                                                    float baselineY,
                                                    float targetHeight,
                                                    juce::Rectangle<float> stage) const
{
    const float severity = juce::jlimit (0.0f, 1.0f, mixValue * 0.62f + feedbackValue * 0.38f);
    const float phase = static_cast<float> (actorTick + state.phaseOffset * 3) * 0.31f
                      + state.variationSeed * 6.2831853f;

    switch (state.variation)
    {
        case ActorVariation::Fade:
        {
            const float opacity = juce::jlimit (0.16f, 1.0f,
                                                0.58f + std::sin (phase) * (0.22f + severity * 0.18f));
            drawActorImage (g, image, centreX, baselineY, targetHeight, opacity);
            break;
        }

        case ActorVariation::Disperse:
        {
            const float progress = state.variationTotalTicks > 0
                ? juce::jlimit (0.0f, 1.0f,
                                1.0f - static_cast<float> (state.variationTicks)
                                      / static_cast<float> (state.variationTotalTicks))
                : 0.5f;

            // Keep the original actor visible early in the event, then dissolve
            // it into deterministic toner-like particles. The source asset is
            // never modified, and each actor uses a different seed/phase.
            const float bodyOpacity = juce::jlimit (0.05f, 1.0f, 1.0f - progress * 0.92f);
            drawActorImage (g, image, centreX, baselineY, targetHeight, bodyOpacity);

            const float aspect = static_cast<float> (image.getWidth())
                               / static_cast<float> (juce::jmax (1, image.getHeight()));
            const float targetWidth = targetHeight * aspect;
            const float left = centreX - targetWidth * 0.5f;
            const float top = baselineY - targetHeight;
            const int particleCount = 72;

            juce::Graphics::ScopedSaveState particleState (g);
            for (int particle = 0; particle < particleCount; ++particle)
            {
                unsigned int hash = static_cast<unsigned int> (particle * 73856093)
                                  ^ static_cast<unsigned int> ((actorIndex + 1) * 19349663)
                                  ^ static_cast<unsigned int> (
                                        juce::roundToInt (state.variationSeed * 100000.0f) * 83492791u);

                const float u = static_cast<float> ((hash >> 8) & 0xffff) / 65535.0f;
                hash = hash * 1664525u + 1013904223u;
                const float v = static_cast<float> ((hash >> 8) & 0xffff) / 65535.0f;

                const int sx = juce::jlimit (0, image.getWidth() - 1,
                                             juce::roundToInt (u * static_cast<float> (image.getWidth() - 1)));
                const int sy = juce::jlimit (0, image.getHeight() - 1,
                                             juce::roundToInt (v * static_cast<float> (image.getHeight() - 1)));
                const auto sourceColour = image.getPixelAt (sx, sy);
                if (sourceColour.getAlpha() < 48)
                    continue;

                hash = hash * 1664525u + 1013904223u;
                const float rx = static_cast<float> ((hash >> 8) & 0xffff) / 65535.0f - 0.5f;
                hash = hash * 1664525u + 1013904223u;
                const float ry = static_cast<float> ((hash >> 8) & 0xffff) / 65535.0f - 0.5f;

                const float spread = targetHeight * (0.02f + progress * (0.20f + severity * 0.16f));
                const float px = left + u * targetWidth
                               + rx * spread
                               + std::sin (phase + static_cast<float> (particle) * 0.37f)
                                 * spread * 0.08f;
                const float py = top + v * targetHeight
                               + ry * spread
                               - progress * targetHeight * (0.015f + severity * 0.025f);

                const float particleSize = juce::jmax (1.0f,
                    targetHeight * (0.004f + progress * 0.004f));
                const float alpha = juce::jlimit (0.0f, 1.0f,
                    progress * (0.28f + severity * 0.54f)
                    * static_cast<float> (sourceColour.getAlpha()) / 255.0f);

                g.setColour (sourceColour.withAlpha (alpha));
                g.fillRect (juce::Rectangle<float> (px, py, particleSize, particleSize));
            }
            break;
        }

        case ActorVariation::Overlap:
        {
            drawActorImage (g, image, centreX, baselineY, targetHeight, 0.84f);
            const float offset = targetHeight * (0.018f + feedbackValue * 0.045f);
            drawActorImage (g, image, centreX - offset, baselineY, targetHeight, 0.28f);
            drawActorImage (g, image, centreX + offset * 1.3f, baselineY, targetHeight, 0.22f);
            break;
        }

        case ActorVariation::UpperBodyWrong:
        {
            drawActorImage (g, image, centreX, baselineY, targetHeight, 1.0f);

            const float aspect = static_cast<float> (image.getWidth())
                               / static_cast<float> (juce::jmax (1, image.getHeight()));
            const float targetWidth = targetHeight * aspect;
            const float top = baselineY - targetHeight;
            juce::Graphics::ScopedSaveState clipped (g);
            g.reduceClipRegion (juce::Rectangle<int> (
                juce::roundToInt (centreX - targetWidth * 0.55f),
                juce::roundToInt (top),
                juce::jmax (1, juce::roundToInt (targetWidth * 1.1f)),
                juce::jmax (1, juce::roundToInt (targetHeight * 0.54f))));

            const float wrongOffset = targetHeight * (0.028f + severity * 0.055f)
                                    * (std::sin (phase) >= 0.0f ? 1.0f : -1.0f);
            drawActorImage (g, image,
                            centreX + wrongOffset,
                            baselineY - targetHeight * 0.012f,
                            targetHeight * (1.02f + severity * 0.035f),
                            0.78f);
            break;
        }

        case ActorVariation::Distant:
        {
            const float distantHeight = targetHeight * (0.46f + (1.0f - severity) * 0.12f);
            const float centreBias = stage.getCentreX() - centreX;
            drawActorImage (g, image,
                            centreX + centreBias * 0.08f,
                            baselineY - stage.getHeight() * 0.018f,
                            distantHeight,
                            0.96f);
            break;
        }

        case ActorVariation::Silhouette:
        case ActorVariation::Invert:
        case ActorVariation::Noise:
        {
            auto effected = makeActorEffectImage (image, state.variation, actorIndex);
            drawActorImage (g, effected, centreX, baselineY, targetHeight,
                            state.variation == ActorVariation::Silhouette ? 0.92f : 1.0f);
            break;
        }

        case ActorVariation::Freeze:
        case ActorVariation::None:
        default:
            drawActorImage (g, image, centreX, baselineY, targetHeight, 1.0f);
            break;
    }
}

void FlowerAnimationComponent::drawStudent (juce::Graphics& g,
                                             const StudentAsset& student,
                                             Pose pose,
                                             float centreX,
                                             float baselineY,
                                             float targetHeight) const
{
    const auto& image = student.imageFor (pose);
    if (! image.isValid())
        return;

    const float adjustedBaseline = baselineY
                                 + verticalOffsetForPose (pose, 0) * targetHeight;
    drawActorImage (g, image, centreX, adjustedBaseline, targetHeight);
}

void FlowerAnimationComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff10100f));

    if (! hasVisualBank())
    {
        auto area = bounds.toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff5f594d));
        g.drawRoundedRectangle (area, 5.0f, 1.0f);
        g.setColour (juce::Colour (0xffaaa087));
        g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        g.drawText ("FLOWER ARTWORK PENDING", bounds.reduced (12).removeFromTop (22),
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff777064));
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("approved rooftop + 8-student pose library required",
                    bounds.reduced (12).removeFromBottom (20),
                    juce::Justification::centredLeft);
        return;
    }

    auto stage = bounds.toFloat();
    const float targetAspect = 16.0f / 9.0f;
    const float currentAspect = stage.getWidth() / juce::jmax (1.0f, stage.getHeight());

    if (currentAspect > targetAspect)
    {
        const float width = stage.getHeight() * targetAspect;
        stage = stage.withSizeKeepingCentre (width, stage.getHeight());
    }
    else
    {
        const float height = stage.getWidth() / targetAspect;
        stage = stage.withSizeKeepingCentre (stage.getWidth(), height);
    }

    g.drawImage (background,
                 stage,
                 juce::RectanglePlacement::stretchToFit,
                 false);

    bool anyVisible = false;
    for (const auto& state : studentStates)
        anyVisible = anyVisible || state.visible;

    if (! anyVisible)
        return;

    const float stageWidth = stage.getWidth();
    const float stageHeight = stage.getHeight();
    const float baselineY = stage.getY() + stageHeight * 0.88f;
    const float targetHeight = stageHeight * 0.48f;

    // Actor v3: each student owns an independent normalized X position and
    // motion target. There is deliberately no shared slot/grid or scene centre.
    for (int i = 0; i < studentCount; ++i)
    {
        const auto& state = studentStates[static_cast<size_t> (i)];
        if (! state.visible)
            continue;

        const float x = stage.getX() + state.currentX * stageWidth;
        const bool walking = std::abs (state.targetX - state.currentX) > 0.004f;

        Pose pose = state.pose;
        if (walking)
            pose = state.motionDirection < 0 ? Pose::WalkLeft : Pose::WalkRight;

        const float studentHeight = targetHeight * state.heightScale;
        const float studentBaseline = baselineY + state.floorOffset * stageHeight;

        // Actor v3 never mixes high-resolution and legacy artwork on screen.
        // The approved 8-frame walk core becomes active only after all eight
        // students have a verified source strip; the opposite direction is
        // derived from that source by the approved geometric mirror rule.
        if (walking && hasCompleteHighResActorCore())
        {
            // Approved walk QA is displayed at 16 fps, but each source pose is
            // held for two actor ticks. This preserves the accepted restrained
            // 8 fps pose cadence while position still advances at 16 Hz.
            const int frame = ((actorTick + state.phaseOffset) / 2) % walkFrameCount;
            const auto& bank = state.motionDirection < 0
                             ? highResWalkLeft[static_cast<size_t> (i)]
                             : highResWalkRight[static_cast<size_t> (i)];
            drawActorVariation (g,
                                bank[static_cast<size_t> (frame)],
                                state,
                                i,
                                x,
                                studentBaseline,
                                studentHeight,
                                stage);
            continue;
        }

        if (! walking && hasCompleteHighResActorCore())
        {
            const auto& idleImage = highResStandReady[static_cast<size_t> (i)]
                                  ? highResStand[static_cast<size_t> (i)]
                                  : highResWalkRight[static_cast<size_t> (i)][0];

            drawActorVariation (g,
                                idleImage,
                                state,
                                i,
                                x,
                                studentBaseline,
                                studentHeight,
                                stage);
            continue;
        }

        if (verticalOffsetForPose (pose, i) > -0.04f)
        {
            juce::Graphics::ScopedSaveState shadowState (g);
            const float shadowWidth = studentHeight * 0.13f;
            const float shadowHeight = juce::jmax (2.0f, stageHeight * 0.008f);
            g.setColour (juce::Colours::black.withAlpha (0.15f));
            g.fillEllipse (x - shadowWidth * 0.5f,
                           studentBaseline - shadowHeight * 0.25f,
                           shadowWidth,
                           shadowHeight);
        }

        const auto& legacyImage = students[static_cast<size_t> (i)].imageFor (pose);
        const float adjustedBaseline = studentBaseline
                                     + verticalOffsetForPose (pose, i) * studentHeight;
        drawActorVariation (g,
                            legacyImage,
                            state,
                            i,
                            x,
                            adjustedBaseline,
                            studentHeight,
                            stage);
    }
}
