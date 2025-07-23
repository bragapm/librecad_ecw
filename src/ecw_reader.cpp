#include "ecw_reader.h"
#define NCSECW_STATIC_LIBS

#include <NCSFile.h>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

const uint32_t DOWNSAMPLE_THRESHOLD = 10000;
const uint32_t MAXVIEW_WINDOWSIZE = 384;
#define RAND() ((double) rand() / (double) RAND_MAX)

struct ViewSizeInfo {
    uint32_t StartX;
    uint32_t StartY;
    uint32_t EndX;
    uint32_t EndY;
};

ViewSizeInfo GennerateViewRegion(uint32_t imgWidth, uint32_t imgHeight) {
    ViewSizeInfo viewSizeInfo;
    srand(static_cast<unsigned int>(time(nullptr)));

    viewSizeInfo.StartX = static_cast<uint32_t>(RAND() * imgWidth);
    viewSizeInfo.EndX = static_cast<uint32_t>(RAND() * (imgWidth - viewSizeInfo.StartX)) + viewSizeInfo.StartX;
    viewSizeInfo.StartY = static_cast<uint32_t>(RAND() * imgHeight);
    viewSizeInfo.EndY = static_cast<uint32_t>(RAND() * (imgHeight - viewSizeInfo.StartY)) + viewSizeInfo.StartY;

    if ((viewSizeInfo.EndX - viewSizeInfo.StartX) < MAXVIEW_WINDOWSIZE) {
        viewSizeInfo.EndX = viewSizeInfo.StartX + MAXVIEW_WINDOWSIZE;
        if (viewSizeInfo.EndX >= imgWidth) {
            viewSizeInfo.StartX = imgWidth - MAXVIEW_WINDOWSIZE;
            viewSizeInfo.EndX = imgWidth - 1;
        }
    }

    if ((viewSizeInfo.EndY - viewSizeInfo.StartY) < MAXVIEW_WINDOWSIZE) {
        viewSizeInfo.EndY = viewSizeInfo.StartY + MAXVIEW_WINDOWSIZE;
        if (viewSizeInfo.EndY >= imgHeight) {
            viewSizeInfo.StartY = imgHeight - MAXVIEW_WINDOWSIZE;
            viewSizeInfo.EndY = imgHeight - 1;
        }
    }

    if ((viewSizeInfo.EndX - viewSizeInfo.StartX) < (viewSizeInfo.EndY - viewSizeInfo.StartY))
        viewSizeInfo.EndY = viewSizeInfo.StartY + (viewSizeInfo.EndX - viewSizeInfo.StartX);
    else
        viewSizeInfo.EndX = viewSizeInfo.StartX + (viewSizeInfo.EndY - viewSizeInfo.StartY);

    return viewSizeInfo;
}

bool read_ecw_to_file(const char* input_path, const char* output_path) {
    NCS::CApplication App;
    NCS::CView view;

    std::cout << "Opening file: " << input_path << std::endl;

    NCS::CError err = view.Open(input_path);
    if (!err.Success()) {
        printf("Failed to open file: %s\n", input_path);
        std::cout << "Error opening file: " << err.GetErrorMessage() << std::endl;
        return false;
    }

    const NCSFileInfo* info = view.GetFileInfo();
    if (!info) {
        printf("Failed to retrieve file info.\n");
        return false;
    }

    UINT32 width = info->nSizeX;
    UINT32 height = info->nSizeY;
    UINT32 bands = NCSMin(3U, info->nBands);

    std::vector<UINT32> bandList;
    for (UINT32 bandIndex = 0; bandIndex < bands; bandIndex++) 
        bandList.push_back(bandIndex);


    // Read full image (view region generation is unused here)
    err = view.SetView(bands, &bandList[0], 0, 0, width - 1, height - 1, width, height);
    if (!err.Success()) {
        printf("SetView failed.\n");
        return false;
    }

    std::cout << "Width: " << width << ", Height: " << height << ", Bands: " << bands << std::endl;

    NCSCellType cellType = info->eCellType;
    NCS::SDK::CBuffer2D::Type bufferType;
    switch (cellType) {
    case NCSCT_UINT8:  bufferType = NCS::SDK::CBuffer2D::BT_UINT8; break;
    case NCSCT_UINT16: bufferType = NCS::SDK::CBuffer2D::BT_UINT16; break;
    case NCSCT_INT16:  bufferType = NCS::SDK::CBuffer2D::BT_INT16; break;
    case NCSCT_IEEE4:  bufferType = NCS::SDK::CBuffer2D::BT_IEEE4; break;
    case NCSCT_IEEE8:  bufferType = NCS::SDK::CBuffer2D::BT_IEEE8; break;
    default:
        std::cerr << "Unsupported cell type: " << cellType << std::endl;
        return false;
    }

    NCS::SDK::CBuffer2DVector Buffers;
    Buffers.resize(bands);
    for (UINT32 b = 0; b < bands; b++)
    {
        Buffers[b].Alloc(width, height, bufferType, NCS::SDK::CBuffer2D::HEAPCOMPRESSION);
    }

    NCSReadStatus readStatus = view.Read(Buffers, cellType);
    if (readStatus != NCS_READ_OK) {
        printf("Read failed.\n");
        std::cout << "Read failed. Error code: " << static_cast<int>(readStatus) << std::endl;

        switch (readStatus) {
        case NCS_READ_FAILED:
            std::cout << "General read failure." << std::endl;
            break;
        case NCS_FILE_INVALID:
            std::cout << "The file is invalid." << std::endl;
            break;
        case NCS_COULDNT_ALLOC_MEMORY:
            std::cout << "Could not allocate memory." << std::endl;
            break;
        /*case NCS_COULDNT_OPEN_INPUT_FILE:
            std::cout << "Could not open the input file." << std::endl;
            break;*/
        default:
            std::cout << "Unknown read error." << std::endl;
            break;
        }
        return false;
    }

    std::vector<uint8_t> imageData(width * height * 3, 0);
    for (UINT32 y = 0; y < height; ++y) {
        for (UINT32 x = 0; x < width; ++x) {
            for (UINT32 b = 0; b < bands; ++b) {
                void* ptr = Buffers[b].GetPtr(x, y);
                if (ptr) {
                    imageData[(y * width + x) * 3 + b] = *static_cast<uint8_t*>(ptr);
                }
            }
        }
    }

    if (!stbi_write_png(output_path, width, height, 3, imageData.data(), width * 3)) {
        printf("Failed to write PNG to: %s\n", output_path);
        return false;
    }

    return true;
}

