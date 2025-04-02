#include <catch2/catch.hpp>
#include <cmath>
#include "bw64/bw64.hpp"

using namespace bw64;

TEST_CASE("serialize_deserialize_markers_and_labels") {
  std::string tempFile = "test_markers_labels.wav";
  std::remove(tempFile.c_str());

  const uint16_t channels = 1;
  const uint32_t sampleRate = 44100;
  const uint16_t bitDepth = 16;
  const uint64_t numFrames = 88200;

  std::vector<float> audioData(numFrames);
  for (uint64_t i = 0; i < numFrames; ++i) {
    audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
  }

  std::vector<CuePoint> cuePoints;

  CuePoint cue1;
  cue1.id = 1;
  cue1.position = (uint32_t)(sampleRate * 0.5); // 0.5 seconds
  cue1.dataChunkId = bw64::utils::fourCC("data");
  cue1.chunkStart = 0;
  cue1.blockStart = 0;
  cue1.sampleOffset = (uint32_t)(sampleRate * 0.5);
  cuePoints.push_back(cue1);

  CuePoint cue2;
  cue2.id = 2;
  cue2.position = (uint32_t)(sampleRate * 1.0); // 1.0 seconds
  cue2.dataChunkId = bw64::utils::fourCC("data");
  cue2.chunkStart = 0;
  cue2.blockStart = 0;
  cue2.sampleOffset = (uint32_t)(sampleRate * 1.0);
  cuePoints.push_back(cue2);

  CuePoint cue3;
  cue3.id = 3;
  cue3.position = (uint32_t)(sampleRate * 1.5); // 1.5 seconds
  cue3.dataChunkId = bw64::utils::fourCC("data");
  cue3.chunkStart = 0;
  cue3.blockStart = 0;
  cue3.sampleOffset = (uint32_t)(sampleRate * 1.5);
  cuePoints.push_back(cue3);

  auto cueChunk = std::make_shared<CueChunk>(cuePoints);

  auto label1 = std::make_shared<LabelChunk>(1, "Marker 01");
  auto label2 = std::make_shared<LabelChunk>(2, "Marker 01a");
  auto label3 = std::make_shared<LabelChunk>(3, "Marker 02");

  // Create a LIST chunk of type 'adtl' containing all label chunks
  std::vector<std::shared_ptr<bw64::Chunk>> labelChunks;
  labelChunks.push_back(label1);
  labelChunks.push_back(label2);
  labelChunks.push_back(label3);

  auto listChunk = std::make_shared<ListChunk>(bw64::utils::fourCC("adtl"), labelChunks);

  try {
    std::vector<std::shared_ptr<bw64::Chunk>> additionalChunks;
    additionalChunks.push_back(cueChunk);

    auto writer = std::unique_ptr<bw64::Bw64Writer>(
      new bw64::Bw64Writer(tempFile.c_str(), channels, sampleRate, bitDepth,
        additionalChunks, false, false, 0));

    writer->write(audioData.data(), numFrames);

    // Adds after the data chunk
    writer->postDataChunk(listChunk);

    writer->close();

    // Check that file was created
    {
      std::ifstream checkFile(tempFile);
      REQUIRE(checkFile.good());
      checkFile.close();
    }

    auto reader = bw64::readFile(tempFile);

    bool foundCueChunk = false;
    bool foundListChunk = false;

    INFO("Chunks in the file:");
    for (const auto& chunkHeader : reader->chunks()) {
      std::string chunkId = {
        static_cast<char>((chunkHeader.id >> 0) & 0xFF),
        static_cast<char>((chunkHeader.id >> 8) & 0xFF),
        static_cast<char>((chunkHeader.id >> 16) & 0xFF),
        static_cast<char>((chunkHeader.id >> 24) & 0xFF)
      };
      INFO("  " << chunkId << " at position " << chunkHeader.position << ", size " << chunkHeader.size);

      if (chunkHeader.id == bw64::utils::fourCC("cue ")) {
        foundCueChunk = true;
      }
      else if (chunkHeader.id == bw64::utils::fourCC("LIST")) {
        foundListChunk = true;
      }
    }

    REQUIRE(foundCueChunk);
    REQUIRE(foundListChunk);

    std::vector<CuePoint> readCuePoints;
    std::map<uint32_t, std::string> readLabels;

    std::ifstream file(tempFile, std::ios::binary);

    // TODO: Add a function, or manager class to deal with cue and
    // TODO: label chunks, also 'note' and 'ltxt' as well.
    // For now, just read them manually

    for (const auto& chunkHeader : reader->chunks()) {
      if (chunkHeader.id == bw64::utils::fourCC("cue ")) {
        file.seekg(chunkHeader.position + 8); // Skip chunk id and size
        auto readCueChunk = parseCueChunk(file, chunkHeader.id, chunkHeader.size);
        readCuePoints = readCueChunk->cuePoints();
      }
      else if (chunkHeader.id == bw64::utils::fourCC("LIST")) {
        file.seekg(chunkHeader.position + 8); // Skip chunk ID and size

        // Get any 'adtl' list chunks
        uint32_t listType;
        bw64::utils::readValue(file, listType);

        if (listType == bw64::utils::fourCC("adtl")) {
          uint64_t listDataEnd = chunkHeader.position + 8 + chunkHeader.size;

          while (file.tellg() < listDataEnd) {
            uint32_t subChunkId;
            uint32_t subChunkSize;

            bw64::utils::readValue(file, subChunkId);
            bw64::utils::readValue(file, subChunkSize);

            if (subChunkId == bw64::utils::fourCC("labl")) {
              uint32_t cuePointId;
              bw64::utils::readValue(file, cuePointId);

              std::string label(subChunkSize - 4, '\0');
              file.read(&label[0], subChunkSize - 4);

              size_t nullPos = label.find('\0');
              if (nullPos != std::string::npos) {
                label.resize(nullPos);
              }

              readLabels[cuePointId] = label;
            } else {
              // Skip unknown
              file.seekg(subChunkSize, std::ios::cur);
              INFO("Unknown subchunk " << subChunkId << " of size " << subChunkSize);
            }

            if (subChunkSize % 2 == 1) {
              file.seekg(1, std::ios::cur);
            }
          }
        }
      }
    }

    REQUIRE(readCuePoints.size() == 3);

    if (readCuePoints.size() >= 3) {
      REQUIRE(readCuePoints[0].id == 1);
      REQUIRE(readCuePoints[0].position == (uint32_t)(sampleRate * 0.5));
      REQUIRE(readCuePoints[1].id == 2);
      REQUIRE(readCuePoints[1].position == (uint32_t)(sampleRate * 1.0));
      REQUIRE(readCuePoints[2].id == 3);
      REQUIRE(readCuePoints[2].position == (uint32_t)(sampleRate * 1.5));
    }

    REQUIRE(readLabels.size() == 3);

    if (readLabels.size() >= 3) {
      REQUIRE(readLabels[1] == "Marker 01");
      REQUIRE(readLabels[2] == "Marker 01a");
      REQUIRE(readLabels[3] == "Marker 02");
    }

    for (const auto& cue : readCuePoints) {
      REQUIRE(readLabels.find(cue.id) != readLabels.end());
    }

    reader->close();
    file.close();
  }
  catch (const std::exception& e) {
    FAIL("Exception occurred: " << e.what());
  }

  std::remove(tempFile.c_str());
}


