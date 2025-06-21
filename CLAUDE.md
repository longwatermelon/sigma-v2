# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Important Restrictions

- **NEVER run `./a.out` commands** - The user will handle all video generation testing

## Setup and Build Commands

```bash
# Set up OpenAI API key for TTS
export OPENAI_API_KEY="your-api-key-here"

# Build the project
make

# Run with default video type (anime edit)
./a.out

# Run specific video types
./a.out 1    # bateman edit (requires bateman.mp4 in res/video/edit/)
./a.out 2    # bateman meme compilation (requires bateman.mp4 in res/video/edit/)
./a.out 3    # comparison
./a.out 4    # shelby edit
./a.out 5    # shelby meme compilation
./a.out 6    # quiz
./a.out 7    # anime edit (default)
./a.out 8    # conspiracy (requires topic argument: ./a.out 8 "topic")

# Show help (only shows types 1-6, but types 7-8 are also available)
./a.out -h
```

## Project Architecture

This is a C++ video generation tool that creates "sigma male" themed videos by combining source footage, audio, text overlays, and effects. The architecture consists of:

### Core Components

- **main.cpp**: Entry point that orchestrates video creation based on type selection
- **video.cpp/h**: Core video processing engine that handles frame-by-frame rendering and event processing
- **source.h**: Defines video source cuts, event types, and content generation logic for different video formats
- **util.h**: Utility functions including time/frame conversion, audio duration calculation, text-to-speech, and OpenAI API integration

### Video Creation Pipeline

1. **Source Selection**: Selects base video files from `res/video/` based on video type
2. **Event Generation**: Creates timeline of events (text overlays, background changes, sound effects) 
3. **Frame Processing**: Renders each frame by applying active events and effects
4. **Audio Integration**: Combines background music with generated sound effects using FFmpeg
5. **Output**: Produces final MP4 in `out/` directory

### Event System

The system uses an event-driven architecture where different event types (Background, Text, Region, Sound Effects, PNG Overlays, etc.) are scheduled on a timeline and processed frame-by-frame. Events are defined in namespaces like `edit::`, `meme::`, `compare::`, `quiz::`, and `conspiracy::`.

**Event Types Available:**
- **Background**: Video background cuts with optional Y offset
- **Text**: Centered text overlays with optional large formatting
- **TopText**: Text displayed at the top of the frame
- **LeftText**: Text displayed on the left side of the frame
- **Caption**: Bottom caption text (used for narration/speech)
- **Region**: Video regions with custom source rectangles, positioning, and scaling
- **HBar**: Horizontal bars for visual separation
- **Character**: Character image overlays (deprecated, use PngOverlay instead)
- **TimerBar**: Countdown timer bars
- **Sfx**: Sound effects that play at specific times
- **PngOverlay**: PNG image overlays with transparency support and precise positioning

### Dependencies

- OpenCV 4 (video processing and rendering)
- libcurl (OpenAI API requests)
- nlohmann/json (JSON parsing)
- FFmpeg (audio processing, called via system commands)
- OpenAI TTS API (high-quality neural text-to-speech synthesis)

### Resource Structure

- `res/audio/`: Background music files (aura-compare.mp3, derniere-beatdrop.mp3, next.mp3, nofear.mp3, wiishop.mp3, clock.wav)
- `res/video/`: Source video files organized by type
  - `edit/`: anime.mp4, shelby.mp4 (bateman.mp4 may be missing)
  - `compare/`: src.mp4 for comparison videos
  - `images/`: PNG images for overlays (bateman-think.png, bateman-drink.png)
  - `parkour.mp4`: Used for quiz and conspiracy videos
- `res/font.ttf`: Font for text rendering
- `res/cloaked-figure.png`: Background image for conspiracy theory videos
- Environment variable `OPENAI_API_KEY` required for TTS functionality
- `out/`: Generated video output directory

### Video Types

Each video type implements different content generation strategies:
- **Edit videos** (1, 4, 7): Sync cuts to music beats with inspirational text
- **Meme compilations** (2, 5): Generate random captions over footage  
- **Comparisons** (3): Character vs character battles with scoring
- **Quiz videos** (6): Interactive sigma-themed questions with dynamic PNG overlays (bateman-think.png during questions/timers, bateman-drink.png during other periods)
- **Conspiracy videos** (8): AI-generated conspiracy theories via OpenAI API with custom background imagery and TTS narration

The system automatically generates titles and uses randomization to create varied content within each format. The default video type is 7 (anime edit) when no arguments are provided.

## Text-to-Speech System

The project uses OpenAI's TTS API for high-quality neural voice synthesis. Key features:

- **Voice Model**: Onyx (natural-sounding voice from OpenAI)
- **API**: Uses OpenAI's `/v1/audio/speech` endpoint with the `gpt-4o-mini-tts` model
- **Audio Format**: WAV format for direct duration calculation (no FFmpeg conversion needed)
- **Voice Control**: Uses instructions parameter for confident, authoritative tone (conspiracy videos use aggressive, angry tone)
- **Audio Processing**: Includes WAV header fixing and silence trimming capabilities
- **Functions**: 
  - `tts_generate(text, output_file)`: Generate WAV audio file from text using OpenAI TTS
  - `tts_dur(text)`: Calculate duration of spoken text for timeline sync
- **Setup**: Set the `OPENAI_API_KEY` environment variable with your OpenAI API key
- **Integration**: TTS is used for duration calculations in video event timing

The TTS system provides high-quality voice synthesis through OpenAI's latest neural models with enhanced control capabilities.

## PNG Overlay System

The project supports PNG image overlays with full transparency support and precise positioning:

- **Event Type**: `PngOverlay` (enum value 10)
- **Positioning**: Specify row and column coordinates for the top-left corner of the PNG
- **Resolution**: PNGs are displayed at their original resolution
- **Transparency**: Full RGBA support with alpha blending for smooth overlays
- **Bounds Checking**: Automatic clipping to prevent rendering outside video frame boundaries
- **Usage**: `evt_png_overlay(start_time, end_time, "path/to/image.png", row, col)`

**Quiz Video Integration:**
- Quiz videos (type 6) use dynamic PNG overlays positioned at (650, 580) for 400x400 images
- `bateman-think.png`: Displayed during question announcements and timer periods
- `bateman-drink.png`: Displayed during intro, answer reveals, and final text
- Images are positioned in the mid-right area, above captions and to the right of answer listings

## Known Issues

- The help menu (`./a.out -h`) only displays video types 1-6, but types 7 (anime edit) and 8 (conspiracy) are also available
- Video types 1-2 (bateman edit/meme) may fail if `res/video/edit/bateman.mp4` is missing from the repository
- Conspiracy videos require both the video type argument (8) and a topic string argument
