/**
 * @file parser.hpp
 *
 * Collection of parser functions, which construct chunk objects from istreams.
 */
#pragma once
#include "chunks.hpp"
#include "chunks_ext.hpp"
#include "utils.hpp"

namespace bw64 {

  ///@brief Parse ExtraData from input stream
  inline std::shared_ptr<ExtraData> parseExtraData(std::istream& stream) {
    uint16_t validBitsPerSample;
    uint32_t dwChannelMask;
    bwGUID subFormat;
    utils::readValue(stream, validBitsPerSample);
    utils::readValue(stream, dwChannelMask);
    utils::readValue(stream, subFormat);
    return std::make_shared<ExtraData>(validBitsPerSample, dwChannelMask,
                                       subFormat);
  }

  /// @brief Parse FormatInfoChunk from input stream
  inline std::shared_ptr<FormatInfoChunk> parseFormatInfoChunk(
      std::istream& stream, uint32_t id, uint64_t size) {
    if (id != utils::fourCC("fmt ")) {
      std::stringstream errorString;
      errorString << "chunkId != 'fmt '";
      throw std::runtime_error(errorString.str());
    }
    if (size < 16) {
      throw std::runtime_error("'fmt ' chunk is too small");
    }

    uint16_t formatTag;
    uint16_t channelCount;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;
    uint16_t blockAlignment;
    uint16_t bitsPerSample;
    uint16_t cbSize;
    std::shared_ptr<ExtraData> extraData;

    utils::readValue(stream, formatTag);
    utils::readValue(stream, channelCount);
    utils::readValue(stream, sampleRate);
    utils::readValue(stream, bytesPerSecond);
    utils::readValue(stream, blockAlignment);
    utils::readValue(stream, bitsPerSample);

    if (size >= 18) {
      utils::readValue(stream, cbSize);

      if (size != 18 + cbSize) {
        throw std::runtime_error("fmt chunk is not as specified in cbSize");
      }
    } else {
      cbSize = 0;

      if (size != 16) {
        throw std::runtime_error("fmt chunk without cbSize should be 16 bytes");
      }
    }

    if (formatTag == WAVE_FORMAT_PCM || formatTag == WAVE_FORMAT_IEEE_FLOAT) {
      if (cbSize != 0) {
        throw std::runtime_error(
            "WAVE_FORMAT_PCM fmt chunk should not have extra data");
      }
    } else if (formatTag == WAVE_FORMAT_EXTENSIBLE) {
      if (cbSize != 22) {
        std::stringstream errorString;
        errorString << "WAVE_FORMAT_EXTENSIBLE fmt chunk must have 22 bytes of "
                       "extra data, but has "
                    << cbSize;
        throw std::runtime_error(errorString.str());
      }

      extraData = parseExtraData(stream);

      uint32_t Data1 = extraData->subFormat().Data1;
      if (Data1 != WAVE_FORMAT_PCM && Data1 != WAVE_FORMAT_IEEE_FLOAT) {
        throw std::runtime_error("subformat unsupported");
      }
    } else {
      std::stringstream errorString;
      errorString << "format unsupported: " << formatTag;
      throw std::runtime_error(errorString.str());
    }

    auto formatInfoChunk = std::make_shared<FormatInfoChunk>(
        channelCount, sampleRate, bitsPerSample, extraData, formatTag);

    if (formatInfoChunk->blockAlignment() != blockAlignment) {
      std::stringstream errorString;
      errorString << "sanity check failed. 'blockAlignment' is "
                  << blockAlignment << " but should be "
                  << formatInfoChunk->blockAlignment();
      throw std::runtime_error(errorString.str());
    }
    if (formatInfoChunk->bytesPerSecond() != bytesPerSecond) {
      std::stringstream errorString;
      errorString << "sanity check failed. 'bytesPerSecond' is "
                  << bytesPerSecond << " but should be "
                  << formatInfoChunk->bytesPerSecond();
      throw std::runtime_error(errorString.str());
    }

    return formatInfoChunk;
  }

  ///@brief Parse AxmlChunk from input stream
  inline std::shared_ptr<AxmlChunk> parseAxmlChunk(std::istream& stream,
                                                   uint32_t id, uint64_t size) {
    if (id != utils::fourCC("axml")) {
      std::stringstream errorString;
      errorString << "chunkId != 'axml'";
      throw std::runtime_error(errorString.str());
    }
    std::string data(size, 0);
    // since c++11, std::string[0] returns a valid reference to a null byte for
    // size==0
    utils::readChunk(stream, &data[0], size);
    return std::make_shared<AxmlChunk>(std::move(data));
  }

