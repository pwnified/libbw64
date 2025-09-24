/**
 * @file bw64.hpp
 *
 * libbw64 main header file. This should be the only file you need to include
 * into your user code.
 */
#pragma once
#include <cstdio>
#include <vector>
#include "reader.hpp"
#include "writer.hpp"

namespace bw64 {

  /**
   * @brief Open a BW64 file for reading
   *
   * @param filename path of the file to read
   *
   * Convenience function to open a BW64 file for reading.
   *
   * @returns `unique_ptr` to a Bw64Reader instance that is ready to read
   * samples.
   */
  inline std::unique_ptr<Bw64Reader> readFile(const std::string& filename) {
    return std::unique_ptr<Bw64Reader>(new Bw64Reader(filename.c_str()));
  }

  /**
   * @brief Open a BW64 file for writing
   *
   * Convenience function to open a new BW64 file for writing, adding `axml` and
   * `chna` chunks.
   *
   * If passed to this function, the `axml` and `chna` chunks will be added to
   * the BW64 file *before* the actual data chunk, which is the recommended
   * practice if all components are already known before writing a file.
   *
   * @param filename path of the file to write
   * @param channels the channel count of the new file
   * @param sampleRate the samplerate of the new file
   * @param bitDepth target bitdepth of the new file
   * @param chnaChunk Channel allocation chunk to include, if any
   * @param axmlChunk AXML chunk to include, if any
   *
   * @returns `unique_ptr` to a Bw64Writer instance that is ready to write
   * samples.
   *
   */
  inline std::unique_ptr<Bw64Writer> writeFile(
      const std::string& filename,
      uint16_t channels = 1u,
      uint32_t sampleRate = 48000u,
      uint16_t bitDepth = 24u,
      std::shared_ptr<ChnaChunk> chnaChunk = nullptr,
      std::shared_ptr<AxmlChunk> axmlChunk = nullptr) {
    std::vector<std::shared_ptr<Chunk>> preDataChunks;
    if (chnaChunk) {
      preDataChunks.push_back(chnaChunk);
    }
    if (axmlChunk) {
      preDataChunks.push_back(axmlChunk);
    }
    return std::unique_ptr<Bw64Writer>(new Bw64Writer(
        filename.c_str(), channels, sampleRate, bitDepth, preDataChunks));
  }


/**
 * @brief Create BW64 file for writing
 *
 * @return Shared pointer to Bw64Writer instance
 */
  inline std::shared_ptr<Bw64Writer> createSharedWriter(
      const std::string& filename,
      uint16_t channels = 1u,
      uint32_t sampleRate = 48000u,
      uint16_t bitDepth = 24u,
      std::shared_ptr<ChnaChunk> chnaChunk = nullptr,
      std::shared_ptr<AxmlChunk> axmlChunk = nullptr) {
    auto uniqueWriter = writeFile(filename, channels, sampleRate, bitDepth, chnaChunk, axmlChunk);
    return std::shared_ptr<Bw64Writer>(std::move(uniqueWriter));
  }


/**
 * @brief Create a BW64 file for writing
 *
 * Convenience function which accepts a vector of markers to add to the file.
 * The markers will be added to the file *before* the actual data chunk, which
 * is the recommended practice if all components are already known before writing
 *
 * @returns `shared_ptr` to a Bw64Writer instance that is ready to write samples
 */
  inline std::shared_ptr<Bw64Writer> createSharedWriterWithMarkers(
      const std::string& filename,
      uint16_t channels = 1u,
      uint32_t sampleRate = 48000u,
      uint16_t bitDepth = 24u,
      bool useExtensible = false,
      bool useFloat = false,
      uint32_t channelMask = 0,
      const std::vector<CuePoint>& markers = {},
      std::vector<std::shared_ptr<Chunk>> preDataChunks = {}) {

    bool gotChnaChunk = false;
    for (auto chunk : preDataChunks) {
      if (chunk->id() == utils::fourCC("chna")) {
        gotChnaChunk = true;
        break;
      }
    }
    if (!gotChnaChunk) {
      // Create a default CHNA chunk with one track per channel
      std::vector<AudioId> audioIds;
      for (uint16_t ch = 1; ch <= channels; ++ch) {
        char uid[13];
        char trackRef[16];
        std::snprintf(uid, 13, "ATU_%08d", ch);
        std::snprintf(trackRef, 16, "AT_000100%02d_01", ch);
        audioIds.emplace_back(ch, std::string(uid), std::string(trackRef), "AP_00010001");
      }
      preDataChunks.push_back(std::make_shared<ChnaChunk>(audioIds));
    }

    auto writer = std::shared_ptr<Bw64Writer>(new Bw64Writer(filename.c_str(), channels, sampleRate, bitDepth, preDataChunks, useExtensible, useFloat, channelMask, (uint32_t)markers.size()));

    for (const auto& cue : markers) {
      writer->addMarker(cue);
    }
    return writer;
  }


/**
 * @brief Create a BW64 file for writing
 *
 * Convenience function which specifies the maximum number of markers to add.
 * The markers will be added to the file *before* the actual data chunk, which
 * is the recommended practice if all components are already known before writing
 *
 * @returns `shared_ptr` to a Bw64Writer instance that is ready to write samples
 */
  inline std::shared_ptr<Bw64Writer> createSharedWriterWithMaxMarkers(
      const std::string& filename,
      uint16_t channels = 1u,
      uint32_t sampleRate = 48000u,
      uint16_t bitDepth = 24u,
      bool useExtensible = false,
      bool useFloat = false,
      uint32_t channelMask = 0,
      uint32_t maxMarkers = 0,
      std::vector<std::shared_ptr<Chunk>> preDataChunks = {}) {

    bool gotChnaChunk = false;
    for (auto chunk : preDataChunks) {
      if (chunk->id() == utils::fourCC("chna")) {
        gotChnaChunk = true;
        break;
      }
    }
    if (!gotChnaChunk) {
      // Create a default CHNA chunk with one track per channel
      std::vector<AudioId> audioIds;
      for (uint16_t ch = 1; ch <= channels; ++ch) {
        char uid[13];
        char trackRef[16];
        std::snprintf(uid, 13, "ATU_%08d", ch);
        std::snprintf(trackRef, 16, "AT_000100%02d_01", ch);
        audioIds.emplace_back(ch, std::string(uid), std::string(trackRef), "AP_00010001");
      }
      preDataChunks.push_back(std::make_shared<ChnaChunk>(audioIds));
    }

    auto writer = std::shared_ptr<Bw64Writer>(new Bw64Writer(filename.c_str(), channels, sampleRate, bitDepth, preDataChunks, useExtensible, useFloat, channelMask, maxMarkers));
    return writer;
  }

}  // namespace bw64
