\[ English | [简体中文](../../../../zh-cn/api/framework/media/index.md) \]

# Multimedia API

The openvela multimedia framework provides unified capabilities for audio/video playback, recording, audio focus management, policy control, and media session, along with voice wakeup and utility interfaces.

## openvela Implementation Notes

- **Codec Backend**: The underlying audio/video codec, muxing/demuxing, and filter capabilities are provided by **FFmpeg**, with source located at `external/ffmpeg/` (LGPL v2.1+)
- **Usage Recommendations**:
    - Prefer the openvela `framework/media` wrappers (`media_player_*` / `media_recorder_*`, etc.), which integrate audio focus, policy control, and session synchronization
    - For custom filter graphs, direct codec access, or probing non-standard streaming media, use the FFmpeg native API directly; refer to the [FFmpeg Official Documentation](https://ffmpeg.org/documentation.html)
- **FFmpeg Integration Configuration** (`external/ffmpeg/Kconfig`):

    ```kconfig
    CONFIG_LIB_FFMPEG=y                # Main switch: enable FFmpeg library
    CONFIG_LIB_FFMPEG_CONFIGURATION="" # Arguments passed to FFmpeg ./configure for component trimming
    CONFIG_LIB_FFMPEG_TEST=n           # Whether to compile FFmpeg test targets
    CONFIG_UTILS_FFMPEG_PRIORITY=100   # Task priority for the ffmpeg CLI tool
    CONFIG_UTILS_FFMPEG_STACKSIZE=51200 # Task stack size for the ffmpeg CLI tool
    ```

    Component trimming is done via the `CONFIG_LIB_FFMPEG_CONFIGURATION` string passed to FFmpeg's built-in `./configure` script. For example:

    ```kconfig
    CONFIG_LIB_FFMPEG_CONFIGURATION="--disable-everything --enable-decoder=mp3,aac --enable-demuxer=mov,mp4"
    ```

    For available `--enable-*` / `--disable-*` options, refer to FFmpeg's official `./configure --help`.

## Core Capabilities

- **[Player](media_player.md)** — Audio/video playback (local/network stream/byte stream)
- **[Recorder](media_recorder.md)** — Audio/video recording and image capture
- **[Media Session](media_session.md)** — Controller-controllee playback control and state synchronization

## Audio Policy

- **[Audio Focus](media_focus.md)** — Multi-application audio playback priority coordination
- **[Audio Policy](media_policy.md)** — Audio routing, device management, volume and mode switching

## Voice Wakeup

- **[Media Trigger](media_trigger.md)** — High-level voice wakeup interface (acoustic model loading + recognition control)
- **[Sound Model](media_trigger_model.md)** — Low-level acoustic model operations (loading/properties/hotword detection)

## Utilities and Debugging

- **[Media Utils](media_utils.md)** — DTMF signal generation, event name lookup, dump, custom commands
