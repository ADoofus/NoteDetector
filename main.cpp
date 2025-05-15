#include "gui.hpp"
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "implot/implot.h"
#include "implot/implot_internal.h"
#include "AudioFile.h"
#include <cmath>
#include <filesystem>
#include <cassert>
#include <memory>
#include "GLFW/\include\glfw3.h"
#include "filedialog/tinyfiledialogs.h"

int len = 0;
std::vector<float> intensity;

std::vector<float> AnalyzeAudio(const std::string& path, int* len) {
    std::cout << "**********************" << std::endl;
    std::cout << "Running Example: Load Audio File and Print Summary" << std::endl;
    std::cout << "**********************" << std::endl;

    AudioFile<float> audioFile;
    if (!audioFile.load(path)) {
        std::cerr << "Failed to load file: " << path << std::endl;
        *len = 0;
        return {};
    }

    std::cout << "Bit Depth: " << audioFile.getBitDepth() << std::endl;
    std::cout << "Sample Rate: " << audioFile.getSampleRate() << std::endl;
    std::cout << "Num Channels: " << audioFile.getNumChannels() << std::endl;
    std::cout << "Length in Seconds: " << audioFile.getLengthInSeconds() << std::endl;
    std::cout << std::endl;

    int channel = 0;
    int numSamples = audioFile.getNumSamplesPerChannel();
    const auto& samples = audioFile.samples[channel];

    int downsampleRate = 1000;
    *len = numSamples / downsampleRate;

    std::vector<float> intensity(*len);
    for (int i = 0; i < *len; ++i) {
        intensity[i] = samples[i * downsampleRate];
    }

    return intensity;
}



int main() {
    
    ImPlot::CreateContext();
    GLFWwindow* window = CreateWindowIMGUI(640, 480, 0, 1);
    if (window == nullptr) {
        std::cout << "THE WINDOW DOESNT EXIST CUH" << std::endl;
        return 0;
    } 

    while (!glfwWindowShouldClose(window))
    {
        UpdateWindowSize(window);

        //Actual stuff =============================================


        ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::Text("Select An Audio File!");
        if (ImGui::Button("Import File")) {
            const char* filters[] = { "*.wav", "*.aiff", "*.jpg", "*.*" };
            const char* file = tinyfd_openFileDialog("Select a file", "", 4, filters, "All files", 0);
            if (file) {
                std::cout << "Imported file: " << file << std::endl;
                intensity = AnalyzeAudio(file, &len);

                // PlotLine(len, intensity);
            }
        }
        
        if (len > 0) {
             std::vector<float> xs(len);
            for (int i = 0; i < len; ++i) {
                xs[i] = static_cast<float>(i);
            }

            if (ImPlot::BeginPlot("Audio")) {
                ImPlot::SetupAxes("Time", "Intensity");
                ImPlot::PlotLine("f(x)", xs.data(), intensity.data(), len);
                ImPlot::EndPlot();
            }
        }
        //===============================================================

        ImGui::End();

        // Rendering
        RenderWindow(window);
    }

    // Cleanup
    TerminateIMGUI(window);

}