TEST_CASE("marker_api_test") {
    std::string tempFile = "marker_api_test.wav";
    std::remove(tempFile.c_str());

    const uint16_t channels = 1;
    const uint32_t sampleRate = 44100;
    const uint16_t bitDepth = 16;
    const uint64_t numFrames = 88200;
    const uint32_t maxMarkers = 5;

    std::vector<float> audioData(numFrames);
    for (uint64_t i = 0; i < numFrames; ++i) {
        audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
    }

    try {
        // Create a writer with pre-allocated cue chunk
        auto writer = bw64::createSharedWriterWithMaxMarkers(
            tempFile, channels, sampleRate, bitDepth, false, false, 0, maxMarkers);

        // Add markers
        writer->addMarker(1, sampleRate * 0.5, "Marker 1"); // 0.5 seconds
        writer->addMarker(2, sampleRate * 1.0, "Marker 2"); // 1.0 seconds
        writer->addMarker(3, sampleRate * 1.5, "Marker 3"); // 1.5 seconds

        // Try to add a marker with an ID that already exists
        REQUIRE_THROWS_AS(writer->addMarker(1, sampleRate * 2.0, "Marker 1 Duplicate"), std::runtime_error);

        // Write audio data
        writer->write(audioData.data(), numFrames);
        writer->close();

        // Open the file for reading
        auto reader = bw64::readFile(tempFile);

        // Get all markers
        auto markers = reader->getMarkers();
        REQUIRE(markers.size() == 3);

        // Check marker 1
        REQUIRE(markers[0].id == 1);
        REQUIRE(markers[0].position == (uint32_t)(sampleRate * 0.5));
        REQUIRE(markers[0].label == "Marker 1");

        // Check marker 2
        REQUIRE(markers[1].id == 2);
        REQUIRE(markers[1].position == (uint32_t)(sampleRate * 1.0));
        REQUIRE(markers[1].label == "Marker 2");

        // Check marker 3
        REQUIRE(markers[2].id == 3);
        REQUIRE(markers[2].position == (uint32_t)(sampleRate * 1.5));
        REQUIRE(markers[2].label == "Marker 3");

        // Find a marker by ID
        const auto* marker = reader->findMarkerById(2);
        REQUIRE(marker != nullptr);
        REQUIRE(marker->id == 2);
        REQUIRE(marker->position == (uint32_t)(sampleRate * 1.0));
        REQUIRE(marker->label == "Marker 2");

        // Try to find a marker that doesn't exist
        const auto* nonExistentMarker = reader->findMarkerById(999);
        REQUIRE(nonExistentMarker == nullptr);

        reader->close();
    }
    catch (const std::exception& e) {
        FAIL("Exception occurred: " << e.what());
    }

    std::remove(tempFile.c_str());
}

