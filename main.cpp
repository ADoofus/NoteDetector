#include "gui.hpp"
#include "fourierTrans/fourier.cpp"
#include "filedialog/tinyfiledialogs.h"
#include "AudioFile.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "implot/implot.h"
#include "implot/implot_internal.h"

#include "pianoVisual/imgui_piano.h"

#include "imageLoader/imageLoader.cpp"


#include "GLFW/include/glfw3.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <map>


int len = 0;
int sampleRate = 0;
double duration = 0;
int numSamples = 0;
int downsampleRate = 10;
double peakFrequency = 0;
std::string detectedNote = "";
std::vector<double> intensity;
std::vector<double> fIntensity;
std::vector<double> fxAxis;
std::vector<double> xAxis;

// static bool KeyPressed[128] = {};
// static int PrevNoteActive = -1;

const char* filters[] = { "*.wav", "*.aiff", "*.jpg", "*.*" };


std::vector<double> LowPassFilter(const std::vector<double>& input, int windowSize) {
    std::vector<double> output(input.size());
    int halfWindow = windowSize / 2;

    for (size_t i = 0; i < input.size(); ++i) {
        double sum = 0;
        int count = 0;
        for (int j = -halfWindow; j <= halfWindow; ++j) {
            if (i + j >= 0 && i + j < input.size()) {
                sum += input[i + j];
                ++count;
            }
        }
        output[i] = sum / count;
    }
    return output;
}

std::string FrequencyToNoteName(double frequency) {
    if (frequency <= 0) return "N/A";

    static const std::string noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    double A4 = 440.0;
    int midiNumber = int(std::round(12 * std::log2(frequency / A4))) + 69;
    int octave = midiNumber / 12 - 1;
    int noteIndex = midiNumber % 12;

    if (midiNumber < 0 || midiNumber > 127) return "Out of Range";
    return noteNames[noteIndex] + std::to_string(octave);
}


std::vector<double> AnalyzeAudio(const std::string& path, int* len, int* sampleRate) {
    std::cout << "**********************" << std::endl;
    std::cout << "Running Example: Load Audio File and Print Summary" << std::endl;
    std::cout << "**********************" << std::endl;

    AudioFile<double> audioFile;
    if (!audioFile.load(path)) {
        std::cerr << "Failed to load file: " << path << std::endl;
        *len = 0;
        return {};
    }

    std::cout << "Bit Depth: " << audioFile.getBitDepth() << std::endl;
    std::cout << "Sample Rate: " << audioFile.getSampleRate() << std::endl;
    std::cout << "Num Channels: " << audioFile.getNumChannels() << std::endl;
    std::cout << "Length in Seconds: " << audioFile.getLengthInSeconds() << std::endl << std::endl;

    int channel = 0;
    const auto& samples = audioFile.samples[channel];

    numSamples = audioFile.getNumSamplesPerChannel();
    *len = numSamples / downsampleRate;
    *sampleRate = audioFile.getSampleRate();
    duration = audioFile.getLengthInSeconds();

    // // Apply low-pass filter before downsampling
    // auto filtered = LowPassFilter(samples, downsampleRate);

    std::vector<double> intensity(*len);
    for (int i = 0; i < *len; ++i) {
        intensity[i] = samples[i*downsampleRate];
    }

    return intensity;
}

void LoadRigby() {
    int imgWidth = 0, imgHeight = 0;
    
    // Format the text message
    std::string selectedImage = "assets\\rigby.jpg";
    std::string message = "";
    if (len > 0) {
        message = "Me Rigby cat says \n this note is: " + detectedNote;
        selectedImage = "assets\\rigbyResult.jpg";
    }
    GLuint myImageTex = LoadTextureFromFile(selectedImage.c_str(), &imgWidth, &imgHeight);

    ImGui::Begin("The Almighty Rigby", nullptr);
    ImVec2 imagePos = ImGui::GetCursorScreenPos();
    ImVec2 imageSize = ImVec2(imgWidth, imgHeight);
    if (myImageTex) {
        ImGui::Image((void*)(intptr_t)myImageTex, imageSize);
    } else {
        ImGui::Text("Failed to load image.");
    }
    ImGui::SetNextWindowSize(imageSize);


    // Text position
    ImVec2 textPos = ImVec2(imagePos.x + 20, imagePos.y + 30);

    // Font size
    float fontSize = 32.0f;

    // Get draw list
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw stroke (black, slightly offset)
    drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(textPos.x + 1, textPos.y + 1),
                    IM_COL32(0, 0, 0, 255), message.c_str());

    // Draw main text (yellow)
    drawList->AddText(ImGui::GetFont(), fontSize, textPos,
            IM_COL32(255, 255, 0, 255), message.c_str());
    ImGui::End();
}

