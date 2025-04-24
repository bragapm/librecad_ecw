#include "ecw_reader.h"
#include <stdio.h>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ECWConverter <input_path> <output_path>" << std::endl;
        return 1;
    }

    const char* input_path = argv[1];
    const char* output_path = argv[2];

    std::cout << "Input: " << input_path << "\nOutput: " << output_path << std::endl;

    //const char* path = R"(D:\Programming\C++\ECWConverter\x64\Release\testdata\Greyscale_8bit.ecw)"; // change to test path
    //bool result = read_ecw_to_file(path, "output_ecw.png");

    bool result = read_ecw_to_file(input_path, output_path);

    if (result)
        printf("Image saved successfully.\n");
    else
        printf("Failed to save image.\n");

    return result ? 0 : 1;
}