  ///@brief Parse AudioId from input stream
  inline AudioId parseAudioId(std::istream& stream) {
    uint16_t trackIndex;
    char uid[12];
    char trackRef[14];
    char packRef[11];

    utils::readValue(stream, trackIndex);
    utils::readValue(stream, uid);
    utils::readValue(stream, trackRef);
    utils::readValue(stream, packRef);
    stream.seekg(1, std::ios::cur);  // skip padding
    if (!stream.good())
      throw std::runtime_error("file error while seeking past audioId padding");

    return AudioId(trackIndex, std::string(uid, 12), std::string(trackRef, 14),
                   std::string(packRef, 11));
  }

  ///@brief Parse ChnaChunk from input stream
  inline std::shared_ptr<ChnaChunk> parseChnaChunk(std::istream& stream,
                                                   uint32_t id, uint64_t size) {
    if (id != utils::fourCC("chna")) {
      std::stringstream errorString;
      errorString << "chunkId != 'chna'";
      throw std::runtime_error(errorString.str());
    }
    if (size < 4) {
      throw std::runtime_error("illegal chna chunk size");
    }

    uint16_t numUids;
    uint16_t numTracks;
    utils::readValue(stream, numTracks);
    utils::readValue(stream, numUids);
    auto chnaChunk = std::make_shared<ChnaChunk>();
    for (int i = 0; i < numUids; ++i) {
      auto audioId = parseAudioId(stream);
      chnaChunk->addAudioId(audioId);
    }

    if (chnaChunk->numUids() != numUids) {
      std::stringstream errorString;
      errorString << "numUids != '" << chnaChunk->numUids() << "'";
      throw std::runtime_error(errorString.str());
    }
    if (chnaChunk->numTracks() != numTracks) {
      std::stringstream errorString;
      errorString << "numTracks != '" << chnaChunk->numTracks() << "'";
      throw std::runtime_error(errorString.str());
    }
    return chnaChunk;
  }

  /// @brief Construct DataSize64Chunk from input stream
  inline std::shared_ptr<DataSize64Chunk> parseDataSize64Chunk(
      std::istream& stream, uint32_t id, uint64_t size) {
    if (id != utils::fourCC("ds64")) {
      std::stringstream errorString;
      errorString << "chunkId != 'ds64'";
      throw std::runtime_error(errorString.str());
    }

    // chunk consists of a fixed-size header, tableLength table entries, and
    // optionally some junk
    const uint64_t headerLength = 28u;
    const uint64_t tableEntryLength = 12u;
    if (size < headerLength) {
      throw std::runtime_error("illegal ds64 chunk size");
    }

    uint32_t tableLength;
    uint64_t bw64Size;
    uint64_t dataSize;
    uint64_t dummySize;
    utils::readValue(stream, bw64Size);
    utils::readValue(stream, dataSize);
    utils::readValue(stream, dummySize);
    utils::readValue(stream, tableLength);

    const uint64_t minSize = headerLength + tableLength * tableEntryLength;
    if (size < minSize) {
      throw std::runtime_error("ds64 chunk too short to hold table entries");
    }

    std::map<uint32_t, uint64_t> table;
    for (uint32_t i = 0; i < tableLength; ++i) {
      uint32_t id;
      uint64_t size;
      utils::readValue(stream, id);
      utils::readValue(stream, size);
      table[id] = size;
    }
    // skip junk data
    stream.seekg(size - minSize, std::ios::cur);
    if (!stream.good())
      throw std::runtime_error("file error while seeking past ds64 chunk");

    return std::make_shared<DataSize64Chunk>(bw64Size, dataSize, table);
  }

