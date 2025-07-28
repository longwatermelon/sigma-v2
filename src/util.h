#pragma once
#include <vector>
#include <fstream>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <cmath>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
using json=nlohmann::json;
using namespace std;
using namespace cv;
#define sz(x) ((int)size(x))
#define all(x) begin(x),end(x)
template <typename T> using vec=vector<T>;
template <typename T> void vprint(T st, T nd) {auto it=st;while (next(it)!=nd){cout<<*it<<' ';it=next(it);}cout<<*it<<'\n';}

#define W 1080
#define H 1920
#define R1 228
#define R2 1682
#define FPS 30

// frame to time
inline float frm2t(int frame) {
    return (float)frame/FPS;
}

// time to frame
inline int t2frm(float t) {
    return t*FPS;
}

inline bool fix_wav_header(const std::string &filename) {
    // Fix OpenAI's corrupted WAV headers where chunk size is set to 0xFFFFFFFF
    std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        std::cerr << "Error opening WAV file for fixing: " << filename << std::endl;
        return false;
    }

    // Get actual file size
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read and verify RIFF header
    char riff[4];
    file.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) {
        std::cerr << "Not a valid RIFF file: " << filename << std::endl;
        return false;
    }

    // Calculate and write correct file size (total file size - 8 bytes for RIFF header)
    uint32_t correct_file_size = static_cast<uint32_t>(file_size - 8);
    file.write(reinterpret_cast<const char*>(&correct_file_size), sizeof(correct_file_size));

    // Skip WAVE header
    file.seekg(12, std::ios::beg);

    // Find and fix the data chunk
    char chunkId[4];
    uint32_t chunkSize;
    while (file.read(chunkId, 4)) {
        std::streampos chunk_size_pos = file.tellg();
        file.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
        
        if (std::strncmp(chunkId, "data", 4) == 0) {
            // Found data chunk - check if it has the corrupted size
            if (chunkSize == 0xFFFFFFFF) {
                // Calculate correct data size (remaining file size - current position)
                std::streampos data_start = file.tellg();
                uint32_t correct_data_size = static_cast<uint32_t>(file_size - data_start);
                
                // Write the correct chunk size
                file.seekp(chunk_size_pos);
                file.write(reinterpret_cast<const char*>(&correct_data_size), sizeof(correct_data_size));
                file.flush();
                
                // printf("Fixed WAV header: %s (data size: %u bytes)\n", filename.c_str(), correct_data_size);
                return true;
            } else {
                // Chunk size looks normal, no fix needed
                return true;
            }
        } else {
            // Skip this chunk
            file.seekg(chunkSize, std::ios::cur);
        }
    }
    
    std::cerr << "Data chunk not found in WAV file: " << filename << std::endl;
    return false;
}

