# jit.pass

`jit.pass` is a minimal Max/MSP external used for testing the build and signing process. It simply passes a Jitter Matrix from the input to the output.

## Purpose

This external exists to verify:
1.  **Build System**: CMake can locate the Max SDK and compile successfully on macOS/Windows.
2.  **Code Signing**: The generated `.mxo` bundle is correctly signed (ad-hoc) and satisfies macOS Gatekeeper (system security policy).
3.  **Loading**: Max can load the bundle without "incorrect architecture" or "executable not found" errors.

## Build Instructions

1.  **Dependencies**: Max SDK (point `MAX_SDK_PATH` to it).
2.  **Build**:
    ```bash
    mkdir build && cd build
    cmake ..
    make
    ```
3.  **Install**: Copy `jit.pass.mxo` to your `Max 8/Packages/jit.pass/externals` or search path.

## Usage

```max
[jit.noise 4 char 320 240]
|
[jit.pass]
|
[jit.pwindow]
```

If the window shows noise, the external is working.
