# jit.rtmp

A Max/MSP Jitter external for streaming video to an RTMP server (e.g., YouTube Live, Twitch, local RTMP server).

## Features
- Accepts Jitter matrices (ARGB/RGBA) as input.
- Encodes video to H.264 using FFmpeg (libx264).
- Streams to an RTMP URL.
- Low latency settings.

## Prerequisites

- **Max SDK**: You need the Max SDK to compile this external.
- **FFmpeg**: You need FFmpeg development libraries (`libavcodec`, `libavformat`, `libavutil`, `libswscale`) installed and available via `pkg-config`.

## Building

1.  Clone this repository.
2.  Create a build directory: `mkdir build && cd build`.
3.  Run CMake, pointing to your Max SDK location if not in `../max-sdk-base`.
    ```bash
    cmake .. -DMAX_SDK_PATH=/path/to/max-sdk
    ```
4.  Compile: `make`.

## Usage

1.  Copy the compiled external (`jit.rtmp.mxo` or `jit.rtmp.mxe64`) to your Max search path.
2.  Create a `jit.rtmp` object in your patch.
3.  Connect a video source (e.g., `jit.grab`, `jit.movie`, or `jit.world` output) to the input.
4.  Set the `url` attribute to your RTMP destination.
    ```
    url rtmp://localhost/live/stream
    ```
5.  Set encoding parameters:
    - `width`: Video width (default 1280)
    - `height`: Video height (default 720)
    - `framerate`: Framerate (default 30)
    - `bitrate`: Bitrate in bits/s (default 2500000)
6.  Start streaming by setting the `running` attribute to 1.
    ```
    running 1
    ```

## Using with Textures (jit.gl, jit.world)

`jit.rtmp` operates on Jitter matrices (CPU memory). If you are using `jit.world` or other OpenGL objects that output textures (GPU memory), you must convert the texture to a matrix using `jit.gl.asmatrix` before sending it to `jit.rtmp`.

```
[jit.world @enable 1 @output_texture 1]
   |
[jit.gl.asmatrix @pixel_format 4 2 1 3] (Ensure correct plane mapping if needed)
   |
[jit.rtmp @url rtmp://live.twitch.tv/app/STREAM_KEY @width 1280 @height 720 @running 1]
```

## Notes

- The external expects 4-plane char matrices (ARGB/RGBA).
- It uses `libx264` with `zerolatency` tuning for low latency.
- Ensure your network bandwidth is sufficient for the chosen bitrate.