void OnImport(const char* file) {
    std::cout << "Imported file: " << file << std::endl;
    intensity = AnalyzeAudio(file, &len, &sampleRate);

    std::vector<std::complex<double>> compIntensity(len);
    for (int i = 0; i < len; ++i) {
        double hanning = 0.5 * (1 - std::cos(2 * PI * i / (len - 1)));
        compIntensity[i] = std::complex<double>(intensity[i] * hanning, 0.0);
    }

    FFT(compIntensity);

    fIntensity.resize(len/2);
    for (int i = 0; i < len/2; ++i) {
        fIntensity[i] = (std::norm(compIntensity[i]));
    }

    fxAxis.resize(len/2);
    int effectiveSampleRate = sampleRate / downsampleRate;
    for (int i = 0; i < len/2; ++i) {
        fxAxis[i] = static_cast<double>(effectiveSampleRate) * (static_cast<double>(i)/static_cast<double>(len));
    }

    xAxis.resize(len);
    for (int i = 0; i < len; ++i) {
        xAxis[i] = i / (double)effectiveSampleRate;
    }

    double maxIntensity = 0.0;
    for (int i = 1; i < fIntensity.size(); ++i) {
        if (fIntensity[i] > maxIntensity) {
            maxIntensity = fIntensity[i];
            peakFrequency = fxAxis[i];
        }
    }
    detectedNote = FrequencyToNoteName(peakFrequency);
}

bool SimplePianoCallback(void*, ImGuiExt::Piano::KeyboardMsgType msg, ImGuiExt::Piano::keyCode_t key, float vel) {
    if (key >= 128) return false;
    if (msg == ImGuiExt::Piano::KeyboardMsgType::NoteGetStatus) return KeyPressed[key];
    if (msg == ImGuiExt::Piano::KeyboardMsgType::NoteOn)  KeyPressed[key] = true;
    if (msg == ImGuiExt::Piano::KeyboardMsgType::NoteOff) KeyPressed[key] = false;
    return false;
}

// int NoteNameToKeyIndex(const std::string& note) {
//     static std::map<std::string, int> noteMap = {
//         {"C", 0}, {"C#", 1}, {"D", 2}, {"D#", 3},
//         {"E", 4}, {"F", 5}, {"F#", 6}, {"G", 7},
//         {"G#", 8}, {"A", 9}, {"A#", 10}, {"B", 11}
//     };

//     if (note.length() < 2 || note.length() > 3)
//         return -1;

//     std::string name = note.substr(0, note.length() - 1);
//     int octave = note.back() - '0';

//     if (noteMap.find(name) == noteMap.end() || octave < 0 || octave > 9)
//         return -1;

//     return (octave + 1) * 12 + noteMap[name];
// }

int main() {
    ImPlot::CreateContext();
    GLFWwindow* window = CreateWindowIMGUI(1200, 800, 0, 1);
    if (!window) {
        std::cerr << "Failed to create window!" << std::endl;
        return 0;
    }

    while (!glfwWindowShouldClose(window)) {
        UpdateWindowSize(window);

        ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::Text("Select An Audio File!");

        if (ImGui::Button("Import File")) {
            const char* file = tinyfd_openFileDialog("Select a file", "", 4, filters, "All files", 0);

            if (file) {
                OnImport(file);
            }
        }

        if (len > 0) {
            std::string sampleStr = std::to_string(sampleRate);
            std::string waveformTitle = "Waveform Sampled At " + sampleStr + "Hz";
            std::string spectrumTitle = "Periodogram";

            if (ImPlot::BeginPlot(waveformTitle.c_str())) {
                ImPlot::SetupAxes("Time (Seconds)", "Amplitude");
                ImPlot::PlotLine("Signal", xAxis.data(), intensity.data(), len);
                ImPlot::EndPlot();
            }

            if (ImPlot::BeginPlot(spectrumTitle.c_str())) {
                ImPlot::SetupAxes("Frequency (Hz)", "Intensity");
                ImPlot::PlotLine("FFT", fxAxis.data(), fIntensity.data(), len/2);
                ImPlot::EndPlot();
            }
            // ImGuiExt::Piano::Keyboard("MyPiano", ImVec2(900, 100), ImGuiNoteNameToKeyIndex(detectedNote), 0, 128, SimplePianoCallback);

            ImGui::Separator();
            ImGui::Text("Peak Frequency: %.2f Hz", peakFrequency);
            // ImGui::Text("Detected Note: %s", detectedNote.c_str());
        }

        LoadRigby();

        ImGui::End();
        RenderWindow(window);
    }

    TerminateIMGUI(window);
    return 0;
}
