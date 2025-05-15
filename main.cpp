#include "gui.hpp"
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "AudioFile.h"
#include <cmath>
#include <filesystem>
#include <cassert>

void loadAudioFileAndPrintSummary()
{
    std::cout << "**********************" << std::endl;
    std::cout << "Running Example: Load Audio File and Print Summary" << std::endl;
    std::cout << "**********************" << std::endl;

    const std::filesystem::path filePath = std::filesystem::current_path() / "PianoKeys" / "Piano.ff.C4.aiff";
    std::string filePathStr = filePath.string();
    
    AudioFile<float> audioFile;
    bool loadedOK = audioFile.load(filePathStr);
    
    std::cout << "Bit Depth: " << audioFile.getBitDepth() << std::endl;
    std::cout << "Sample Rate: " << audioFile.getSampleRate() << std::endl;
    std::cout << "Num Channels: " << audioFile.getNumChannels() << std::endl;
    std::cout << "Length in Seconds: " << audioFile.getLengthInSeconds() << std::endl;
    std::cout << std::endl;

    int channel = 0;
    int numSamples = audioFile.getNumSamplesPerChannel();

    for (int i = 0; i < numSamples; i+= 1000)
    {
        double currentSample = audioFile.samples[channel][i];
        std::string str = std::to_string(currentSample);

        std::cout << str << std::endl;
    }
}


int main() {
    loadAudioFileAndPrintSummary();
    
    createWindowWithImgui(640, 480, 0, 1);
}