/// @file reader.hpp
#pragma once
#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <vector>
#include "chunks.hpp"
#include "chunks_ext.hpp"
#include "utils.hpp"
#include "parser.hpp"

#include <iostream>

namespace bw64 {

  /**
   * @brief Representation of a BW64 file
   *
   * Normally, you will create an instance of this class using bw64::readFile().
   *
   * This is a
   * [RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization)
   * class, meaning that the file will be openend and initialized (parse header,
   * format etc.) on construction, and closed on destruction.
   */
  class Bw64Reader {
   public:
    /**
     * @brief Open a new BW64 file for reading
     *
     * Opens a new BW64 file for reading, parses the whole file to read the
     * format and identify all chunks in it.
     *
     * @note For convenience, you might consider using the `readFile` helper
     * function.
     */
    Bw64Reader(const char* filename) {
      fileStream_.open(filename, std::fstream::in | std::fstream::binary);
      if (!fileStream_.is_open()) {
        std::stringstream errorString;
        errorString << "Could not open file: " << filename;
        throw std::runtime_error(errorString.str());
      }
      readRiffChunk();
      if (fileFormat_ == utils::fourCC("BW64") ||
          fileFormat_ == utils::fourCC("RF64")) {
        auto chunkHeader = parseHeader();
        if (chunkHeader.id != utils::fourCC("ds64")) {
          throw std::runtime_error(
              "mandatory ds64 chunk for BW64 or RF64 file not found");
        }
        auto ds64Chunk =
            parseDataSize64Chunk(fileStream_, chunkHeader.id, chunkHeader.size);
        chunks_.push_back(ds64Chunk);
        chunkHeaders_.push_back(chunkHeader);
      }
      parseChunkHeaders();
      for (auto chunkHeader : chunkHeaders_) {
        if (chunkHeader.id != utils::fourCC("ds64")) {
          auto chunk = parseChunk(fileStream_, chunkHeader);
          chunks_.push_back(chunk);
        }
      }

      auto fmtChunk = formatChunk();
      if (!fmtChunk) {
        throw std::runtime_error("mandatory fmt chunk not found");
      }
      channelCount_ = fmtChunk->channelCount();
      formatTag_ = fmtChunk->formatTag();
      sampleRate_ = fmtChunk->sampleRate();
      bitsPerSample_ = fmtChunk->bitsPerSample();

      if (!dataChunk()) {
        throw std::runtime_error("mandatory data chunk not found");
      }
      
      associateCueLabels();
      seek(0);
    }

    /// close the file
    ///
    /// It is recommended to call this before the destructor, to handle
    /// exceptions.
    void close() {
      if (!fileStream_.is_open()) return;

      fileStream_.close();

      if (!fileStream_.good())
        throw std::runtime_error("file error detected when closing");
    }

    /// destructor; this will close the file if it has not already been done,
    /// but it is recommended to call close() first to handle exceptions
    ~Bw64Reader() { close(); }

    /// @brief Get file format (RIFF, BW64 or RF64)
    uint32_t fileFormat() const { return fileFormat_; }
    /// @brief Get file size
    uint32_t fileSize() const { return fileSize_; }
    /// @brief Get format tag
    uint16_t formatTag() const { return formatTag_; };
    /// @brief Get number of channels
    uint16_t channels() const { return channelCount_; };
    /// @brief Get sample rate
    uint32_t sampleRate() const { return sampleRate_; };
    /// @brief Get bit depth
    uint16_t bitDepth() const { return bitsPerSample_; };
    /// @brief Get number of frames
    uint64_t numberOfFrames() const {
      return dataChunk()->size() / blockAlignment();
    }
    /// @brief Get block alignment
    uint16_t blockAlignment() const {
      return utils::safeCast<uint16_t>(static_cast<uint32_t>(channels()) *
                                       bitDepth() / 8);
    }

    template <typename ChunkType>
    std::vector<std::shared_ptr<ChunkType>> chunksWithId(
        const std::vector<Chunk>& chunks, uint32_t chunkId) const {
      std::vector<char> foundChunks;
      auto chunk =
          std::copy_if(chunks.begin(), chunks.end(), foundChunks.begin(),
                       [chunkId](const std::shared_ptr<Chunk> chunk) {
                         return chunk->id() == chunkId;
                       });
      return foundChunks;
    }

