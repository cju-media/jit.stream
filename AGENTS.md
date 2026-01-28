# Instructions for Agents

## Build System

When working on the CMake build system for this Max External:

*   **macOS Code Signing**:
    *   Max externals on macOS must be code signed to load, even locally (ad-hoc signing is sufficient).
    *   Before running `codesign`, you **must** run `xattr -cr <path_to_bundle>` to remove extended attributes (like Resource Forks). Faling to do so will cause `codesign` to fail with "resource fork, Finder information, or similar detritus not allowed".
    *   The `CMakeLists.txt` is configured to handle this automatically in a `POST_BUILD` step. Do not remove this step.

## Platform Specifics

*   **Windows**: The entry point `ext_main` must be exported. Ensure `C74_EXPORT` is used in the C source.