inline double wav_dur(const std::string &filename) {
    // Open the file in binary mode
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return -1;
    }

    // Read and verify the RIFF header
    char riff[4];
    file.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) {
        std::cerr << "Not a valid RIFF file." << std::endl;
        return -1;
    }

    // Skip the next 4 bytes (overall file size) and read the WAVE header
    file.seekg(4, std::ios::cur);
    char wave[4];
    file.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0) {
        std::cerr << "Not a valid WAVE file." << std::endl;
        return -1;
    }

    // Read chunks until the "fmt " chunk is found.
    char chunkId[4];
    uint32_t chunkSize = 0;
    bool fmtFound = false;
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            fmtFound = true;
            break;
        }
        // Skip this chunk (its content) if not "fmt "
        file.seekg(chunkSize, std::ios::cur);
    }
    if (!fmtFound) {
        std::cerr << "\"fmt \" chunk not found." << std::endl;
        return -1;
    }

    // Read the fmt chunk data:
    // - audioFormat (2 bytes)
    // - numChannels (2 bytes)
    // - sampleRate (4 bytes)
    // - byteRate (4 bytes)
    // - blockAlign (2 bytes)
    // - bitsPerSample (2 bytes)
    uint16_t audioFormat, numChannels, blockAlign, bitsPerSample;
    uint32_t sampleRate, byteRate;
    file.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
    file.read(reinterpret_cast<char*>(&numChannels), sizeof(numChannels));
    file.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
    file.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
    file.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
    file.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));
    
    // If fmt chunk has extra data, skip it
    if (chunkSize > 16) {
        file.seekg(chunkSize - 16, std::ios::cur);
    }

    // Now search for the "data" chunk, which holds the sound samples.
    bool dataFound = false;
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
        if (std::strncmp(chunkId, "data", 4) == 0) {
            dataFound = true;
            break;
        }
        // Skip any extra chunks that are not "data"
        file.seekg(chunkSize, std::ios::cur);
    }
    if (!dataFound) {
        std::cerr << "\"data\" chunk not found." << std::endl;
        return -1;
    }

    // At this point, chunkSize contains the size (in bytes) of the actual audio data.
    uint32_t dataSize = chunkSize;

    // Calculate duration in seconds:
    // Duration = (data size in bytes) / (byte rate in bytes per second)
    double durationSeconds = static_cast<double>(dataSize) / byteRate;
    return durationSeconds;
}