    template <typename ChunkType>
    std::shared_ptr<ChunkType> chunk(
        const std::vector<std::shared_ptr<Chunk>>& chunks,
        uint32_t chunkId) const {
      auto chunk = std::find_if(chunks.begin(), chunks.end(),
                                [chunkId](const std::shared_ptr<Chunk> chunk) {
                                  return chunk->id() == chunkId;
                                });
      if (chunk != chunks.end()) {
        return std::static_pointer_cast<ChunkType>(*chunk);
      } else {
        return nullptr;
      }
    }

    /**
     * @brief Get 'ds64' chunk
     *
     * @returns `std::shared_ptr` to DataSize64Chunk if present and otherwise
     * a nullptr.
     */
    std::shared_ptr<DataSize64Chunk> ds64Chunk() const {
      return chunk<DataSize64Chunk>(chunks_, utils::fourCC("ds64"));
    }
    /**
     * @brief Get 'fmt ' chunk
     *
     * @returns `std::shared_ptr` to FormatInfoChunk if present and otherwise
     * a nullptr.
     */
    std::shared_ptr<FormatInfoChunk> formatChunk() const {
      return chunk<FormatInfoChunk>(chunks_, utils::fourCC("fmt "));
    }
    /**
     * @brief Get 'data' chunk
     *
     * @warning This method usually should not be called, as the acces to the
     * DataChunk is handled seperately by the Bw64Reader class .
     *
     * @returns `std::shared_ptr` to DataChunk if present and otherwise
     * a nullptr.
     */
    std::shared_ptr<DataChunk> dataChunk() const {
      return chunk<DataChunk>(chunks_, utils::fourCC("data"));
    }
    /**
     * @brief Get 'chna' chunk
     *
     * @returns `std::shared_ptr` to ChnaChunk if present and otherwise a
     * nullptr.
     */
    std::shared_ptr<ChnaChunk> chnaChunk() const {
      return chunk<ChnaChunk>(chunks_, utils::fourCC("chna"));
    }
    /**
     * @brief Get 'axml' chunk
     *
     * @returns `std::shared_ptr` to AxmlChunk if present and otherwise a
     * nullptr.
     */
    std::shared_ptr<AxmlChunk> axmlChunk() const {
      return chunk<AxmlChunk>(chunks_, utils::fourCC("axml"));
    }
    /**
     * @brief Get 'cue ' chunk
     *
     * @returns `std::shared_ptr` to CueChunk if present and otherwise a
     * nullptr.
     */
    std::shared_ptr<CueChunk> getCueChunk() const {
      return chunk<CueChunk>(chunks_, utils::fourCC("cue "));
    }


    /**
     * @brief Get list of all chunks which are present in the file
     */
    std::vector<ChunkHeader> chunks() const { return chunkHeaders_; }

    /**
     * @brief Check if a chunk with the given id is present
     */
    bool hasChunk(uint32_t id) const {
      auto foundHeader = std::find_if(
          chunkHeaders_.begin(), chunkHeaders_.end(),
          [id](const ChunkHeader header) { return header.id == id; });
      if (foundHeader == chunkHeaders_.end()) {
        return false;
      } else {
        return true;
      }
    }

    /**
     * @brief Seek a frame position in the DataChunk
     */
    void seek(int64_t offset, std::ios_base::seekdir way = std::ios::beg) {
      auto numberOfFramesInt = utils::safeCast<int64_t>(numberOfFrames());

      // where to seek relative to according to way
      int64_t startFrame = 0;
      if (way == std::ios::cur) {
        startFrame = tell();
      } else if (way == std::ios::beg) {
        startFrame = 0;
      } else if (way == std::ios::end) {
        startFrame = numberOfFramesInt;
      }

      // requested frame number, clamped to a frame within the data chunk
      int64_t frame = startFrame + offset;
      if (frame < 0)
        frame = 0;
      else if (frame > numberOfFramesInt)
        frame = numberOfFramesInt;

      // the position in the file of the frame
      const int64_t framePos =
          dataStartPos() + frame * static_cast<int64_t>(blockAlignment());

      fileStream_.seekg(framePos);

      if (!fileStream_.good())
        throw std::runtime_error("file error while seeking");
    }

    /**
     * @brief Read frames from dataChunk
     *
     * @param[out] outBuffer Buffer to write the samples to
     * @param[in]  frames    Number of frames to read
     *
     * @returns number of frames read
     */
    template <typename T, typename std::enable_if<
                              std::is_floating_point<T>::value, int>::type = 0>
    uint64_t read(T* outBuffer, uint64_t frames) {
      if (tell() + frames > numberOfFrames()) {
        frames = numberOfFrames() - tell();
      }

      if (frames) {
        rawDataBuffer_.resize(frames * blockAlignment());
        fileStream_.read(rawDataBuffer_.data(), frames * blockAlignment());
        if (fileStream_.eof())
          throw std::runtime_error("file ended while reading frames");
        if (!fileStream_.good())
          throw std::runtime_error("file error while reading frames");

        if (formatChunk()->isFloat()) {
          utils::decodeFloatSamples(rawDataBuffer_.data(), outBuffer,
                                    frames * channels(), bitDepth());
        } else {
          utils::decodePcmSamples(rawDataBuffer_.data(), outBuffer,
                                  frames * channels(), bitDepth());
        }
      }

      return frames;
    }