  inline std::shared_ptr<DataChunk> parseDataChunk(std::istream& /* stream */,
                                                   uint32_t id, uint64_t size) {
    if (id != utils::fourCC("data")) {
      std::stringstream errorString;
      errorString << "chunkId != 'data'";
      throw std::runtime_error(errorString.str());
    }
    auto dataChunk = std::make_shared<DataChunk>();
    dataChunk->setSize(size);
    return dataChunk;
  }

/**
 * @brief Parser function for cue chunk
 */
  inline std::shared_ptr<CueChunk> parseCueChunk(std::istream& stream, uint32_t id, uint64_t size) {
    if (id != bw64::utils::fourCC("cue ")) {
      std::stringstream errorString;
      errorString << "chunkId != 'cue '";
      throw std::runtime_error(errorString.str());
    }

    if (size < 4) {
      throw std::runtime_error("Cue chunk too small");
    }

    uint32_t numCuePoints;
    bw64::utils::readValue(stream, numCuePoints);

    if (size != 4 + numCuePoints * 24) {
      throw std::runtime_error("Incorrect cue chunk size");
    }

    std::vector<CuePoint> cuePoints;
    for (uint32_t i = 0; i < numCuePoints; i++) {
      CuePoint cue;
      bw64::utils::readValue(stream, cue.id);
      bw64::utils::readValue(stream, cue.position);
      bw64::utils::readValue(stream, cue.dataChunkId);
      bw64::utils::readValue(stream, cue.chunkStart);
      bw64::utils::readValue(stream, cue.blockStart);
      bw64::utils::readValue(stream, cue.sampleOffset);
      cuePoints.push_back(cue);
    }

    return std::make_shared<CueChunk>(cuePoints);
  }

/**
 * @brief Parser function for label chunk
 */
  inline std::shared_ptr<LabelChunk> parseLabelChunk(std::istream& stream, uint32_t id, uint64_t size) {
    if (id != bw64::utils::fourCC("labl")) {
      throw std::runtime_error("chunkId != 'labl'");
    }

    if (size < 5) { // At least 4 bytes for ID + 1 byte for null terminator
      throw std::runtime_error("Label chunk too small");
    }

    uint32_t cuePointId;
    bw64::utils::readValue(stream, cuePointId);

    // Read the null-terminated string
    std::string label;
    label.resize(size - 4); // Allocate space for the string excluding the cue point ID

    stream.read(&label[0], size - 4);

    // Remove null terminator and any extra padding
    size_t nullPos = label.find('\0');
    if (nullPos != std::string::npos) {
      label.resize(nullPos);
    }

    return std::make_shared<LabelChunk>(cuePointId, label);
  }

/**
 * @brief Parser function for LIST chunk
 */
  inline std::shared_ptr<ListChunk> parseListChunk(std::istream& stream, uint32_t id, uint64_t size) {
    if (id != bw64::utils::fourCC("LIST")) {
      throw std::runtime_error("chunkId != 'LIST'");
    }

    if (size < 4) {
      throw std::runtime_error("LIST chunk too small");
    }

    uint32_t listType;
    utils::readValue(stream, listType);

    std::vector<std::shared_ptr<Chunk>> subChunks;
    uint64_t bytesRead = 4; // Already got the list type (4 bytes)

    while (bytesRead < size) {
      uint32_t subChunkId;
      uint32_t subChunkSize;

      utils::readValue(stream, subChunkId);
      utils::readValue(stream, subChunkSize);
      bytesRead += 8; // 4 bytes for each

      std::shared_ptr<Chunk> subChunk;
      if (subChunkId == utils::fourCC("labl")) {
        subChunk = parseLabelChunk(stream, subChunkId, subChunkSize);
        bytesRead += subChunkSize;
      } else {
        // Unknown chunks
        stream.seekg(subChunkSize, std::ios::cur);
        subChunk = std::make_shared<UnknownChunk>(subChunkId);
        bytesRead += subChunkSize;
      }

      subChunks.push_back(subChunk);

      if (subChunkSize % 2 == 1) {
        stream.seekg(1, std::ios::cur);
        bytesRead += 1;
      }
    }

    return std::make_shared<ListChunk>(listType, subChunks);
  }


  inline std::shared_ptr<Chunk> parseChunk(std::istream& stream,
                                           ChunkHeader header) {
    stream.clear();
    stream.seekg(header.position + 8u);
    if (!stream.good())
      throw std::runtime_error(
          "file error while seeking past chunk header chunk");

    if (header.id == utils::fourCC("ds64")) {
      return parseDataSize64Chunk(stream, header.id, header.size);
    } else if (header.id == utils::fourCC("fmt ")) {
      return parseFormatInfoChunk(stream, header.id, header.size);
    } else if (header.id == utils::fourCC("axml")) {
      return parseAxmlChunk(stream, header.id, header.size);
    } else if (header.id == utils::fourCC("chna")) {
      return parseChnaChunk(stream, header.id, header.size);
    } else if (header.id == utils::fourCC("data")) {
      return parseDataChunk(stream, header.id, header.size);
    } else if (header.id == utils::fourCC("cue ")) {
      return parseCueChunk(stream, header.id, header.size);
    } else if (header.id == utils::fourCC("LIST")) {
      return parseListChunk(stream, header.id, header.size);
    } else {
      return std::make_shared<UnknownChunk>(stream, header.id, header.size);
    }
  }

}  // namespace bw64
