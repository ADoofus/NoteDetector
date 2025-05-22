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
#include "peakDetector.cpp"

#include "rtAudio/RtAudio.h"

#include "GLFW/include/glfw3.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include "RtAudio.h"

const int downsampleRate = 10;

int len = 0;
int intervalLen = 0;
int sampleRate = 0;
double duration = 0;
int numSamples = 0;
int timeIndex = 0;
double peakFrequency = 0;

std::string detectedNote = "";
std::vector<double> intensity;
std::vector<double> intensityInterval;
std::vector<std::complex<double>> compIntensity;
std::vector<double> fIntensity;
std::vector<double> fxAxis;
std::vector<double> xAxis;
std::vector<double> xAxisInterval;
std::vector<double> peaks;
std::vector<double> plotPeaks;
bool needsAutoFit = false;
bool analyzeFullAudio = false;


//Piano
static bool KeyPressed[128] = {};
static ImGuiExt::Piano::keyCode_t prevNoteActive = ImGuiExt::Piano::keyCodeNone;
static ImGuiExt::Piano::keyCode_t detectedNoteIndex = ImGuiExt::Piano::keyCodeNone;

bool useRealtimeAudio = false;
const unsigned int CHANNELS = 1;
const unsigned int RECORD_SAMPLE_RATE = 41000/downsampleRate;
const float REALTIME_SECONDS = 2.0f;
const unsigned int BUFFER_FRAMES = 256;
const unsigned int CIRCULAR_BUFFER_SIZE = static_cast<unsigned int>(RECORD_SAMPLE_RATE * REALTIME_SECONDS * CHANNELS);

// Circular Recording buffer
std::vector<double> circularBuffer(CIRCULAR_BUFFER_SIZE, 0.0f);
std::atomic<unsigned int> writeIndex(0);
std::mutex bufferMutex;
std::unique_ptr<RtAudio> rtAudio;
RtAudio::StreamParameters inputParams;
bool isRecording = false;
std::vector<double> liveBuffer;

// RtAudio callback
int AudioCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                  double /*streamTime*/, RtAudioStreamStatus status, void * /*userData*/) {
    if (status) std::cerr << "Stream underflow/overflow detected.\n";
    if (!inputBuffer) return 0;

    double *in = static_cast<double *>(inputBuffer);
    for (unsigned int i = 0; i < nBufferFrames; ++i) {
        unsigned int index = (writeIndex + i) % CIRCULAR_BUFFER_SIZE;
        circularBuffer[index] = in[i];
    }
    writeIndex = (writeIndex + nBufferFrames) % CIRCULAR_BUFFER_SIZE;

    return 0;
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
void PeakDetection() {
    double maxIntensity = 0.0;
    peakFrequency = 0.0;
    for (int i = 1; i < static_cast<int>(fIntensity.size()); ++i) {
        if (fIntensity[i] > maxIntensity) {
            maxIntensity = fIntensity[i];
            peakFrequency = fxAxis[i];
        }
    }
    detectedNote = FrequencyToNoteName(peakFrequency);
}
void UpdateLiveBuffer() {
    liveBuffer.resize(CIRCULAR_BUFFER_SIZE);
    for (unsigned int i = 0; i < CIRCULAR_BUFFER_SIZE; ++i) {
        unsigned int index = (writeIndex + i) % CIRCULAR_BUFFER_SIZE;
        liveBuffer[i] = circularBuffer[index];
    }

    compIntensity.resize(CIRCULAR_BUFFER_SIZE);
    for (int i = 0; i < CIRCULAR_BUFFER_SIZE; ++i) {
        double hanning = 0.5 * (1 - std::cos(2 * PI * i / (CIRCULAR_BUFFER_SIZE - 1)));
        compIntensity[i] = std::complex<double>(liveBuffer[i] * hanning, 0.0);
    }
    FFT(compIntensity);

    fIntensity.resize(CIRCULAR_BUFFER_SIZE / 2);
    for (int i = 0; i < CIRCULAR_BUFFER_SIZE / 2; ++i) {
        fIntensity[i] = std::norm(compIntensity[i]);
    }

    PeakDetection();
}

void StartAudio() {
    rtAudio = std::make_unique<RtAudio>();

    if (rtAudio->getDeviceCount() < 1) {
        std::cerr << "No audio devices found!" << std::endl;
        return;
    }

    inputParams.deviceId = rtAudio->getDefaultInputDevice();
    inputParams.nChannels = CHANNELS;
    inputParams.firstChannel = 0;

    RtAudioErrorType err;

    err = rtAudio->openStream(nullptr, &inputParams, RTAUDIO_FLOAT64,
                              RECORD_SAMPLE_RATE, (unsigned int *)&BUFFER_FRAMES,
                              &AudioCallback, nullptr);
    if (err != RTAUDIO_NO_ERROR) {
        std::cerr << "Error opening stream: " << rtAudio->getErrorText() << std::endl;
        return;
    }

    err = rtAudio->startStream();
    if (err != RTAUDIO_NO_ERROR) {
        std::cerr << "Error starting stream: " << rtAudio->getErrorText() << std::endl;
        return;
    }

    isRecording = true;
    std::cout << "Started real-time recording" << std::endl;
}


void RecordAudio(RtAudio &audio, RtAudio::StreamParameters &inputParams) { 
    audio.openStream(nullptr, &inputParams, RTAUDIO_FLOAT64, RECORD_SAMPLE_RATE,
                        (unsigned int *)&BUFFER_FRAMES, &AudioCallback, nullptr);
    audio.startStream();

    std::cout << "Recording in real-time with circular buffer...\n";

    // std::thread reader(UpdateAudio);
}

void StopAudio() {
    if (!rtAudio) return;

    isRecording = false;
    liveBuffer.clear();
    std::cout << "Stopped recording" << std::endl;
}


const char* filters[] = { "*.wav", "*.aiff", "*.jpg", "*.*" };

// Helper to convert time (seconds) to sample index
inline int TimeToIndex(double time) {
    return static_cast<int>(time * sampleRate);
}

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
    std::vector<double> intensity(*len);
    for (int i = 0; i < *len; ++i) {
        intensity[i] = samples[i * downsampleRate];
    }
    return intensity;
}