TEST_CASE("marker_api_advanced_test") {
    std::string tempFile = "marker_api_advanced_test.wav";
    std::remove(tempFile.c_str());

    const uint16_t channels = 1;
    const uint32_t sampleRate = 44100;
    const uint16_t bitDepth = 16;
    const uint64_t numFrames = 88200;
    const uint32_t maxMarkers = 5;

    std::vector<float> audioData(numFrames);
    for (uint64_t i = 0; i < numFrames; ++i) {
        audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
    }

    try {
        // Create a writer with pre-allocated cue chunk
        auto writer = bw64::createSharedWriterWithMaxMarkers(
            tempFile, channels, sampleRate, bitDepth, false, false, 0, maxMarkers);

        // Add a marker using CuePoint
        bw64::CuePoint cue;
        cue.id = 1;
        cue.position = sampleRate * 0.5; // 0.5 seconds
        cue.dataChunkId = bw64::utils::fourCC("data");
        cue.chunkStart = 0;
        cue.blockStart = 0;
        cue.sampleOffset = sampleRate * 0.5;
        cue.label = "Marker 1";
        writer->addMarker(cue);

        // Add multiple markers at once
        std::vector<bw64::CuePoint> markers;
        
        bw64::CuePoint cue2;
        cue2.id = 2;
        cue2.position = sampleRate * 1.0; // 1.0 seconds
        cue2.dataChunkId = bw64::utils::fourCC("data");
        cue2.chunkStart = 0;
        cue2.blockStart = 0;
        cue2.sampleOffset = sampleRate * 1.0;
        cue2.label = "Marker 2";
        markers.push_back(cue2);
        
        bw64::CuePoint cue3;
        cue3.id = 3;
        cue3.position = sampleRate * 1.5; // 1.5 seconds
        cue3.dataChunkId = bw64::utils::fourCC("data");
        cue3.chunkStart = 0;
        cue3.blockStart = 0;
        cue3.sampleOffset = sampleRate * 1.5;
        cue3.label = "Marker 3";
        markers.push_back(cue3);
        
        writer->addMarkers(markers);

        // Add more markers (should be fine as long as maxMarkers is sufficient)
        writer->addMarker(4, sampleRate * 2.0, "Marker 4");
        writer->addMarker(5, sampleRate * 2.5, "Marker 5");

        // Write audio data
        writer->write(audioData.data(), numFrames);
        writer->close();

        // Open the file for reading
        auto reader = bw64::readFile(tempFile);

        // Get all markers
        auto readMarkers = reader->getMarkers();
        REQUIRE(readMarkers.size() == 5);

        // Check marker 1
        REQUIRE(readMarkers[0].id == 1);
        REQUIRE(readMarkers[0].position == (uint32_t)(sampleRate * 0.5));
        REQUIRE(readMarkers[0].label == "Marker 1");

        // Check marker 2
        REQUIRE(readMarkers[1].id == 2);
        REQUIRE(readMarkers[1].position == (uint32_t)(sampleRate * 1.0));
        REQUIRE(readMarkers[1].label == "Marker 2");

        // Check marker 3
        REQUIRE(readMarkers[2].id == 3);
        REQUIRE(readMarkers[2].position == (uint32_t)(sampleRate * 1.5));
        REQUIRE(readMarkers[2].label == "Marker 3");

        reader->close();
    }
    catch (const std::exception& e) {
        FAIL("Exception occurred: " << e.what());
    }

    std::remove(tempFile.c_str());
}

