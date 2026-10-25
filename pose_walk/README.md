# POSE WALK — Android pixel pose sandbox

Static, offline WebView-wrapped Android game using the user's original reference image converted into 28 embedded pixel sprites. No AI-generated assets, network permissions, ads or accounts.

- Touch LEFT / RIGHT to walk while held.
- Select any of 28 poses (standing, crouching, sitting, lying, walking).
- RESET centers the character.
- App id: com.posewalk.studio, minSdk 24, targetSdk 35.
- Android Gradle Plugin 8.7.3, Gradle 8.9.
- Build is manual via CircleCI workflow manual_android_build, run_build=true.
- The existing products/main/Golden branches are untouched.

After the CircleCI job passes, download POSE-WALK-debug.apk from the build's artifacts.
