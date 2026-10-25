# POSE WALK — Android pixel pose sandbox

Static, offline WebView-wrapped Android game using the user's original reference image converted into 28 embedded pixel sprites. No AI-generated assets, network permissions, ads or accounts.

- Touch LEFT / RIGHT to walk while held. Walking uses a 12-frame interpolated sprite cycle from the original image, normalized to right-facing source frames and mirrored only for leftward motion.
- Select any of the unchanged 28 reference poses (standing, crouching, sitting, lying, walking). The interpolated walking cycle is separate from the reference poses.
- RESET centers the character.
- App id: com.posewalk.studio, minSdk 24, targetSdk 35.
- Android Gradle Plugin 8.7.3, Gradle 8.9.
- Build is manual via CircleCI workflow manual_android_build, run_build=true. Preflight validates all 28 poses, 12 walking frames and direction-specific canvas flipping before installing the Android SDK.
- The existing products/main/Golden branches are untouched.

After the CircleCI job passes, download POSE-WALK-debug.apk from the build's artifacts.