TEST_CASE("exceed_max_markers_test") {
    std::string tempFile = "exceed_max_markers_test.wav";
    std::remove(tempFile.c_str());

    const uint16_t channels = 1;
    const uint32_t sampleRate = 44100;
    const uint16_t bitDepth = 16;
    const uint64_t numFrames = 88200;
    const uint32_t maxMarkers = 2; // Only allow 2 markers

    std::vector<float> audioData(numFrames);
    for (uint64_t i = 0; i < numFrames; ++i) {
        audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
    }

    try {
        // Create a writer with limited pre-allocated cue chunk space
        auto writer = bw64::createSharedWriterWithMaxMarkers(
            tempFile, channels, sampleRate, bitDepth, false, false, 0, maxMarkers);

        // Add markers up to the limit
        writer->addMarker(1, sampleRate * 0.5, "Marker 1");
        writer->addMarker(2, sampleRate * 1.0, "Marker 2");

        // Try to add one more marker (should not throw - cue points are stored in vector)
        writer->addMarker(3, sampleRate * 1.5, "Marker 3");

        // Write audio data
        writer->write(audioData.data(), numFrames);

        // Try to close the file (should throw because the cue chunk is full)
        REQUIRE_THROWS_AS(writer->close(), std::runtime_error);
    }
    catch (const std::exception& e) {
        FAIL("Exception occurred: " << e.what());
    }

    std::remove(tempFile.c_str());
}

TEST_CASE("marker_sort_order_test") {
	std::string tempFile = "marker_sort_order_test.wav";
	std::remove(tempFile.c_str());

	const uint16_t channels = 1;
	const uint32_t sampleRate = 44100;
	const uint16_t bitDepth = 16;
	const uint64_t numFrames = 88200;
	const uint32_t maxMarkers = 5;

	std::vector<float> audioData(numFrames);
	for (uint64_t i = 0; i < numFrames; ++i) {
		audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
	}

	try {
		// Create a writer with pre-allocated cue chunk
		auto writer = bw64::createSharedWriterWithMaxMarkers(
															 tempFile, channels, sampleRate, bitDepth, false, false, 0, maxMarkers);

		// Add markers in non-sequential order
		writer->addMarker(3, sampleRate * 1.5, "Marker 3"); // Third position, added first
		writer->addMarker(1, sampleRate * 0.5, "Marker 1"); // First position, added second
		writer->addMarker(2, sampleRate * 1.0, "Marker 2"); // Second position, added third
		writer->addMarker(5, sampleRate * 2.5, "Marker 5"); // Fifth position, added fourth
		writer->addMarker(4, sampleRate * 2.0, "Marker 4"); // Fourth position, added last

		// Write audio data
		writer->write(audioData.data(), numFrames);
		writer->close();

		// Open the file for reading
		auto reader = bw64::readFile(tempFile);

		// Get all markers - should be sorted by position, not ID
		auto markers = reader->getMarkers();
		REQUIRE(markers.size() == 5);

		// Verify sorting by position
		REQUIRE(markers[0].position == (uint32_t)(sampleRate * 0.5));
		REQUIRE(markers[0].id == 1);
		REQUIRE(markers[1].position == (uint32_t)(sampleRate * 1.0));
		REQUIRE(markers[1].id == 2);
		REQUIRE(markers[2].position == (uint32_t)(sampleRate * 1.5));
		REQUIRE(markers[2].id == 3);
		REQUIRE(markers[3].position == (uint32_t)(sampleRate * 2.0));
		REQUIRE(markers[3].id == 4);
		REQUIRE(markers[4].position == (uint32_t)(sampleRate * 2.5));
		REQUIRE(markers[4].id == 5);

		reader->close();
	}
	catch (const std::exception& e) {
		FAIL("Exception occurred: " << e.what());
	}

	std::remove(tempFile.c_str());
}