bool read_ecw_to_file_2(const char* input_path, const char* output_path) {
    NCS::CApplication App;
    NCS::CView view;

    std::cout << "[INFO] Opening file: " << input_path << std::endl;

    NCS::CError err = view.Open(input_path);
    if (!err.Success()) {
        std::cerr << "[ERROR] Failed to open file:  " << err.GetErrorMessage() << std::endl;
        return false;
    }

    const NCSFileInfo* info = view.GetFileInfo();
    if (!info) {
        std::cerr << "[ERROR] Failed to retrieve file info." << std::endl;
        return false;
    }

    uint32_t width = info->nSizeX;
    uint32_t height = info->nSizeY;
    uint32_t bands = NCSMin(3U, info->nBands);

    std::cout << "[INFO] Image Size: " << width << "x" << height << ", Bands: " << bands << std::endl;

    std::vector<UINT32> bandList;
    for (uint32_t i = 0; i < bands; ++i) {
        bandList.push_back(i);
    }

    uint32_t viewWidth = width;
    uint32_t viewHeight = height;

    // Downsample if very large
    if (width > DOWNSAMPLE_THRESHOLD || height > DOWNSAMPLE_THRESHOLD) {
        viewWidth = width / 4;
        viewHeight = height / 4;
        std::cout << "[WARN] Image too large, downsampling to " 
            << viewWidth << "x" << viewHeight << ", Bands: " << bands << std::endl;
    }

    // SetView (request resolution)
    err = view.SetView(bands, bandList.data(), 0, 0, width - 1, height - 1, viewWidth, viewHeight);
    if (!err.Success()) {
        std::cerr << "[ERROR] SetView failed: " << err.GetErrorMessage() << std::endl;
        return false;
    }

    NCSCellType cellType = info->eCellType;
    if (cellType != NCSCT_UINT8 && cellType != NCSCT_UINT16 &&
        cellType != NCSCT_INT16 && cellType != NCSCT_IEEE4) {
        std::cerr << "[ERROR] Unsupported ECW cell type: " << cellType << std::endl;
        return false;
    }

    NCS::SDK::CBuffer2DVector Buffers;
    Buffers.resize(bands);
    for (uint32_t b = 0; b < bands; ++b) {
        Buffers[b].Alloc(viewWidth, viewHeight, NCS::SDK::CBuffer2D::BT_UINT8, 
            NCS::SDK::CBuffer2D::HEAPCOMPRESSION);
    }

    // Read image
    NCSReadStatus readStatus = view.Read(Buffers, NCSCT_UINT8);
    if (readStatus != NCS_READ_OK) {
        std::cerr << "[ERROR] Read failed with code: " << readStatus << std::endl;
        return false;
    }

    // Prepare RGB 8-bit image buffer
    std::vector<uint8_t> imageData(viewWidth * viewHeight * 3, 0);
    for (uint32_t y = 0; y < viewHeight; ++y) {
        for (uint32_t x = 0; x < viewWidth; ++x) {
            for (uint32_t b = 0; b < bands; ++b) {
                void* ptr = Buffers[b].GetPtr(x, y);
                if (ptr) {
                    imageData[(y * viewWidth + x) * 3 + b] = *static_cast<uint8_t*>(ptr);
                }
            }
        }
    }

    if (!stbi_write_png(output_path, viewWidth, viewHeight, 3, imageData.data(), viewWidth * 3)) {
        std::cerr << "[ERROR] Failed to write PNG to: " << output_path << std::endl;
        return false;
    }

    std::cout << "[INFO] Successfully wrote PNG: " << output_path << std::endl;
    return true;
}