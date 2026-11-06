# CircleCI pipeline selection — standalone MIYAKO FM-1 (2026-11-06)

Repository: hakaihum-cpu/flower-standalone
Branch: feature/fm1-miyako-standalone-20261106
CircleCI config: .circleci/config.yml (on this branch)
Trigger: manual only, parameter run_build=true
Workflow: fm1_manual
Current job: fm1_standalone_preflight
Expected artifact: fm1-dist/READINESS.txt

**Do not confuse preflight with a firmware build.** This job validates the
independent UI controller and source-closure rules. No firmware executable,
JieLi target link, loader, FWSC package or device-flashing file can be created
until MIYAKO DSP/FX, all visual modes, board HAL, boot/update and verified
linker path are implemented. No dummy firmware or legacy KARMA executable is
allowed as a shortcut.

Old branch feature/fm1-miyako-port-20261104 retains its old CircleCI setup
for historical inspection only. It is not the standalone MIYAKO delivery path.

Current status: configuration committed, source-level static review complete.
CircleCI itself has **not been run**. Therefore no pipeline number, no CI result
and no firmware binary exist from this work.
