# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Setup and Build Commands

```bash
# Set up OpenAI API key for TTS
export OPENAI_API_KEY="your-api-key-here"

# Build the project
make

# Run with default video type (anime edit)
./a.out

# Run specific video types
./a.out 1    # bateman edit
./a.out 2    # bateman meme compilation  
./a.out 3    # comparison
./a.out 4    # shelby edit
./a.out 5    # shelby meme compilation
./a.out 6    # quiz
./a.out 7    # anime edit (default)
./a.out 8    # conspiracy (requires topic argument: ./a.out 8 "topic")

# Show help
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

The system uses an event-driven architecture where different event types (Background, Text, Region, Sound Effects, etc.) are scheduled on a timeline and processed frame-by-frame. Events are defined in namespaces like `edit::`, `meme::`, `compare::`, `quiz::`, and `conspiracy::`.

### Dependencies

- OpenCV 4 (video processing and rendering)
- libcurl (OpenAI API requests)
- nlohmann/json (JSON parsing)
- FFmpeg (audio processing, called via system commands)
- OpenAI TTS API (high-quality neural text-to-speech synthesis)

### Resource Structure

- `res/audio/`: Background music and sound effects  
- `res/video/`: Source video files organized by type (edit/, compare/, etc.)
- `res/font.ttf`: Font for text rendering
- Environment variable `OPENAI_API_KEY` required for TTS functionality
- `out/`: Generated video output directory

### Video Types

Each video type implements different content generation strategies:
- **Edit videos**: Sync cuts to music beats with inspirational text
- **Meme compilations**: Generate random captions over footage  
- **Comparisons**: Character vs character battles with scoring
- **Quiz videos**: Interactive sigma-themed questions
- **Conspiracy videos**: AI-generated conspiracy theories via OpenAI API

The system automatically generates titles and uses randomization to create varied content within each format.

## Text-to-Speech System

The project uses OpenAI's TTS API for high-quality neural voice synthesis. Key features:

- **Voice Model**: Onyx (natural-sounding voice from OpenAI)
- **API**: Uses OpenAI's `/v1/audio/speech` endpoint with the `gpt-4o-mini-tts` model
- **Audio Format**: WAV format for direct duration calculation (no FFmpeg conversion needed)
- **Voice Control**: Uses instructions parameter for confident, authoritative tone
- **Functions**: 
  - `tts_generate(text, output_file)`: Generate WAV audio file from text using OpenAI TTS
  - `tts_dur(text)`: Calculate duration of spoken text for timeline sync
- **Setup**: Set the `OPENAI_API_KEY` environment variable with your OpenAI API key
- **Integration**: TTS is used for duration calculations in video event timing

The TTS system provides high-quality voice synthesis through OpenAI's latest neural models with enhanced control capabilities.