    /**
     * @brief Read raw frames from dataChunk
     *
     * @param[out] outBuffer Buffer to write the samples to
     * @param[in]  frames    Number of frames to read
     *
     * @returns number of frames read
     * @discussion outBuffer must match the wave formats internal
     *   type (16, 24, or 32 bit), (int or float).
     */
    template <typename T>
    uint64_t readRaw(T* outBuffer, uint64_t frames) {
      if (frames > numberOfFrames() - tell()) {
        frames = numberOfFrames() - tell();
      }

      if (frames) {
        fileStream_.read((char *)outBuffer, frames * blockAlignment());
        if (fileStream_.eof())
          throw std::runtime_error("file ended while reading frames");
        if (!fileStream_.good())
          throw std::runtime_error("file error while reading frames");
      }

      return frames;
    }

    /**
     * @brief Tell the current frame position of the dataChunk
     *
     * @returns current frame position of the dataChunk
     */
    uint64_t tell() {
      return ((uint64_t)fileStream_.tellg() - dataStartPos()) / formatChunk()->blockAlignment();
    }

    /**
     * @brief Check if end of data is reached
     *
     * @returns `true` if end of data is reached and otherwise `false`
     */
    bool eof() { return tell() == numberOfFrames(); }

    
    uint64_t dataStartPos() {
      return getChunkHeader(utils::fourCC("data")).position + 8u;
    }

    /**
     * @brief Get all LIST chunks in the file
     *
     * @returns a vector of shared pointers to all LIST chunks in the file
     */
    std::vector<std::shared_ptr<ListChunk>> getListChunks() const {
      std::vector<std::shared_ptr<ListChunk>> result;
      for (const auto& chunk : chunks_) {
        if (chunk->id() == utils::fourCC("LIST")) {
          auto listChunk = std::static_pointer_cast<ListChunk>(chunk);
          result.push_back(listChunk);
        }
      }
      return result;
    }


    /**
     * @brief Get all 'labl' chunks in the file
     */
    // In Bw64Reader
    std::vector<CuePoint> getMarkers() const {
      // First, get all cue points
      auto cueChunkPtr = getCueChunk();
      if (!cueChunkPtr) {
        return {}; // No cue points found
      }

      // Get a copy of all cue points from the cue chunk
      std::vector<CuePoint> markers = cueChunkPtr->cuePoints();

      // Now look for labels in LIST chunks and add them to the cue points
      for (const auto& chunk : chunks_) {
        if (chunk->id() == utils::fourCC("LIST")) {
          auto listChunk = std::static_pointer_cast<ListChunk>(chunk);

          // Check if it's an 'adtl' list
          if (listChunk->listType() == utils::fourCC("adtl")) {
            // Look through sub-chunks for labels
            for (const auto& subChunk : listChunk->subChunks()) {
              if (subChunk->id() == utils::fourCC("labl")) {
                auto labelChunk = std::static_pointer_cast<LabelChunk>(subChunk);

                // Find the corresponding marker and set its label
                uint32_t cueId = labelChunk->cuePointId();
                for (auto& marker : markers) {
                  if (marker.id == cueId) {
                    marker.label = labelChunk->label();
                    break; // Found the right marker
                  }
                }
              }
            }
          }
        }
      }

      return markers;
    }

    /// Find a marker by its ID
    const CuePoint* findMarkerById(uint32_t id) const {
      auto cueChunkPtr = getCueChunk();
      if (!cueChunkPtr) {
          return nullptr; // No cue points
      }

      const auto& cuePoints = cueChunkPtr->cuePoints();

      auto it = std::find_if(cuePoints.begin(), cuePoints.end(),
                             [id](const CuePoint& cp) { return cp.id == id; });

      return (it != cuePoints.end()) ? &(*it) : nullptr;
    }

