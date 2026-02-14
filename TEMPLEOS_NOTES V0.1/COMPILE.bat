@ECHO OFF
g++ main.cpp -o main.exe -I"C:/msys64/ucrt64/include" -L"C:/msys64/ucrt64/lib" -lfreeglut -lopengl32 -lglu32
pause