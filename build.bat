g++ -DWIN32 -D__WINDOWS_DS__ -std=c++20 -o gui ^
  ./*.cpp ./imgui/*.cpp ./rtAudio/*.cpp ./pianoVisual/*.cpp ./filedialog/*.c ./implot/*.cpp ^
  -LC:/mingw/lib ^
  -I./pianoVisual -I./rtAudio -I./imgui -I./implot -I./GLFW/include -I./filedialog ^
  -L./GLFW/lib-mingw-w64 ^
  -lglfw3 -lopengl32 -lgdi32 -limm32 -lshell32 -lole32 -lcomdlg32 -ldsound -lwinmm -lpthread
