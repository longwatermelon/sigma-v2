#!/bin/bash

# setup-piper.sh - Download and setup Piper TTS for sigma-v2 project
# This script downloads Piper TTS and the required voice model for fresh git clones

set -e  # Exit on any error

PIPER_VERSION="2023.11.14-2"
PIPER_URL="https://github.com/rhasspy/piper/releases/download/${PIPER_VERSION}/piper_linux_x86_64.tar.gz"
VOICE_MODEL_URL="https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/lessac/medium/en_US-lessac-medium.onnx"
VOICE_CONFIG_URL="https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json"

echo "Setting up Piper TTS for sigma-v2..."

# Check if piper directory already exists
if [ -d "piper" ]; then
    echo "Piper directory already exists. Skipping download."
    exit 0
fi

# Create temporary directory for downloads
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

echo "Downloading Piper TTS..."
wget -q --show-progress "$PIPER_URL" -O "$TEMP_DIR/piper_linux_x86_64.tar.gz"

echo "Extracting Piper TTS..."
tar -xzf "$TEMP_DIR/piper_linux_x86_64.tar.gz" -C "$TEMP_DIR"

# Move extracted files to piper directory
mv "$TEMP_DIR/piper" ./

echo "Downloading voice model (en_US-lessac-medium)..."
wget -q --show-progress "$VOICE_MODEL_URL" -O "piper/en_US-lessac-medium.onnx"
wget -q --show-progress "$VOICE_CONFIG_URL" -O "piper/en_US-lessac-medium.onnx.json"

# Make piper executable
chmod +x piper/piper

echo "Testing Piper installation..."
if echo "Piper TTS setup complete" | ./piper/piper --model piper/en_US-lessac-medium.onnx --output_file test_setup.wav > /dev/null 2>&1; then
    rm -f test_setup.wav
    echo "✓ Piper TTS setup completed successfully!"
    echo "✓ Voice model: en_US-lessac-medium"
    echo "✓ Executable: ./piper/piper"
else
    echo "✗ Piper TTS test failed"
    exit 1
fi

echo "Done! You can now use Piper TTS in your sigma-v2 project."