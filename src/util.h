#pragma once
#include <vector>
#include <fstream>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <cstdlib>
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

inline string tts_preproc(string s) {
    replace(all(s),'\n',' ');
    return s;
}

inline void tts_generate(const string &text, const string &output_file) {
    string piper_cmd = "echo \""+tts_preproc(text)+"\" | ./piper/piper --model piper/en_US-lessac-medium.onnx --output_file "+output_file+" --quiet";
    system(piper_cmd.c_str());
}

inline double tts_dur(const string &s) {
    // Generate temporary WAV via Piper
    string in_file = "out/ttsdur.wav";
    string out_file = "out/ttsdur_trim.wav";
    tts_generate(s, in_file);

    // Remove trailing silence using ffmpeg's silenceremove filter
    // We only target the end (stop_*) parameters so the beginning remains untouched.
    string trim_cmd = "ffmpeg -y -i "+in_file+" -af silenceremove=start_periods=0:stop_periods=1:stop_duration=0.05:stop_threshold=-70dB "+out_file+" -loglevel quiet";
    system(trim_cmd.c_str());

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
