//#include "ecw_reader.h"
//#include <stdio.h>
//#include <iostream>
#include "API/Sync/NCSSyncDecoder.h"
#include "NCSExceptionTypes.h"

#include <iostream>
#include <vector>
#include <stdexcept>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

//int main(int argc, char* argv[]) {
//    if (argc < 3) {
//        std::cerr << "Usage: ECWConverter <input_path> <output_path>" << std::endl;
//        return 1;
//    }
//
//    const char* input_path = argv[1];
//    const char* output_path = argv[2];
//
//    std::cout << "Input: " << input_path << "\nOutput: " << output_path << std::endl;
//
//    //const char* path = R"(D:\Programming\C++\ECWConverter\x64\Release\testdata\Greyscale_8bit.ecw)"; // change to test path
//    //bool result = read_ecw_to_file(path, "output_ecw.png");
//
//    //bool result = read_ecw_to_file(input_path, output_path);
//    bool result = read_ecw_to_file_2(input_path, output_path);
//
//    if (result)
//        printf("Image saved successfully.\n");
//    else
//        printf("Failed to save image.\n");
//
//    return result ? 0 : 1;
//}

int main(int argc, char* argv[])
{
	#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	#endif

	if (argc != 2) return 1;
	NCS::CString filePath(argv[1]);

	uint64_t fileInfoArray[8];
	uint64_t fileStatus = 0; //0 == start, 1 == ok, 2 == error
	try {
		const auto decoderContextFactory = NCS::API::Decode::ISyncDatasetDecoderContextFactory::Instance();
		const auto decoderContext = decoderContextFactory->OpenFromString(filePath.ToStdString());

		auto blockingRGBADecoder = NCS::API::Decode::ISyncDatasetRGBAScanLineDecoder::OpenFromContext(decoderContext);
		const auto defaultBandlist = decoderContext->GetDatasetMetadata()->GetDefaultRGBABandlist();

		const auto fileInfo = decoderContext->GetDatasetMetadata()->GetFileInfo();
		if (fileInfo->GetCellType() != NCSCT_UINT8) {
			//std::cout << "Input image is not UINT8. Applying a 99% clip to help visibility." << std::endl;
			blockingRGBADecoder->GetBlockingDecoderOptions()->SetPercentageClip(99, 100);
		}

		uint32_t imageWidth = fileInfo->GetSizeX();
		uint32_t imageHeight = fileInfo->GetSizeY();
		double topLeftX = fileInfo->GetOriginX();
		double topLeftY = fileInfo->GetOriginY();
		double ppuX = fileInfo->GetCellIncrementX();
		double ppuY = fileInfo->GetCellIncrementX();
		double rotation = fileInfo->GetRotationDegrees();

		fileStatus = 1;
		fileInfoArray[0] = fileStatus;
		fileInfoArray[1] = imageWidth;
		fileInfoArray[2] = imageHeight;
		memcpy(&fileInfoArray[3], &topLeftX, 8);
		memcpy(&fileInfoArray[4], &topLeftY, 8);
		memcpy(&fileInfoArray[5], &ppuX, 8);
		memcpy(&fileInfoArray[6], &ppuY, 8);
		memcpy(&fileInfoArray[7], &rotation, 8);

		std::cout.write(reinterpret_cast<const char*>(fileInfoArray), 8 * 8);
		std::cout.flush();

		uint64_t inputBuffer[6];
		while (std::cin.read(reinterpret_cast<char*>(inputBuffer), 6 * 8)) {
			uint32_t imgL = uint32_t(inputBuffer[0]);
			uint32_t imgB = uint32_t(inputBuffer[1]);
			uint32_t imgR = uint32_t(inputBuffer[2]);
			uint32_t imgT = uint32_t(inputBuffer[3]);
			uint32_t viewX = uint32_t(inputBuffer[4]);
			uint32_t viewY = uint32_t(inputBuffer[5]);

			NCSViewSize viewSize{ viewX, viewY };
			const auto rgbaViewContext = blockingRGBADecoder->SetViewByImageCoordinates(defaultBandlist, viewSize, { imgL, imgT, imgR, imgB });

			uint64_t num64 = ((viewSize.Width * viewSize.Height) >> 1) + 1;
			std::vector<uint64_t> rgbaData(num64);
			uint32_t* u32Data = reinterpret_cast<uint32_t*>(rgbaData.data());

			for (uint32_t line = 0; line < viewSize.Height; line++) {
				rgbaViewContext->ReadLineRGBA(u32Data + line * viewSize.Width);
			}

			//int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);
			//stbi_write_png("test.png", viewSize.Width, viewSize.Height, 4, u32Data, viewSize.Width * 4);

			std::cout.write(reinterpret_cast<const char*>(u32Data), num64 * 8);
			std::cout.flush();
		}
	}
	catch (NCS::CChainableRuntimeError& error) {
		//NCS::CString exc = error.GetExceptionChain();
		if (fileStatus == 0) { //file open error
			fileStatus = 2;
			fileInfoArray[0] = fileStatus;
			std::cout.write(reinterpret_cast<const char*>(fileInfoArray), 8 * 8);
			std::cout.flush();
		}
	}

	return int(fileStatus);

	/*
	if (argc != 2)
	{
		std::cout << "Usage: " << argv[0] << " <input filename.ecw|jp2>" << std::endl;
		std::cout << "Example: " << argv[0] << " ecwp://demo-apollo.hexagongeospatial.com/SampleIWS/images/usa/sandiego3i.ecw" << std::endl;
		return 1;
	}

	// Use NCS::CString for utf8 conversion
	NCS::CString filePath(argv[1]);

	try {

		// Create a dataset decoder context from a dataset decoder context factory
		// NCS::API::Decode::IAsyncDatasetDecoderContextFactory::Instance() could also be used if your application needs this to be non blocking
		const auto decoderContextFactory = NCS::API::Decode::ISyncDatasetDecoderContextFactory::Instance();
		const auto decoderContext = decoderContextFactory->OpenFromString(filePath.ToStdString());

		// Create a decoder object from the decoder context. Each decoder type implements its own factory method to create an instance
		// This is a 'blocking' decoder so the ecwjp2 sdk will block until the required data is available before returning from read methods
		auto blockingRGBADecoder = NCS::API::Decode::ISyncDatasetRGBAScanLineDecoder::OpenFromContext(decoderContext);

		// Calculate the desired view size from the dataset while preserving the datasets aspect ratio
		NCSViewSize viewSize{ 256, 256 };
		const auto fileInfo = decoderContext->GetDatasetMetadata()->GetFileInfo();
		if (fileInfo->GetSizeX() > fileInfo->GetSizeY()) {
			viewSize.Height = viewSize.Width * fileInfo->GetSizeY() / fileInfo->GetSizeX();
		}
		else {
			viewSize.Width = viewSize.Height * fileInfo->GetSizeX() / fileInfo->GetSizeY();
		}

		// Get default rgba bandlist from dataset
		const auto defaultBandlist = decoderContext->GetDatasetMetadata()->GetDefaultRGBABandlist();

		//apply percentage clip if reading non UINT8 data
		if (fileInfo->GetCellType() != NCSCT_UINT8) {
			std::cout << "Input image is not UINT8. Applying a 99% clip to help visibility." << std::endl;
			blockingRGBADecoder->GetBlockingDecoderOptions()->SetPercentageClip(99, 100);
		}

		// Start reading the dataset.  The progressive decoder is non blocking so we will need to wait
		// for it to be complete for this example.
		const auto rgbaViewContext = blockingRGBADecoder->SetViewByImageCoordinates(defaultBandlist, viewSize, {
			0,
			0,
			fileInfo->GetSizeX() - 1,
			fileInfo->GetSizeY() - 1
		});

		std::vector<uint8_t> bilData(viewSize.Width * viewSize.Height * defaultBandlist.size());
		std::vector<uint8_t *> bilLine(defaultBandlist.size());

		for (uint32_t line = 0; line < viewSize.Height; line++) {

			// advance bilLine pointers to the correct part of the bilData buffer for each line
			for (size_t band = 0; band < defaultBandlist.size(); band++) {
				bilLine[band] = bilData.data() + (line * viewSize.Width * defaultBandlist.size()) + (band * viewSize.Width);
			}

			// Using the uint8_t variant for the bmp encoder
			rgbaViewContext->ReadLineARGB(bilLine.data());
		}

		CBILBmpEncoder bmpEncoder("BlockingBILDecoder.bmp", bilData.data(), defaultBandlist.size(), viewSize.Width, viewSize.Height);
		bmpEncoder.Write();
	}
	catch (NCS::CChainableRuntimeError &error) {
		// A subclass of NCS::CChainableRuntimeError will be thrown in the case of an error
		NCS::CString exc = error.GetExceptionChain();
		std::cout << "Error: " << exc << std::endl;
		return 3;
	}
	catch (std::runtime_error &error) {
		// This will only be thrown by the CBmpEncoder
		std::cout << "BMP Error: " << error.what() << std::endl;
		return 4;
	}

	return 0;
	*/
}