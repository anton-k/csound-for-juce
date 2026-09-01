build:
    cmake -B build -G Ninja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build --target  juce_csd
    cp build/compile_commands.json .

test:
    cmake -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure
    cp build/compile_commands.json .

