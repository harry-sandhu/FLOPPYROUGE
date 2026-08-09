rm -rf build && \
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ && \
cmake --build build


wine build/FloppyRogue.exe