void UpdateData() {
    if (peaks.empty()) return;

    int startIdx = TimeToIndex(peaks[timeIndex]);
    if (analyzeFullAudio) {startIdx = 0;}
    if (analyzeFullAudio) {intervalLen = len;}

    compIntensity.resize(intervalLen);
    for (int i = 0; i < intervalLen; ++i) {
        double hanning = 0.5 * (1 - std::cos(2 * PI * i / (intervalLen - 1)));
        compIntensity[i] = std::complex<double>(intensity[startIdx + i] * hanning, 0.0);
    }
    FFT(compIntensity);

    int effectiveSampleRate = sampleRate / downsampleRate;

    xAxis.resize(len);
    for (int i = 0; i < len; ++i) {
        xAxis[i] = i / static_cast<double>(effectiveSampleRate);
    }

    xAxisInterval.resize(intervalLen);
    for (int i = 0; i < intervalLen; ++i) {
        xAxisInterval[i] = (startIdx + i) / static_cast<double>(effectiveSampleRate);
    }

    fxAxis.resize(intervalLen / 2);
    for (int i = 0; i < intervalLen / 2; ++i) {
        fxAxis[i] = effectiveSampleRate * (static_cast<double>(i) / intervalLen);
    }

    fIntensity.resize(intervalLen / 2);
    for (int i = 0; i < intervalLen / 2; ++i) {
        fIntensity[i] = std::norm(compIntensity[i]);
    }
}


void OnImport(const char* file) {
    std::cout << "Imported file: " << file << std::endl;
    intensity = AnalyzeAudio(file, &len, &sampleRate);
    timeIndex = 0;

    PeakDetection();

    // detectedPeaks are sample indices; convert to seconds here:
    std::vector<double> detectedPeaks = get_wave_info(intensity).peaks;
    peaks.resize(detectedPeaks.size());
    plotPeaks.resize(detectedPeaks.size());
    for (size_t i = 0; i < detectedPeaks.size(); ++i) {
        peaks[i] = detectedPeaks[i] / static_cast<double>(sampleRate);
        plotPeaks[i] = (detectedPeaks[i] * downsampleRate) / static_cast<double>(sampleRate);

        // std::cout << "Peak at time (seconds): " << peaks[i] << std::endl;
    }

    if (peaks.size() >= 2) {
        intervalLen = TimeToIndex(peaks[1]) - TimeToIndex(peaks[0]);
    } else {
        intervalLen = len;
    }

    intensityInterval.resize(intervalLen);
    int startIdx = TimeToIndex(peaks[timeIndex]);
    for (int i = 0; i < intervalLen; ++i) {
        intensityInterval[i] = intensity[startIdx + i];
    }

    UpdateData();
}