TEST_CASE("marker_api_no_cue_chunk_test") {
    std::string tempFile = "marker_api_no_cue_chunk_test.wav";
    std::remove(tempFile.c_str());

    const uint16_t channels = 1;
    const uint32_t sampleRate = 44100;
    const uint16_t bitDepth = 16;
    const uint64_t numFrames = 88200;

    std::vector<float> audioData(numFrames);
    for (uint64_t i = 0; i < numFrames; ++i) {
        audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
    }

    try {
        // Create a writer without pre-allocated cue chunk
        auto writer = bw64::createSharedWriterWithMaxMarkers(
            tempFile, channels, sampleRate, bitDepth, false, false, 0, 0);

        // Try to add a marker (should throw because no cue chunk was pre-allocated)
        REQUIRE_THROWS_AS(writer->addMarker(1, sampleRate * 0.5, "Marker 1"), std::runtime_error);

        // Write audio data
        writer->write(audioData.data(), numFrames);
        writer->close();

        // Open the file for reading
        auto reader = bw64::readFile(tempFile);

        // Get all markers (should be empty)
        auto markers = reader->getMarkers();
        REQUIRE(markers.empty());

        reader->close();
    }
    catch (const std::exception& e) {
        FAIL("Exception occurred: " << e.what());
    }

    std::remove(tempFile.c_str());
}

TEST_CASE("create_shared_writer_with_markers_test") {
    std::string tempFile = "create_shared_writer_with_markers_test.wav";
    std::remove(tempFile.c_str());

    const uint16_t channels = 1;
    const uint32_t sampleRate = 44100;
    const uint16_t bitDepth = 16;
    const uint64_t numFrames = 88200;

    std::vector<float> audioData(numFrames);
    for (uint64_t i = 0; i < numFrames; ++i) {
        audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
    }

    try {
        // Create markers
        std::vector<bw64::CuePoint> markers;
        
        bw64::CuePoint cue1;
        cue1.id = 1;
        cue1.position = sampleRate * 0.5; // 0.5 seconds
        cue1.dataChunkId = bw64::utils::fourCC("data");
        cue1.chunkStart = 0;
        cue1.blockStart = 0;
        cue1.sampleOffset = sampleRate * 0.5;
        cue1.label = "Marker 1";
        markers.push_back(cue1);
        
        bw64::CuePoint cue2;
        cue2.id = 2;
        cue2.position = sampleRate * 1.0; // 1.0 seconds
        cue2.dataChunkId = bw64::utils::fourCC("data");
        cue2.chunkStart = 0;
        cue2.blockStart = 0;
        cue2.sampleOffset = sampleRate * 1.0;
        cue2.label = "Marker 2";
        markers.push_back(cue2);
        
        bw64::CuePoint cue3;
        cue3.id = 3;
        cue3.position = sampleRate * 1.5; // 1.5 seconds
        cue3.dataChunkId = bw64::utils::fourCC("data");
        cue3.chunkStart = 0;
        cue3.blockStart = 0;
        cue3.sampleOffset = sampleRate * 1.5;
        cue3.label = "Marker 3";
        markers.push_back(cue3);

        // Create a writer with markers
        auto writer = bw64::createSharedWriterWithMarkers(
            tempFile, channels, sampleRate, bitDepth, false, false, 0, markers);

        // Write audio data
        writer->write(audioData.data(), numFrames);
        writer->close();

        // Open the file for reading
        auto reader = bw64::readFile(tempFile);

        // Get all markers
        auto readMarkers = reader->getMarkers();
        REQUIRE(readMarkers.size() == 3);

        // Check marker 1
        REQUIRE(readMarkers[0].id == 1);
        REQUIRE(readMarkers[0].position == (uint32_t)(sampleRate * 0.5));
        REQUIRE(readMarkers[0].label == "Marker 1");

		// Check marker 2
		REQUIRE(readMarkers[1].id == 2);
		REQUIRE(readMarkers[1].position == (uint32_t)(sampleRate * 1.0));
		REQUIRE(readMarkers[1].label == "Marker 2");

		// Check marker 3
		REQUIRE(readMarkers[2].id == 3);
		REQUIRE(readMarkers[2].position == (uint32_t)(sampleRate * 1.5));
		REQUIRE(readMarkers[2].label == "Marker 3");

		reader->close();
	}
	catch (const std::exception& e) {
		FAIL("Exception occurred: " << e.what());
	}

	std::remove(tempFile.c_str());
}
