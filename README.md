# jit.stream

`jit.stream` is a Max/MSP external that allows you to stream video (Jitter Matrices) and audio (MSP Signals) directly to an RTMP server (e.g., YouTube Live, Twitch, Nginx RTMP) using FFmpeg.

## Features

- **Video Input**: Accepts Jitter Matrices (4-plane `char` ARGB/RGBA).
- **Audio Input**: Accepts Stereo MSP Signals.
- **Protocol**: RTMP (Real-Time Messaging Protocol).
- **Encoding**: H.264 (Video) and AAC (Audio).
- **Architecture**: Threaded encoding to prevent Max UI freezes.

## Build Instructions

### Prerequisites

1.  **Max SDK**: Download the [Max 8 SDK](https://github.com/cycling74/max-sdk) (or newer).
2.  **FFmpeg**: Install FFmpeg development libraries.
    *   **macOS**: `brew install ffmpeg` (via Homebrew).
    *   **Windows**: Download shared/dev builds from [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) or similar.
3.  **CMake**: Version 3.15 or higher.

### Building

1.  Clone this repository.
2.  Create a `build` directory:
    ```bash
    mkdir build
    cd build
    ```
3.  Run CMake:
    *   You need to point `MAX_SDK_PATH` to your Max SDK `source/c74support` parent folder.
    *   **macOS**:
        ```bash
        cmake -DMAX_SDK_PATH=/path/to/max-sdk-base ..
        make
        ```
    *   **Windows**:
        ```bash
        cmake -DMAX_SDK_PATH="C:/path/to/max-sdk-base" ..
        cmake --build . --config Release
        ```

4.  The output file (`jit.stream.mxo` or `jit.stream.mxe64`) will be generated. Copy this to your Max Packages `externals` folder or search path.

## Operation Instructions

### Object Instantiation

Create the object in a Max patcher:

```
[jit.stream]
```

### Attributes

| Attribute | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `@url` | Symbol | `rtmp://localhost/live/stream` | The destination RTMP URL. |
| `@width` | Long | `1280` | Video width in pixels. |
| `@height` | Long | `720` | Video height in pixels. |
| `@fps` | Long | `30` | Target frames per second. |
| `@bitrate` | Long | `2500000` (2.5 Mbps) | Video bitrate in bits per second. |
| `@enable` | Long (0/1) | `0` | Start (`1`) or Stop (`0`) streaming. |

### Inputs

*   **Inlet 0 (Left)**: Jitter Matrix.
    *   Connect a `jit.matrix` or `jit.grab`.
    *   **Note on Textures**: Direct `jit.gl.texture` input is not supported due to OpenGL context limitations. To stream a texture (e.g., from `jit.world` or `jit.gl.node`), use `[jit.gl.asyncread]` to download the texture to a matrix before sending it to `jit.stream`.
*   **Inlet 0 & 1 (Signals)**: Stereo Audio.
    *   The object performs as a standard MSP object (`~`).
    *   Send audio signals to the left inlet (Left Channel) and the second inlet (Right Channel, if available/configured, though currently `jit.stream` has 2 inlets total, so Left=Video/AudioL, Right=AudioR).
    *   *Correction*: The object layout is:
        *   **Inlet 0**: Video (Matrix) + Audio Left (Signal).
        *   **Inlet 1**: Audio Right (Signal).

### Example Usage

1.  **Set up Video**:
    ```
    [jit.grab @output_texture 0] -> [jit.stream @url rtmp://... @enable 1]
    ```
    Or from OpenGL:
    ```
    [jit.world @output_texture 1] -> [jit.gl.asyncread] -> [jit.stream]
    ```

2.  **Set up Audio**:
    ```
    [ezadc~] -> (Inlet 0) [jit.stream] (Inlet 1) <- [ezadc~]
    ```

3.  **Start Streaming**:
    *   Set the `@url` to your RTMP endpoint (e.g., from YouTube Studio).
    *   Send the message `enable 1` to start.
    *   Send `enable 0` to stop.

### Troubleshooting

*   **Crash on resolution change**: The external handles resolution changes automatically, but ensure your `width`/`height` attributes match your source if you want a specific output scale.
*   **UI Freeze**: If the URL is unreachable, the connection attempt runs in a background thread, but initial DNS resolution might still cause a slight hiccup depending on the OS.