void ChangeInterval(bool forward) {
    int increment = forward ? 1 : -1;
    timeIndex = std::clamp(timeIndex + increment, 0, static_cast<int>(peaks.size()) - 1);

    if (timeIndex + 1 < static_cast<int>(peaks.size())) {
        intervalLen = TimeToIndex(peaks[timeIndex + 1]) - TimeToIndex(peaks[timeIndex]);
    } else {
        intervalLen = len - TimeToIndex(peaks[timeIndex]);
    }

    intensityInterval.resize(intervalLen);
    int startIdx = TimeToIndex(peaks[timeIndex]);
    for (int i = 0; i < intervalLen; ++i) {
        intensityInterval[i] = intensity[startIdx + i];
    }

    UpdateData();
    PeakDetection();

    needsAutoFit = true;
}
void ToggleFullAudio(bool toggle) {
    if (toggle) {
        intervalLen = len;
        timeIndex = 0;
        UpdateData();
        PeakDetection();
    } else {
        ChangeInterval(true);
    }
}

void LoadRigby() {
    int imgWidth = 0, imgHeight = 0;
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
    ImVec2 textPos = ImVec2(imagePos.x + 20, imagePos.y + 30);
    float fontSize = 32.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 255), message.c_str());
    drawList->AddText(ImGui::GetFont(), fontSize, textPos, IM_COL32(255, 255, 0, 255), message.c_str());
    ImGui::End();
}
void ListAudioDevices() {
    RtAudio audio;

    unsigned int deviceCount = audio.getDeviceCount();
    std::cout << "Devices: " << std::endl;

    for (unsigned int i = 0; i < deviceCount; ++i) {
        RtAudio::DeviceInfo info = audio.getDeviceInfo(i);
        std::cout << "Device " << i << ": " << info.name << std::endl;
        std::cout << "Input Channels: " << info.inputChannels << std::endl;
        std::cout << "Output Channels: " << info.outputChannels << std::endl;
        std::cout << "Sample Rates: ";
        for (auto rate : info.sampleRates) {
            std::cout << rate << " ";
        }
        std::cout << std::endl;
    }
}

bool SimplePianoCallback(void* userData, ImGuiExt::Piano::KeyboardMsgType msg, ImGuiExt::Piano::keyCode_t keyCode, float velocity) {
    switch (msg) {
        case ImGuiExt::Piano::KeyboardMsgType::NoteGetStatus:
            return keyCode == detectedNoteIndex; // <- highlight only that key
        case ImGuiExt::Piano::KeyboardMsgType::NoteOn:
        case ImGuiExt::Piano::KeyboardMsgType::NoteOff:
            return true;
    }
    return false;
}


int NoteNameToKeyIndex(const std::string& note) {
    static std::map<std::string, int> noteMap = {
        {"C", 0}, {"C#", 1}, {"D", 2}, {"D#", 3},
        {"E", 4}, {"F", 5}, {"F#", 6}, {"G", 7},
        {"G#", 8}, {"A", 9}, {"A#", 10}, {"B", 11}
    };

    if (note.length() < 2 || note.length() > 3)
        return -1;

    std::string name = note.substr(0, note.length() - 1);
    int octave = note.back() - '0';

    if (noteMap.find(name) == noteMap.end() || octave < 0 || octave > 9)
        return -1;

    return (octave + 1) * 12 + noteMap[name];
}


