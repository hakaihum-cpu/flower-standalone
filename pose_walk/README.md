# POSE WALK — Android pixel pose sandbox

Static, offline WebView-wrapped Android game using the user's original reference image converted into 28 embedded pixel sprites. No AI-generated assets, network permissions, ads or accounts.

- Touch LEFT / RIGHT to walk while held. Walking uses 12 new foot-articulated frames built from one consistently right-facing side-view original (pose 19). **Both feet now use the exact same cropped right-facing boot PNG.** The old implementation incorrectly used two different feet from the source image. Every generated frame records the same boot source SHA256 fingerprint. When moving left, the complete frame is mirrored.
- Select any of the unchanged 28 reference poses (standing, crouching, sitting, lying, walking). The interpolated walking cycle is separate from the reference poses.
- RESET centers the character.
- App id: com.posewalk.studio, minSdk 24, targetSdk 35.
- Android Gradle Plugin 8.7.3, Gradle 8.9.
- Build is manual via CircleCI workflow manual_android_build, run_build=true. Preflight validates all 28 poses, 12 right-facing walking frames (48x96), the single shared boot source fingerprint, and direction-specific canvas flipping before installing the Android SDK. Node smoke regression verifies every gait frame in both travel directions.
- The existing products/main/Golden branches are untouched.

After the CircleCI job passes, download POSE-WALK-debug.apk from the build's artifacts.
