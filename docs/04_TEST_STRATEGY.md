# Test Strategy

## Bootstrap gates
1. Static diff review
2. Android Lint
3. JVM unit tests
4. Debug APK build

## Future test levels
Add only when relevant:
- ViewModel/domain unit tests
- Instrumentation tests
- UI tests
- Device matrix tests
- Performance tests
- Audio/MIDI/USB/Bluetooth tests
- Upgrade/migration tests

## Regression rule
Each Jira issue must identify the existing behavior that could regress.