int main() {
    ListAudioDevices();
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

        ImGui::Checkbox("Use Real-Time Audio", &useRealtimeAudio);

        if (useRealtimeAudio && !isRecording) {
            StartAudio();
            fxAxis.resize(CIRCULAR_BUFFER_SIZE / 2);
            for (int i = 0; i < CIRCULAR_BUFFER_SIZE / 2; ++i) {
                fxAxis[i] = RECORD_SAMPLE_RATE * (static_cast<double>(i) / CIRCULAR_BUFFER_SIZE);
            }
        } else if (!useRealtimeAudio && isRecording) {
            StopAudio();
            if (len > 0) {
                int effectiveSampleRate = sampleRate / downsampleRate;
                fxAxis.resize(intervalLen / 2);
                for (int i = 0; i < intervalLen / 2; ++i) {
                    fxAxis[i] = effectiveSampleRate * (static_cast<double>(i) / intervalLen);
                }
            }
        }

        if (len > 0) {

            if (ImGui::Checkbox("Analyze Full Audio", &analyzeFullAudio)) {
                std::cout << "CLICKED" << std::endl;
                // if (analyzeFullAudio) {
                //     ToggleFullAudio(true);
                // } else {
                //     ToggleFullAudio(false);
                // }
            }

            if (ImGui::Button("Backward")) {
                ChangeInterval(false);
            }
            if (ImGui::Button("Forward")) {
                ChangeInterval(true);
            }

            int noteIndex = NoteNameToKeyIndex(detectedNote);
            if (noteIndex >= 0 && noteIndex <= 127) {
                detectedNoteIndex = static_cast<ImGuiExt::Piano::keyCode_t>(noteIndex);
                ImGuiExt::Piano::Keyboard("MyPiano", ImVec2(900, 100), &prevNoteActive, 0, 127, SimplePianoCallback);
            }
            ImGui::Text("Peak Frequency: %.2f Hz", peakFrequency);

            ImGui::Separator();

            std::string sampleStr = std::to_string(sampleRate);
            std::string waveformTitle = "Waveform Sampled At " + sampleStr + "Hz";
            std::string waveformTitle2 = "Waveform Interval Sampled At " + sampleStr + "Hz";
            std::string spectrumTitle = "Periodogram";

            if (isRecording) {
                UpdateLiveBuffer();

                std::vector<double> timeAxis(liveBuffer.size());
                for (size_t i = 0; i < liveBuffer.size(); ++i) {
                    timeAxis[i] = i / static_cast<double>(RECORD_SAMPLE_RATE);
                }

                if (ImPlot::BeginPlot("Real-Time Audio")) {
                    ImPlot::SetupAxes("Time (s)", "Amplitude");
                    ImPlot::PlotLine("Live", timeAxis.data(), liveBuffer.data(), liveBuffer.size());
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot(spectrumTitle.c_str())) {
                    ImPlot::SetupAxes("Frequency (Hz)", "Intensity");
                    ImPlot::PlotLine("FFT", fxAxis.data(), fIntensity.data(), CIRCULAR_BUFFER_SIZE / 2);
                    ImPlot::EndPlot();
                }
            } else {
                if (ImPlot::BeginPlot(waveformTitle.c_str())) {
                    ImPlot::SetupAxes("Time (Seconds)", "Amplitude");
                    if (analyzeFullAudio) {
                        ImPlot::PlotLine("Signal", xAxis.data(), intensity.data(), len);
                    } else {
                        ImPlot::PlotLine("Signal", xAxis.data(), intensity.data(), len);
                        ImPlot::PlotInfLines("Peaks", plotPeaks.data(), static_cast<int>(peaks.size()));
                        if (!peaks.empty()) {
                            double startTime = plotPeaks[timeIndex];
                            double endTime = (timeIndex + 1 < static_cast<int>(plotPeaks.size())) ? plotPeaks[timeIndex + 1] : duration;

                            std::vector<double> intervalFillX = { startTime, endTime };
                            std::vector<double> intervalFillLow = { -1.0, -1.0 }; // Y-min (adjust as needed)
                            std::vector<double> intervalFillHigh = { 1.0, 1.0 };  // Y-max (adjust as needed)

                            ImPlot::PushStyleColor(ImPlotCol_Fill, IM_COL32(255, 255, 0, 50)); // Light yellow transparent fill
                            ImPlot::PlotShaded("Current Interval", intervalFillX.data(), intervalFillLow.data(), intervalFillHigh.data(), 2);
                            ImPlot::PopStyleColor();
                        }
                    }

                    ImPlot::EndPlot();
                }
                if (needsAutoFit) {
                    ImPlot::SetNextAxesToFit();
                    needsAutoFit = false;  // reset
                }
                if (!analyzeFullAudio && ImPlot::BeginPlot(waveformTitle2.c_str())) {
                    ImPlot::SetupAxes("Time (Seconds)", "Amplitude");
                    ImPlot::PlotLine("Signal", xAxisInterval.data(), intensityInterval.data(), intervalLen);
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot(spectrumTitle.c_str())) {
                    ImPlot::SetupAxes("Frequency (Hz)", "Intensity");
                    if (analyzeFullAudio) {
                        ImPlot::PlotLine("FFT", fxAxis.data(), fIntensity.data(), len / 2);
                    } else {
                        ImPlot::PlotLine("FFT", fxAxis.data(), fIntensity.data(), intervalLen / 2);
                    }
                    ImPlot::EndPlot();
                }
            }
        }

        LoadRigby();
        ImGui::End();
        RenderWindow(window);
    }
    TerminateIMGUI(window);
    return 0;
}
