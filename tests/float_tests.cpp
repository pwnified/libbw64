#include <catch2/catch.hpp>
#include <cmath>
#include "bw64/bw64.hpp"

using namespace bw64;

TEST_CASE("float_format_write_read") {
  // Test creating and writing to a float-format file
  {
    std::string tempFile = "float_format_write_read.wav";

    const uint16_t channels = 2;
    const uint32_t sampleRate = 48000;
    const uint16_t bitDepth = 32;
    const uint64_t numFrames = 1000;

    // Create test sine wave
    std::vector<float> writeBuffer(channels * numFrames);
    for (uint64_t i = 0; i < numFrames; ++i) {
      for (uint16_t ch = 0; ch < channels; ++ch) {
        float freq = 440.0f * (ch + 1);  // 440Hz, 880Hz
        float time = static_cast<float>(i) / static_cast<float>(sampleRate);
        writeBuffer[i * channels + ch] = static_cast<float>(std::sin(2.0 * M_PI * freq * time));
      }
    }

    // Create a writer with float format
    {
      // First test with standard non-extensible format
      auto writer = std::unique_ptr<Bw64Writer>(
          new Bw64Writer(tempFile.c_str(), channels, sampleRate, bitDepth, {}, false, true));

      uint64_t writtenFrames = writer->write(writeBuffer.data(), numFrames);
      REQUIRE(writtenFrames == numFrames);

      REQUIRE(writer->formatTag() == WAVE_FORMAT_IEEE_FLOAT);
      REQUIRE(writer->channels() == channels);
      REQUIRE(writer->sampleRate() == sampleRate);
      REQUIRE(writer->bitDepth() == bitDepth);
      REQUIRE(writer->framesWritten() == numFrames);

      writer->close();
    }

    // Read back and verify the data
    {
      auto reader = readFile(tempFile);

      REQUIRE(reader->formatTag() == WAVE_FORMAT_IEEE_FLOAT);
      REQUIRE(reader->channels() == channels);
      REQUIRE(reader->sampleRate() == sampleRate);
      REQUIRE(reader->bitDepth() == bitDepth);
      REQUIRE(reader->numberOfFrames() == numFrames);

      auto fmt = reader->formatChunk();
      REQUIRE(fmt->isFloat() == true);

      std::vector<float> readBuffer(channels * numFrames);
      uint64_t readFrames = reader->read(readBuffer.data(), numFrames);
      REQUIRE(readFrames == numFrames);

      for (uint64_t i = 0; i < channels * numFrames; ++i) {
        REQUIRE(readBuffer[i] == Approx(writeBuffer[i]).epsilon(0.0001));
      }

      reader->close();
    }

    // Now test with extensible format
    {
      // Define channel mask for stereo
      uint32_t channelMask = 0x3; // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT

      auto writer = std::unique_ptr<Bw64Writer>(
          new Bw64Writer(tempFile.c_str(), channels, sampleRate, bitDepth, {}, true, true, channelMask));

      uint64_t writtenFrames = writer->write(writeBuffer.data(), numFrames);
      REQUIRE(writtenFrames == numFrames);

      REQUIRE(writer->formatTag() == WAVE_FORMAT_EXTENSIBLE);
      REQUIRE(writer->framesWritten() == numFrames);

      writer->close();
    }

    // Read back and verify the extensible format
    {
      auto reader = readFile(tempFile);

      REQUIRE(reader->formatTag() == WAVE_FORMAT_EXTENSIBLE);

      auto fmt = reader->formatChunk();
      REQUIRE(fmt->isExtensible() == true);
      REQUIRE(fmt->isFloat() == true);
      REQUIRE(fmt->extraData() != nullptr);
      REQUIRE(guidsEqual(fmt->extraData()->subFormat(), KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) == true);

      reader->close();
    }

    // Clean up the test file
    std::remove(tempFile.c_str());
  }
}


TEST_CASE("float_format_io") {
  std::string tempFile = "float_format_io.wav";

  const uint16_t channels = 2;
  const uint32_t sampleRate = 48000;
  const uint16_t bitDepth = 32;
  const uint64_t numFrames = 1000;

  // Test values that would be clipped in integer PCM but not in float
  std::vector<float> testValues = {
    -3.5f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 3.5f
  };

  // Create test data with values outside the [-1,1] range
  std::vector<float> writeBuffer(channels * numFrames);
  for (uint64_t i = 0; i < numFrames; ++i) {
    for (uint16_t ch = 0; ch < channels; ++ch) {
      writeBuffer[i * channels + ch] = testValues[i % testValues.size()];
    }
  }

  SECTION("Float format preserves values outside [-1,1]") {
    // Create a writer with float format
    {
      auto writer = std::unique_ptr<Bw64Writer>(
                                                new Bw64Writer(tempFile.c_str(), channels, sampleRate, bitDepth,
                                                               std::vector<std::shared_ptr<Chunk>>(), false, true));

      REQUIRE(writer->formatTag() == WAVE_FORMAT_IEEE_FLOAT);
      REQUIRE(writer->formatChunk()->isFloat() == true);

      uint64_t writtenFrames = writer->write(writeBuffer.data(), numFrames);
      REQUIRE(writtenFrames == numFrames);
      writer->close();
    }

    // Read back and verify the data
    {
      auto reader = readFile(tempFile);

      REQUIRE(reader->formatTag() == WAVE_FORMAT_IEEE_FLOAT);
      REQUIRE(reader->formatChunk()->isFloat() == true);

      std::vector<float> readBuffer(channels * numFrames);
      uint64_t readFrames = reader->read(readBuffer.data(), numFrames);
      REQUIRE(readFrames == numFrames);

      // Verify the values outside [-1,1] should be preserved
      for (uint64_t i = 0; i < channels * numFrames; ++i) {
        REQUIRE(readBuffer[i] == Approx(writeBuffer[i]).epsilon(0.0001));
      }

      reader->close();
    }
  }

  SECTION("PCM format clips values outside [-1,1]") {
    // Create a writer with PCM format
    {
      auto writer = std::unique_ptr<Bw64Writer>(
                                                new Bw64Writer(tempFile.c_str(), channels, sampleRate, bitDepth,
                                                               std::vector<std::shared_ptr<Chunk>>(), false, false));

      REQUIRE(writer->formatTag() == WAVE_FORMAT_PCM);
      REQUIRE(writer->formatChunk()->isFloat() == false);

      uint64_t writtenFrames = writer->write(writeBuffer.data(), numFrames);
      REQUIRE(writtenFrames == numFrames);
      writer->close();
    }

    // Read back and verify the data
    {
      auto reader = readFile(tempFile);

      REQUIRE(reader->formatTag() == WAVE_FORMAT_PCM);
      REQUIRE(reader->formatChunk()->isFloat() == false);

      std::vector<float> readBuffer(channels * numFrames);
      uint64_t readFrames = reader->read(readBuffer.data(), numFrames);
      REQUIRE(readFrames == numFrames);

      // Verify the values outside [-1,1] should be clipped
      for (uint64_t i = 0; i < channels * numFrames; ++i) {
        float expected = writeBuffer[i];
        if (expected > 1.0f) expected = 1.0f;
        if (expected < -1.0f) expected = -1.0f;

        REQUIRE(readBuffer[i] == Approx(expected).epsilon(0.0001));
      }

      reader->close();
    }
  }

  std::remove(tempFile.c_str());
}