inline bool trim_wav_silence(const std::string &input_file, const std::string &output_file, double silence_threshold_db = -30.0) {
    // Open input file in binary mode
    std::ifstream file(input_file, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening input file: " << input_file << std::endl;
        return false;
    }

    // Read and verify the RIFF header
    char riff[4];
    file.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) {
        std::cerr << "Not a valid RIFF file." << std::endl;
        return false;
    }

    // Read file size
    uint32_t file_size;
    file.read(reinterpret_cast<char*>(&file_size), sizeof(file_size));

    // Read WAVE header
    char wave[4];
    file.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0) {
        std::cerr << "Not a valid WAVE file." << std::endl;
        return false;
    }

    // Store the position after RIFF/WAVE headers
    std::streampos fmt_start = file.tellg();

    // Find and read fmt chunk
    char chunkId[4];
    uint32_t chunkSize = 0;
    bool fmtFound = false;
    uint16_t audioFormat, numChannels, blockAlign, bitsPerSample;
    uint32_t sampleRate, byteRate;
    
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
            file.read(reinterpret_cast<char*>(&numChannels), sizeof(numChannels));
            file.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
            file.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
            file.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
            file.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));
            
            // Skip any extra fmt data
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }
            fmtFound = true;
            break;
        }
        file.seekg(chunkSize, std::ios::cur);
    }
    
    if (!fmtFound) {
        std::cerr << "\"fmt \" chunk not found." << std::endl;
        return false;
    }

    // Find data chunk
    bool dataFound = false;
    std::streampos data_start;
    uint32_t dataSize;
    
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
        if (std::strncmp(chunkId, "data", 4) == 0) {
            dataSize = chunkSize;
            data_start = file.tellg();
            dataFound = true;
            break;
        }
        file.seekg(chunkSize, std::ios::cur);
    }
    
    if (!dataFound) {
        std::cerr << "\"data\" chunk not found." << std::endl;
        return false;
    }

    // Read all audio data
    std::vector<char> audioData(dataSize);
    file.read(audioData.data(), dataSize);
    file.close();

    // Convert silence threshold from dB to linear scale
    double silence_threshold = std::pow(10.0, silence_threshold_db / 20.0);
    
    // Find the last non-silent sample - trim immediately at any quiet point
    uint32_t samples_per_channel = dataSize / blockAlign;
    uint32_t last_non_silent_sample = samples_per_channel;
    
    // Work backwards from the end to find last non-silent sample
    for (int32_t i = samples_per_channel - 1; i >= 0; i--) {
        double max_amplitude = 0.0;
        
        // Check all channels for this sample
        for (uint16_t ch = 0; ch < numChannels; ch++) {
            uint32_t sample_offset = i * blockAlign + ch * (bitsPerSample / 8);
            
            double amplitude = 0.0;
            if (bitsPerSample == 16) {
                int16_t sample = *reinterpret_cast<int16_t*>(&audioData[sample_offset]);
                amplitude = std::abs(sample) / 32767.0;
            } else if (bitsPerSample == 24) {
                // 24-bit samples are stored as 3 bytes, little endian
                int32_t sample = 0;
                sample |= (audioData[sample_offset] & 0xFF);
                sample |= (audioData[sample_offset + 1] & 0xFF) << 8;
                sample |= (audioData[sample_offset + 2] & 0xFF) << 16;
                // Sign extend for 24-bit
                if (sample & 0x800000) sample |= 0xFF000000;
                amplitude = std::abs(sample) / 8388607.0;
            } else if (bitsPerSample == 32) {
                int32_t sample = *reinterpret_cast<int32_t*>(&audioData[sample_offset]);
                amplitude = std::abs(sample) / 2147483647.0;
            }
            
            max_amplitude = std::max(max_amplitude, amplitude);
        }
        
        // Immediately trim at the first non-silent sample we find
        if (max_amplitude > silence_threshold) {
            last_non_silent_sample = i + 1; // +1 because we want to include this sample
            break;
        }
    }
    
    // Calculate new data size
    uint32_t new_data_size = last_non_silent_sample * blockAlign;
    
    // If no trimming is needed, just copy the file
    if (new_data_size >= dataSize) {
        std::ifstream src(input_file, std::ios::binary);
        std::ofstream dst(output_file, std::ios::binary);
        dst << src.rdbuf();
        return true;
    }
    
    // Write the trimmed WAV file
    std::ofstream out_file(output_file, std::ios::binary);
    if (!out_file) {
        std::cerr << "Error creating output file: " << output_file << std::endl;
        return false;
    }
    
    // Write RIFF header
    out_file.write("RIFF", 4);
    uint32_t new_file_size = 36 + new_data_size; // 36 bytes for headers + data
    out_file.write(reinterpret_cast<const char*>(&new_file_size), sizeof(new_file_size));
    out_file.write("WAVE", 4);
    
    // Write fmt chunk
    out_file.write("fmt ", 4);
    uint32_t fmt_size = 16;
    out_file.write(reinterpret_cast<const char*>(&fmt_size), sizeof(fmt_size));
    out_file.write(reinterpret_cast<const char*>(&audioFormat), sizeof(audioFormat));
    out_file.write(reinterpret_cast<const char*>(&numChannels), sizeof(numChannels));
    out_file.write(reinterpret_cast<const char*>(&sampleRate), sizeof(sampleRate));
    out_file.write(reinterpret_cast<const char*>(&byteRate), sizeof(byteRate));
    out_file.write(reinterpret_cast<const char*>(&blockAlign), sizeof(blockAlign));
    out_file.write(reinterpret_cast<const char*>(&bitsPerSample), sizeof(bitsPerSample));
    
    // Write data chunk
    out_file.write("data", 4);
    out_file.write(reinterpret_cast<const char*>(&new_data_size), sizeof(new_data_size));
    out_file.write(audioData.data(), new_data_size);
    
    out_file.close();
    return true;
}

// Helper for CURL response
inline static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

inline string tts_preproc(string s) {
    replace(all(s),'\n',' ');
    return s;
}