   private:
    void readRiffChunk() {
      uint32_t riffType;
      utils::readValue(fileStream_, fileFormat_);
      utils::readValue(fileStream_, fileSize_);
      utils::readValue(fileStream_, riffType);

      if (fileFormat_ != utils::fourCC("RIFF") &&
          fileFormat_ != utils::fourCC("BW64") &&
          fileFormat_ != utils::fourCC("RF64")) {
        throw std::runtime_error("File is not a RIFF, BW64 or RF64 file.");
      }
      if (riffType != utils::fourCC("WAVE")) {
        throw std::runtime_error("File is not a WAVE file.");
      }
    }

    ChunkHeader getChunkHeader(uint32_t id) {
      auto foundHeader = std::find_if(
          chunkHeaders_.begin(), chunkHeaders_.end(),
          [id](const ChunkHeader header) { return header.id == id; });
      if (foundHeader != chunkHeaders_.end()) {
        return *foundHeader;
      }
      std::stringstream errorMsg;
      errorMsg << "no chunk with id '" << utils::fourCCToStr(id) << "' found";
      throw std::runtime_error(errorMsg.str());
    }

    ChunkHeader parseHeader() {
      uint32_t chunkId;
      uint32_t chunkSize;
      uint64_t position = fileStream_.tellg();
      utils::readValue(fileStream_, chunkId);
      utils::readValue(fileStream_, chunkSize);
      uint64_t chunkSize64 = getChunkSize64(chunkId, chunkSize);
      return ChunkHeader(chunkId, chunkSize64, position);
    }

    uint64_t getChunkSize64(uint32_t id, uint64_t chunkSize) {
      if (ds64Chunk()) {
        if (id == utils::fourCC("BW64") || id == utils::fourCC("RF64")) {
          return ds64Chunk()->bw64Size();
        }
        if (id == utils::fourCC("data")) {
          return ds64Chunk()->dataSize();
        }
        if (ds64Chunk()->hasChunkSize(id)) {
          return ds64Chunk()->getChunkSize(id);
        }
      }
      return chunkSize;
    }

    void parseChunkHeaders() {
      // get the absolute end of the file
      const std::streamoff start = fileStream_.tellg();
      fileStream_.seekg(0, std::ios::end);
      const std::streamoff end = fileStream_.tellg();
      fileStream_.seekg(start, std::ios::beg);

      const std::streamoff header_size = 8;

      while (fileStream_.tellg() + header_size <= end) {
        auto chunkHeader = parseHeader();

        // determine chunk size, skipping a padding byte
        std::streamoff chunk_size =
        utils::safeCast<std::streamoff>(chunkHeader.size);

        std::streamoff chunk_end =
            utils::safeAdd<std::streamoff>(fileStream_.tellg(), chunk_size);

        if (chunk_end > end)
          throw std::runtime_error("chunk ends after end of file");

        chunkHeaders_.push_back(chunkHeader);

        if (chunk_end < end) {
          if (chunk_size % 2 != 0)
            chunk_size = utils::safeAdd<std::streamoff>(chunk_size, 1);
        }
        fileStream_.seekg(chunk_size, std::ios::cur);
        if (!fileStream_.good())
          throw std::runtime_error("file error while seeking past chunk");
      }
    }
    
    void associateCueLabels() {
      // Get all cue points
      auto cueChunkPtr = getCueChunk();
      if (!cueChunkPtr) {
        return; // No cue points found
      }
      
      // Find all LIST chunks with labels
      std::map<uint32_t, std::string> cueLabels;
      
      for (const auto& chunk : chunks_) {
        if (chunk->id() == utils::fourCC("LIST")) {
          auto listChunk = std::static_pointer_cast<ListChunk>(chunk);
          if (listChunk->listType() == utils::fourCC("adtl")) {
            for (const auto& subChunk : listChunk->subChunks()) {
              if (subChunk->id() == utils::fourCC("labl")) {
                auto labelChunk = std::static_pointer_cast<LabelChunk>(subChunk);
                cueLabels[labelChunk->cuePointId()] = labelChunk->label();
              }
            }
          }
        }
      }
      
      // Now update the labels in the original CuePoints
      for (auto& cuePoint : cueChunkPtr->cuePointsRef()) {
        auto it = cueLabels.find(cuePoint.id);
        if (it != cueLabels.end()) {
          cuePoint.label = it->second;
        }
      }
    }

    std::ifstream fileStream_;
    uint32_t fileFormat_;
    uint32_t fileSize_;
    uint16_t channelCount_;
    uint32_t sampleRate_;
    uint16_t formatTag_;
    uint16_t bitsPerSample_;

    std::vector<char> rawDataBuffer_;
    std::vector<std::shared_ptr<Chunk>> chunks_;
    std::vector<ChunkHeader> chunkHeaders_;
  };
}  // namespace bw64
