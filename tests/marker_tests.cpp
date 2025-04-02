#include <catch2/catch.hpp>
#include <cmath>
#include "bw64/bw64.hpp"

using namespace bw64;

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
        writer->close();

        // Open the file for reading
        auto reader = bw64::readFile(tempFile);

        // Get all markers - all should be present since we're not enforcing a hard limit 
        // in the implementation, just pre-allocating space
        auto markers = reader->getMarkers();
        REQUIRE(markers.size() == 3);
        
        reader->close();
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

		// Check marker 4
		REQUIRE(readMarkers[3].id == 4);
		REQUIRE(readMarkers[3].position == (uint32_t)(sampleRate * 2.0));
		REQUIRE(readMarkers[3].label == "Marker 4");

		// Check marker 5
		REQUIRE(readMarkers[4].id == 5);
		REQUIRE(readMarkers[4].position == (uint32_t)(sampleRate * 2.5));
		REQUIRE(readMarkers[4].label == "Marker 5");

		reader->close();
	}
	catch (const std::exception& e) {
		FAIL("Exception occurred: " << e.what());
	}

	std::remove(tempFile.c_str());
}
