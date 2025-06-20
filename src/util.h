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

inline string tts_preproc(string s) {
    replace(all(s),'\n',' ');
    return s;
}

inline void tts_generate(const string &text, const string &output_file) {
    string piper_cmd = "echo \""+tts_preproc(text)+"\" | ./piper/piper --model piper/en_US-lessac-medium.onnx --output_file "+output_file+" --quiet >/dev/null 2>&1";
    system(piper_cmd.c_str());
}

inline double tts_dur(const string &s) {
    // Generate temporary WAV via Piper
    string in_file = "out/ttsdur.wav";
    string out_file = "out/ttsdur_trim.wav";
    tts_generate(s, in_file);

    // Remove trailing silence using C++ instead of ffmpeg
    if (!trim_wav_silence(in_file, out_file, -35.0)) {
        std::cerr << "Failed to trim silence, using original file" << std::endl;
        // Copy original file as fallback
        std::ifstream src(in_file, std::ios::binary);
        std::ofstream dst(out_file, std::ios::binary);
        dst << src.rdbuf();
    }

    // Measure duration of the trimmed audio
    double res = wav_dur(out_file);

    // Clean up temporary files
    system(("rm "+in_file+" "+out_file).c_str());

    return res;
}

// Helper for CURL response
inline static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

inline std::string openai_req(const std::string& model, const std::string& prompt) {
    std::string api_key = std::getenv("OPENAI_API_KEY");
    if (api_key.empty()) {
        throw std::runtime_error("OPENAI_API_KEY environment variable not set");
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