inline void tts_generate(const string &text, const string &output_file) {
    string api_key = getenv("SIGMA_CENTRAL_API_KEY") ? getenv("SIGMA_CENTRAL_API_KEY") : "";
    if (api_key.empty()) {
        throw std::runtime_error("SIGMA_CENTRAL_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for OpenAI TTS");
    }

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    json request_body = {
        {"model", "gpt-4o-mini-tts"},
        {"input", tts_preproc(text)},
        {"voice", "onyx"},
        {"response_format", "wav"},
        {"instructions", "You are FURIOUS and EXPOSING the truth. Speak with seething anger and absolute authority, like a whistleblower who's been silenced for too long. Voice: Deep, intense, and commanding with barely-controlled rage. Delivery: Sharp, aggressive, and accusatory - every word is a weapon against the system. Build intensity throughout each sentence, emphasizing key words with growling contempt. Sound like you're personally offended by the lies and deception. Pronunciation: Bite consonants hard, stretch important words for emphasis, and let your anger fuel the passion behind every revelation."}
    };

    std::string request_str = request_body.dump();

    string url = "https://api.openai.com/v1/audio/speech";
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    CURLcode res = curl_easy_perform(curl);
    
    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("OpenAI TTS API request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Check HTTP status code
    if (http_code != 200) {
        std::cerr << "OpenAI TTS API returned HTTP " << http_code << std::endl;
        std::cerr << "Response: " << response_data.substr(0, 500) << std::endl;
        
        // Handle specific HTTP error codes
        if (http_code == 429) {
            throw std::runtime_error("OpenAI TTS API rate limit exceeded. Please wait before retrying.");
        } else if (http_code == 401) {
            throw std::runtime_error("OpenAI TTS API authentication failed. Check your API key.");
        } else if (http_code == 400) {
            // Bad request - likely due to invalid text
            std::cerr << "Text that caused error: \"" << text << "\"" << std::endl;
        }
    }

    // Check if we got a valid response
    if (response_data.empty()) {
        throw std::runtime_error("OpenAI TTS API returned empty response");
    }
    
    // Check for JSON error response (which would start with '{')
    if (!response_data.empty() && response_data[0] == '{') {
        // Try to parse as JSON to get error message
        try {
            json error_response = json::parse(response_data);
            std::string error_msg = "OpenAI TTS API error: ";
            if (error_response.contains("error")) {
                if (error_response["error"].contains("message")) {
                    error_msg += error_response["error"]["message"].get<std::string>();
                } else {
                    error_msg += error_response["error"].dump();
                }
            } else {
                error_msg += response_data;
            }
            throw std::runtime_error(error_msg);
        } catch (const json::parse_error&) {
            throw std::runtime_error("OpenAI TTS API returned unexpected response: " + response_data.substr(0, 100));
        }
    }

    // Write the audio data to file
    std::ofstream audio_file(output_file, std::ios::binary);
    if (!audio_file) {
        throw std::runtime_error("Failed to create output file: " + output_file);
    }
    audio_file.write(response_data.c_str(), response_data.size());
    audio_file.close();
    
    // Verify the file was written successfully
    std::ifstream verify_file(output_file, std::ios::binary | std::ios::ate);
    if (!verify_file.good() || verify_file.tellg() == 0) {
        throw std::runtime_error("Failed to write audio data to file: " + output_file);
    }
    verify_file.close();
    
    // Fix OpenAI's corrupted WAV header (chunk size = 0xFFFFFFFF)
    if (!fix_wav_header(output_file)) {
        throw std::runtime_error("Failed to fix WAV header for file: " + output_file);
    }
    
    // Trim silence from the end using FFmpeg - conservative trimming
    string temp_file = output_file + "_trimmed.wav";
    string cmd = "ffmpeg -i \"" + output_file + "\" -af \"silenceremove=stop_periods=-1:stop_duration=0.05:stop_threshold=-30dB\" -y \"" + temp_file + "\" 2>/dev/null";
    
    int result = system(cmd.c_str());
    
    if (result == 0) {
        system(("mv \"" + temp_file + "\" \"" + output_file + "\"").c_str());
    } else {
        // If FFmpeg fails, clean up temp file
        system(("rm -f \"" + temp_file + "\"").c_str());
    }
}

inline void tts_generate_no_trim(const string &text, const string &output_file) {
    string api_key = getenv("SIGMA_CENTRAL_API_KEY") ? getenv("SIGMA_CENTRAL_API_KEY") : "";
    if (api_key.empty()) {
        throw std::runtime_error("SIGMA_CENTRAL_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for OpenAI TTS");
    }

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    json request_body = {
        {"model", "gpt-4o-mini-tts"},
        {"input", tts_preproc(text)},
        {"voice", "onyx"},
        {"response_format", "wav"},
        {"instructions", "You are FURIOUS and EXPOSING the truth. Speak with seething anger and absolute authority, like a whistleblower who's been silenced for too long. Voice: Deep, intense, and commanding with barely-controlled rage. Delivery: Sharp, aggressive, and accusatory - every word is a weapon against the system. Build intensity throughout each sentence, emphasizing key words with growling contempt. Sound like you're personally offended by the lies and deception. Pronunciation: Bite consonants hard, stretch important words for emphasis, and let your anger fuel the passion behind every revelation."}
    };

    std::string request_str = request_body.dump();

    string url = "https://api.openai.com/v1/audio/speech";
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    CURLcode res = curl_easy_perform(curl);
    
    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("OpenAI TTS API request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Check HTTP status code
    if (http_code != 200) {
        std::cerr << "OpenAI TTS API returned HTTP " << http_code << std::endl;
        std::cerr << "Response: " << response_data.substr(0, 500) << std::endl;
        
        // Handle specific HTTP error codes
        if (http_code == 429) {
            throw std::runtime_error("OpenAI TTS API rate limit exceeded. Please wait before retrying.");
        } else if (http_code == 401) {
            throw std::runtime_error("OpenAI TTS API authentication failed. Check your API key.");
        } else if (http_code == 400) {
            // Bad request - likely due to invalid text
            std::cerr << "Text that caused error: \"" << text << "\"" << std::endl;
        }
    }

    // Check if we got a valid response
    if (response_data.empty()) {
        throw std::runtime_error("OpenAI TTS API returned empty response");
    }
    
    // Check for JSON error response (which would start with '{')
    if (!response_data.empty() && response_data[0] == '{') {
        // Try to parse as JSON to get error message
        try {
            json error_response = json::parse(response_data);
            std::string error_msg = "OpenAI TTS API error: ";
            if (error_response.contains("error")) {
                if (error_response["error"].contains("message")) {
                    error_msg += error_response["error"]["message"].get<std::string>();
                } else {
                    error_msg += error_response["error"].dump();
                }
            } else {
                error_msg += response_data;
            }
            throw std::runtime_error(error_msg);
        } catch (const json::parse_error&) {
            throw std::runtime_error("OpenAI TTS API returned unexpected response: " + response_data.substr(0, 100));
        }
    }

    // Write the audio data to file
    std::ofstream audio_file(output_file, std::ios::binary);
    if (!audio_file) {
        throw std::runtime_error("Failed to create output file: " + output_file);
    }
    audio_file.write(response_data.c_str(), response_data.size());
    audio_file.close();
    
    // Verify the file was written successfully
    std::ifstream verify_file(output_file, std::ios::binary | std::ios::ate);
    if (!verify_file.good() || verify_file.tellg() == 0) {
        throw std::runtime_error("Failed to write audio data to file: " + output_file);
    }
    verify_file.close();
    
    // Fix OpenAI's corrupted WAV header (chunk size = 0xFFFFFFFF) - but don't trim
    if (!fix_wav_header(output_file)) {
        throw std::runtime_error("Failed to fix WAV header for file: " + output_file);
    }
}

inline string tts_generate_persistent(const string &text) {
    // Generate unique filename for this TTS audio
    static int tts_counter = 0;
    string wav_file = "out/tts_" + to_string(tts_counter++) + ".wav";
    
    // Generate TTS audio (trimmed version for playback)
    tts_generate(text, wav_file);
    
    return wav_file;
}

inline void tts_generate_dialogue(const string &text, const string &output_file, const string &speaker) {
    string api_key = getenv("SIGMA_CENTRAL_API_KEY") ? getenv("SIGMA_CENTRAL_API_KEY") : "";
    if (api_key.empty()) {
        throw std::runtime_error("SIGMA_CENTRAL_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for OpenAI TTS");
    }

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    string instructions, voice="onyx";
    if (speaker == "BATEMAN") {
        instructions = "You are Patrick Bateman - confident, smug, and intellectually superior. Speak with calm authority and barely-contained arrogance. Your voice is deep, measured, and dripping with condescension. You know you're right and everyone else is beneath you. Deliver facts like weapons, with perfect pronunciation and a slight smirk in your voice.";
        voice = "onyx";
    } else if (speaker == "CHUDJAK") {
        instructions = "You are Chudjak - anxious, paranoid, and unhinged. Your voice is shaky, high-pitched, and filled with nervous energy. Speak with frantic desperation and disbelief. Sound like you're constantly on edge, questioning everything with fearful incredulity. Voice should crack with anxiety and sound perpetually panicked.";
        voice = "echo";
    } else {
        // Default to conspiracy voice
        instructions = "You are FURIOUS and EXPOSING the truth. Speak with seething anger and absolute authority, like a whistleblower who's been silenced for too long. Voice: Deep, intense, and commanding with barely-controlled rage. Delivery: Sharp, aggressive, and accusatory - every word is a weapon against the system. Build intensity throughout each sentence, emphasizing key words with growling contempt. Sound like you're personally offended by the lies and deception. Pronunciation: Bite consonants hard, stretch important words for emphasis, and let your anger fuel the passion behind every revelation.";
    }

    json request_body = {
        {"model", "gpt-4o-mini-tts"},
        {"input", tts_preproc(text)},
        {"voice", voice},
        {"response_format", "wav"},
        {"instructions", instructions}
    };

    std::string request_str = request_body.dump();

    string url = "https://api.openai.com/v1/audio/speech";
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    CURLcode res = curl_easy_perform(curl);
    
    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("OpenAI TTS API request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Check HTTP status code
    if (http_code != 200) {
        std::cerr << "OpenAI TTS API returned HTTP " << http_code << std::endl;
        std::cerr << "Response: " << response_data.substr(0, 500) << std::endl;
        
        // Handle specific HTTP error codes
        if (http_code == 429) {
            throw std::runtime_error("OpenAI TTS API rate limit exceeded. Please wait before retrying.");
        } else if (http_code == 401) {
            throw std::runtime_error("OpenAI TTS API authentication failed. Check your API key.");
        } else if (http_code == 400) {
            // Bad request - likely due to invalid text
            std::cerr << "Text that caused error: \"" << text << "\"" << std::endl;
        }
    }

    // Check if we got a valid response
    if (response_data.empty()) {
        throw std::runtime_error("OpenAI TTS API returned empty response");
    }
    
    // Check for JSON error response (which would start with '{')
    if (!response_data.empty() && response_data[0] == '{') {
        // Try to parse as JSON to get error message
        try {
            json error_response = json::parse(response_data);
            std::string error_msg = "OpenAI TTS API error: ";
            if (error_response.contains("error")) {
                if (error_response["error"].contains("message")) {
                    error_msg += error_response["error"]["message"].get<std::string>();
                } else {
                    error_msg += error_response["error"].dump();
                }
            } else {
                error_msg += response_data;
            }
            throw std::runtime_error(error_msg);
        } catch (const json::parse_error&) {
            throw std::runtime_error("OpenAI TTS API returned unexpected response: " + response_data.substr(0, 100));
        }
    }

    // Write the audio data to file
    std::ofstream audio_file(output_file, std::ios::binary);
    if (!audio_file) {
        throw std::runtime_error("Failed to create output file: " + output_file);
    }
    audio_file.write(response_data.c_str(), response_data.size());
    audio_file.close();
    
    // Verify the file was written successfully
    std::ifstream verify_file(output_file, std::ios::binary | std::ios::ate);
    if (!verify_file.good() || verify_file.tellg() == 0) {
        throw std::runtime_error("Failed to write audio data to file: " + output_file);
    }
    verify_file.close();
    
    // Fix OpenAI's corrupted WAV header (chunk size = 0xFFFFFFFF)
    if (!fix_wav_header(output_file)) {
        throw std::runtime_error("Failed to fix WAV header for file: " + output_file);
    }
    
    // Trim silence from the end using FFmpeg - conservative trimming
    string temp_file = output_file + "_trimmed.wav";
    string cmd = "ffmpeg -i \"" + output_file + "\" -af \"silenceremove=stop_periods=-1:stop_duration=0.02:stop_threshold=-30dB\" -y \"" + temp_file + "\" 2>/dev/null";
    
    int result = system(cmd.c_str());
    
    if (result == 0) {
        system(("mv \"" + temp_file + "\" \"" + output_file + "\"").c_str());
    } else {
        // If FFmpeg fails, clean up temp file
        system(("rm -f \"" + temp_file + "\"").c_str());
    }
}

inline string tts_generate_persistent_dialogue(const string &text, const string &speaker) {
    // Generate unique filename for this TTS audio
    static int tts_counter = 0;
    string wav_file = "out/tts_" + to_string(tts_counter++) + ".wav";
    
    // Generate TTS audio (trimmed version for playbook)
    tts_generate_dialogue(text, wav_file, speaker);
    
    return wav_file;
}

inline double tts_dur(const string &s) {
    // Generate temporary WAV without trimming for accurate duration measurement
    string wav_file = "out/ttsdur.wav";
    tts_generate_no_trim(s, wav_file);

    // Verify the WAV file was created
    std::ifstream test_file(wav_file);
    if (!test_file.good()) {
        std::cerr << "WAV file was not created in tts_dur: " << wav_file << std::endl;
        test_file.close();
        return 1.0; // Default 1 second duration
    }
    test_file.close();

    // Measure duration of the untrimmed WAV file
    double res = wav_dur(wav_file);

    // Clean up temporary file
    system(("rm " + wav_file).c_str());

    // Check if wav_dur failed and return a reasonable default
    if (res < 0) {
        std::cerr << "Failed to measure WAV duration, using default duration" << std::endl;
        return 1.0; // Default 1 second duration
    }

    return res;
}

inline std::string openai_req(const std::string& model, const std::string& prompt, const json& response_format = json()) {
    std::string api_key = std::getenv("SIGMA_CENTRAL_API_KEY");
    if (api_key.empty()) {
        throw std::runtime_error("SIGMA_CENTRAL_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    json request_body = {
        {"model", model},
        {"messages", {
            {
                {"role", "user"},
                {"content", prompt}
            }
        }}
    };
    
    // Add response_format if provided
    if (!response_format.is_null()) {
        request_body["response_format"] = response_format;
    }

    std::string request_str = request_body.dump();

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    json response_json = json::parse(response_data);
    try {
        return response_json["choices"][0]["message"]["content"];
    } catch (...) {
        throw std::runtime_error("Unexpected API response format: " + response_data);
    }
}

inline std::string anthropic_req(const std::string& prompt, const json& response_format = json()) {
    std::string api_key = std::getenv("SIGMA_CENTRAL_ANTHROPIC_API_KEY");
    if (api_key.empty()) {
        throw std::runtime_error("SIGMA_CENTRAL_ANTHROPIC_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for Anthropic");
    }

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("x-api-key: " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    json request_body = {
        {"model", "claude-3-haiku-20240307"},
        {"max_tokens", 4000},
        {"messages", {
            {
                {"role", "user"},
                {"content", prompt}
            }
        }}
    };

    std::string request_str = request_body.dump();

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    CURLcode res = curl_easy_perform(curl);
    
    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("Anthropic API request failed: " + std::string(curl_easy_strerror(res)));
    }

    if (http_code != 200) {
        std::cerr << "Anthropic API returned HTTP " << http_code << std::endl;
        std::cerr << "Response: " << response_data.substr(0, 500) << std::endl;
        throw std::runtime_error("Anthropic API error: HTTP " + std::to_string(http_code));
    }

    json response_json = json::parse(response_data);
    try {
        return response_json["content"][0]["text"];
    } catch (...) {
        throw std::runtime_error("Unexpected Anthropic API response format: " + response_data);
    }
}

inline std::vector<std::string> split_on_delimiter(const std::string& input, const std::string& delimiter = "====") {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end;
    while ((end = input.find(delimiter, start)) != std::string::npos) {
        parts.push_back(input.substr(start, end - start));
        start = end + delimiter.length();
    }
    // Add the last part
    parts.push_back(input.substr(start));
    return parts;
}

struct WhisperSegment {
    float start;
    float end;
    string text;
};

inline std::vector<WhisperSegment> whisper_transcribe(const std::string& audio_file) {
    std::string api_key = std::getenv("SIGMA_CENTRAL_API_KEY");
    if (api_key.empty()) {
        throw std::runtime_error("SIGMA_CENTRAL_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for Whisper API");
    }

    std::string response_data;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    // Create multipart form data
    curl_mime* form = curl_mime_init(curl);
    curl_mimepart* field;

    // Add audio file
    field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, audio_file.c_str());

    // Add model
    field = curl_mime_addpart(form);
    curl_mime_name(field, "model");
    curl_mime_data(field, "whisper-1", CURL_ZERO_TERMINATED);

    // Add response format for timestamps
    field = curl_mime_addpart(form);
    curl_mime_name(field, "response_format");
    curl_mime_data(field, "verbose_json", CURL_ZERO_TERMINATED);

    // Add timestamp granularities for word-level timing
    field = curl_mime_addpart(form);
    curl_mime_name(field, "timestamp_granularities[]");
    curl_mime_data(field, "word", CURL_ZERO_TERMINATED);

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/audio/transcriptions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    CURLcode res = curl_easy_perform(curl);
    
    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_mime_free(form);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("OpenAI Whisper API request failed: " + std::string(curl_easy_strerror(res)));
    }

    if (http_code != 200) {
        std::cerr << "OpenAI Whisper API returned HTTP " << http_code << std::endl;
        std::cerr << "Response: " << response_data.substr(0, 500) << std::endl;
        throw std::runtime_error("OpenAI Whisper API error: HTTP " + std::to_string(http_code));
    }

    // Parse JSON response for word-level timing
    std::vector<WhisperSegment> segments;
    try {
        json response_json = json::parse(response_data);
        
        if (response_json.contains("words")) {
            for (const auto& word : response_json["words"]) {
                WhisperSegment ws;
                ws.start = word["start"].get<float>();
                ws.end = word["end"].get<float>();
                ws.text = word["word"].get<std::string>();
                
                // Trim whitespace from text
                size_t start_pos = ws.text.find_first_not_of(" \t\n\r");
                if (start_pos != std::string::npos) {
                    size_t end_pos = ws.text.find_last_not_of(" \t\n\r");
                    ws.text = ws.text.substr(start_pos, end_pos - start_pos + 1);
                }
                
                // Only add non-empty words
                if (!ws.text.empty()) {
                    segments.push_back(ws);
                }
            }
        } else {
            std::cerr << "No words found in Whisper response" << std::endl;
        }
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse Whisper API response: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("Error processing Whisper API response: " + std::string(e.what()));
    }

    return segments;
}
