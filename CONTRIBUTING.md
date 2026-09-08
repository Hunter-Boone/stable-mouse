# Contributing

Stable Mouse is free software under GPL-3.0-only. Contributions use the same license.

Build and run the tests described in the README. For input changes, use a second mouse or keyboard to keep control and complete the relevant hardware checks in `docs/validation.md`. Keep hook callbacks short and avoid logging individual input events. Preserve automatic release of grabbed devices on errors.

Report usability issues in plain terms, such as overshoot, noticeable delay, difficulty dragging, or trouble pressing controls. Include the settings and device details needed to reproduce the issue. Medical diagnoses are not needed.

Feature candidates after hardware validation include per-device profiles, optional click debouncing, and an adjustable click hold. These need separate usability testing because they change how intentional actions are interpreted. Movement history and medical scoring are outside the first release.
