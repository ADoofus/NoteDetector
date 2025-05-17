#include "gui.hpp"
#include "fourierTrans/fourier.cpp"
#include "filedialog/tinyfiledialogs.h"
#include "AudioFile.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "implot/implot.h"
#include "implot/implot_internal.h"

#include "GLFW/include/glfw3.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <complex>

int len = 0;
int sampleRate = 0;
double duration = 0;
int numSamples = 0;
int downsampleRate = 10;
std::vector<double> intensity;
std::vector<double> fIntensity;
std::vector<double> fxAxis;
std::vector<double> xAxis;

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

    // // Apply low-pass filter before downsampling
    // auto filtered = LowPassFilter(samples, downsampleRate);

    std::vector<double> intensity(*len);
    for (int i = 0; i < *len; ++i) {
        intensity[i] = samples[i*downsampleRate];
    }

    return intensity;
}

int main() {
    ImPlot::CreateContext();
    GLFWwindow* window = CreateWindowIMGUI(640, 480, 0, 1);
    if (!window) {
        std::cerr << "Failed to create window!" << std::endl;
        return 0;
    }

    while (!glfwWindowShouldClose(window)) {
        UpdateWindowSize(window);

        ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::Text("Select An Audio File!");

        if (ImGui::Button("Import File")) {
            const char* filters[] = { "*.wav", "*.aiff", "*.jpg", "*.*" };
            const char* file = tinyfd_openFileDialog("Select a file", "", 4, filters, "All files", 0);

            if (file) {
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
        }

        ImGui::End();
        RenderWindow(window);
    }

    TerminateIMGUI(window);
    return 0;
}
