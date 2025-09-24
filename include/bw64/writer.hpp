/// @file writer.hpp
#pragma once
#include <algorithm>
#include <fstream>
#include <limits>
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

namespace bw64 {

  const uint32_t MAX_NUMBER_OF_UIDS = 1024;

  /**
   * @brief BW64 Writer class
   *
   * Normally, you will create an instance of this class using
   * bw64::writeFile().
   *
   * This is a
   * [RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization)
   * class, meaning that the file will be opened and initialized (required
   * headers etc.) on construction, and closed and finalized (writing chunk
   * sizes etc.) on destruction.
   */
  class Bw64Writer {
   public:
    /**
     * @brief Open a new BW64 file for writing
     *
     * Opens a new BW64 file for writing, initializes everything up to the
     * `data` chunk. Afterwards, you may write interleaved audio samples to this
     * file.
     *
     * @warning If the file already exists it will be overwritten.
     *
     * If you need any chunks to appear *before* the data chunk, include them in
     * the `preDataChunks`. They will be written directly after opening the
     * file.
     *
     * @note For convenience, you might consider using the `writeFile` helper
     * function.
     */
    Bw64Writer(const char* filename,
               uint16_t channels,
               uint32_t sampleRate,
               uint16_t bitDepth,
               std::vector<std::shared_ptr<Chunk>> preDataChunks,
               bool useExtensible = false,
               bool useFloat = false,
               uint32_t channelMask = 0,
               uint32_t maxMarkers = 0) {
      fileStream_.open(filename, std::fstream::out | std::fstream::binary);
      if (!fileStream_.is_open()) {
        std::stringstream errorString;
        errorString << "Could not open file: " << filename;
        throw std::runtime_error(errorString.str());
      }
      writeRiffHeader();

      // 28 byte ds64 header + 12 byte entry for axml
      writeChunkPlaceholder(utils::fourCC("JUNK"), 40u);

      if (useExtensible) {
        uint32_t correctedChannelMask = utils::correctChannelMask(channelMask, channels);
        auto formatChunk = std::make_shared<FormatInfoChunk>(channels, sampleRate, bitDepth,
          std::make_shared<ExtraData>(bitDepth, correctedChannelMask,
            useFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM),
          WAVE_FORMAT_EXTENSIBLE);
        writeChunk(formatChunk);
      } else {
        auto formatChunk = std::make_shared<FormatInfoChunk>(channels, sampleRate, bitDepth,
          nullptr,
          useFloat ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM);
        writeChunk(formatChunk);
      }

      for (auto chunk : preDataChunks) {
        writeChunk(chunk);
      }

      // Write CueChunk placeholder
      if (maxMarkers > 0) {
        std::vector<CuePoint> emptyCuePoints(maxMarkers, CuePoint{});
        auto cueChunk = std::make_shared<CueChunk>(emptyCuePoints);
        writeChunk(cueChunk);
        cueChunk->clearCuePoints();
      }

      // Write CHNA chunk placeholder
      if (!chnaChunk()) {
        writeChunkPlaceholder(utils::fourCC("chna"),
                              MAX_NUMBER_OF_UIDS * 40 + 4);
      }

      // Write the data chunk header
      auto dataChunk = std::make_shared<DataChunk>();
      writeChunk(dataChunk);
    }

    /// finalise and close the file
    ///
    /// Write all yet-to-be-written chunks to the file and finalize all
    /// required information, i.e. the final chunk sizes etc.
    ///
    /// It is recommended to call this before the destructor, to handle
    /// exceptions. If it does throw, this object may be in an invalid state,
    /// so do not try again without creating a new object.
    void close() {
      if (!fileStream_.is_open()) return;

      try {
        finalizeDataChunk();
        finalizeCueChunk(); // finalize cue chunk before writing post data chunks
        for (auto chunk : postDataChunks_) {
          writeChunk(chunk);
        }
        finalizeRiffChunk();
        fileStream_.close();
      } catch (...) {
        // ensure that if an exception is thrown the file is still closed, so
        // the destructor does not throw the same exception
        fileStream_.close();
        throw;
      }

      if (!fileStream_.good())
        throw std::runtime_error("file error detected when closing");
    }

    /// destructor; this will finalise and close the file if it has not
    /// already been done, but it is recommended to call close() first to
    /// handle exceptions
    ~Bw64Writer() { close(); }

    /// @brief Get format tag
    uint16_t formatTag() const { return formatChunk()->formatTag(); };
    /// @brief Get number of channels
    uint16_t channels() const { return formatChunk()->channelCount(); };
    /// @brief Get sample rate
    uint32_t sampleRate() const { return formatChunk()->sampleRate(); };
    /// @brief Get bit depth
    uint16_t bitDepth() const { return formatChunk()->bitsPerSample(); };
    /// @brief Get number of frames
    uint64_t framesWritten() const {
      return dataChunk()->size() / formatChunk()->blockAlignment();
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

    std::shared_ptr<DataSize64Chunk> ds64Chunk() const {
      return chunk<DataSize64Chunk>(chunks_, utils::fourCC("ds64"));
    }
    std::shared_ptr<FormatInfoChunk> formatChunk() const {
      return chunk<FormatInfoChunk>(chunks_, utils::fourCC("fmt "));
    }
    std::shared_ptr<DataChunk> dataChunk() const {
      return chunk<DataChunk>(chunks_, utils::fourCC("data"));
    }
    std::shared_ptr<ChnaChunk> chnaChunk() const {
      return chunk<ChnaChunk>(chunks_, utils::fourCC("chna"));
    }
    std::shared_ptr<AxmlChunk> axmlChunk() const {
      return chunk<AxmlChunk>(chunks_, utils::fourCC("axml"));
    }
    std::shared_ptr<CueChunk> cueChunk() const {
      return chunk<CueChunk>(chunks_, utils::fourCC("cue "));
    }

    /// @brief Check if file is bigger than 4GB and therefore a BW64 file
    bool isBw64File() {
      if (riffChunkSize() > UINT32_MAX) {
        return true;
      }

      for (auto& header : chunkHeaders_)
        if (header.size > UINT32_MAX) return true;

      return false;
    }

    /// @brief Use RF64 ID for outer chunk (when >4GB) rather than BW64
    void useRf64Id(bool state) { useRf64Id_ = state; }

    void setChnaChunk(std::shared_ptr<ChnaChunk> chunk) {
      if (chunk->numUids() > 1024) {
        // TODO: make pre data chunk chna chunk a JUNK chunk and add chnaChunk
        // to postDataChunks_?
        throw std::runtime_error("number of trackUids is > 1024");
      }
      auto last_position = fileStream_.tellp();
      overwriteChunk(utils::fourCC("chna"), chunk);
      fileStream_.seekp(last_position);
    }

    void setAxmlChunk(std::shared_ptr<Chunk> chunk) {
      postDataChunks_.push_back(chunk);
    }

    /// @brief Adds a chunk to be written after the data chunk.
    void postDataChunk(std::shared_ptr<Chunk> chunk) {
      postDataChunks_.push_back(chunk);
    }

    /// @brief Get the chunk size for header
    /// Now that we support multiple chunks with the same ID,
    /// this should be renamed to something like clampedChunkSize
    uint32_t chunkSizeForHeader(std::shared_ptr<Chunk> chunk) {
      if (chunk->size() >= UINT32_MAX) {
        return UINT32_MAX;
      } else {
        return static_cast<uint32_t>(chunk->size());
      }
    }

    /// @brief Calculate riff chunk size
    uint64_t riffChunkSize() {
      auto last_position = fileStream_.tellp();
      fileStream_.seekp(0, std::ios::end);
      uint64_t endPos = fileStream_.tellp();
      fileStream_.seekp(last_position);
      return endPos - 8u;
    }

    /// @brief Write RIFF header
    void writeRiffHeader() {
      uint32_t RiffId = utils::fourCC("RIFF");
      uint32_t fileSize = UINT32_MAX;
      uint32_t WaveId = utils::fourCC("WAVE");
      utils::writeValue(fileStream_, RiffId);
      utils::writeValue(fileStream_, fileSize);
      utils::writeValue(fileStream_, WaveId);
    }

    /// @brief Update RIFF header
    void finalizeRiffChunk() {
      auto last_position = fileStream_.tellp();
      fileStream_.seekp(0);
      if (isBw64File()) {
        utils::writeValue(fileStream_,
                          utils::fourCC(useRf64Id_ ? "RF64" : "BW64"));
        utils::writeValue(fileStream_, (std::numeric_limits<uint32_t>::max)());
        overwriteJunkWithDs64Chunk();
      } else {
        utils::writeValue(fileStream_, utils::fourCC("RIFF"));
        uint32_t fileSize = static_cast<uint32_t>(riffChunkSize());
        utils::writeValue(fileStream_, fileSize);
      }
      fileStream_.seekp(last_position);
    }


    /// @brief Update Cue chunk
    void finalizeCueChunk() {
      auto cueChunkPtr = cueChunk();
      if (cueChunkPtr && !cueChunkPtr->cuePoints().empty()) {
        auto labels = cueChunkPtr->getLabels();

        // If we have labels, create a LIST chunk
        if (!labels.empty()) {
          std::vector<std::shared_ptr<Chunk>> labelChunks;
          for (const auto& label : labels) {
            auto labelChunk = std::make_shared<LabelChunk>(label.first, label.second);
            labelChunks.push_back(labelChunk);
          }

          auto listChunk = std::make_shared<ListChunk>(utils::fourCC("adtl"), labelChunks);
          postDataChunks_.push_back(listChunk);
        }

        // Overwrite the cue chunk with its current content
        overwriteChunk(utils::fourCC("cue "), cueChunkPtr);
      }
    }


    void overwriteJunkWithDs64Chunk() {
      auto ds64Chunk = std::make_shared<DataSize64Chunk>();
      ds64Chunk->bw64Size(riffChunkSize());
      // write data size even if it's not too big
      ds64Chunk->dataSize(dataChunk()->size());

      for (auto& header : chunkHeaders_)
        if (header.size > UINT32_MAX)
          ds64Chunk->setChunkSize(header.id, header.size);

      overwriteChunk(utils::fourCC("JUNK"), ds64Chunk);
    }

    void finalizeDataChunk() {
      if (dataChunk()->size() % 2 == 1) {
        utils::writeValue(fileStream_, '\0');
      }
      auto last_position = fileStream_.tellp();
      seekChunk(utils::fourCC("data"));
      utils::writeValue(fileStream_, utils::fourCC("data"));
      utils::writeValue(fileStream_, chunkSizeForHeader(dataChunk()));
      fileStream_.seekp(last_position);
    }

    /// @brief Write chunk template
    template <typename ChunkType>
    void writeChunk(std::shared_ptr<ChunkType> chunk) {
      if (chunk) {
        uint64_t position = fileStream_.tellp();
        chunkHeaders_.push_back(
            ChunkHeader(chunk->id(), chunk->size(), position));
        utils::writeChunk<ChunkType>(fileStream_, chunk,
                                     chunkSizeForHeader(chunk));
        chunks_.push_back(chunk);
      }
    }

    void writeChunkPlaceholder(uint32_t id, uint32_t size) {
      uint64_t position = fileStream_.tellp();
      chunkHeaders_.push_back(ChunkHeader(id, size, position));
      utils::writeChunkPlaceholder(fileStream_, id, size);
    }

    /// @brief Overwrite chunk template
    template <typename ChunkType>
    void overwriteChunk(uint32_t id, std::shared_ptr<ChunkType> chunk) {
      if (chunk->size() > chunkHeader(id).size) { // only works for uniquely id'd chunks
        std::stringstream errorMsg;
        errorMsg << utils::fourCCToStr(chunk->id()) << " chunk is too large ("
                 << chunk->size() << " bytes) to overwrite "
                 << utils::fourCCToStr(id) << " chunk (" << chunkHeader(id).size
                 << " bytes)";
        throw std::runtime_error(errorMsg.str());
      }

      auto last_position = fileStream_.tellp();
      seekChunk(id); // only works for uniquely id'd chunks
      utils::writeChunk<ChunkType>(fileStream_, chunk, chunkSizeForHeader(chunk));
      fileStream_.seekp(last_position);
    }

    void seekChunk(uint32_t id) {
      auto header = chunkHeader(id);
      fileStream_.clear();
      fileStream_.seekp(header.position);
    }

    ChunkHeader& chunkHeader(uint32_t id) {
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

    /**
     * @brief Write frames to dataChunk
     *
     * @param[in] inBuffer Buffer to read samples from
     * @param[in]  frames   Number of frames to write
     *
     * @returns number of frames written
     */
    template <typename T, typename std::enable_if<
                              std::is_floating_point<T>::value, int>::type = 0>
    uint64_t write(T* inBuffer, uint64_t frames) {
      uint64_t bytesWritten = frames * formatChunk()->blockAlignment();
      rawDataBuffer_.resize(bytesWritten);
      if (formatChunk()->isFloat()) {
        utils::encodeFloatSamples(inBuffer, &rawDataBuffer_[0],
                                  frames * formatChunk()->channelCount(),
                                  formatChunk()->bitsPerSample());
      } else {
        utils::encodePcmSamples(inBuffer, &rawDataBuffer_[0],
                                frames * formatChunk()->channelCount(),
                                formatChunk()->bitsPerSample());
      }
      fileStream_.write(&rawDataBuffer_[0], bytesWritten);
      dataChunk()->setSize(dataChunk()->size() + bytesWritten);
      chunkHeader(utils::fourCC("data")).size = dataChunk()->size();
      return frames;
    }


    /**
     * @brief Write frames to dataChunk
     *
     * @param[in]  inBuffer Buffer of interleaved samples to write
     * @param[in]  frames   Number of frames to write
     *
     * @returns number of frames written
     * @discussion inBuffer must match the wave formats internal
     *   type (16, 24, or 32 bit), (int or float).
     */
    template <typename T>
    uint64_t writeRaw(T* inBuffer, uint64_t frames) {
      if (formatChunk()->bitsPerSample() != sizeof(T) * 8) {
        throw std::runtime_error("format wrong size");
      }
      int frameSize = formatChunk()->blockAlignment();
      uint64_t bytesToWrite = frames * frameSize;
      uint64_t start = fileStream_.tellp();
      fileStream_.write((char *)inBuffer, bytesToWrite);
      uint64_t end = fileStream_.tellp();
      dataChunk()->setSize(dataChunk()->size() + (end - start));
      chunkHeader(utils::fourCC("data")).size = dataChunk()->size();
      return (end - start) / frameSize;
    }

    /**
     * @brief Add a marker to a BW64 file
     *
     * @param id Marker ID
     * @param position Sample position
     * @param label Optional label
     */
    inline void addMarker(uint32_t id, uint64_t position, const std::string& label = "") {
      // Get the cue chunk
      auto cueChunkPtr = cueChunk();
      if (!cueChunkPtr) {
        throw std::runtime_error("No cue chunk preallocated. Create writer with maxMarkers > 0.");
      }

      // Add cue point with label to the chunk
      cueChunkPtr->addCuePoint(id, position, label);
    }

    /**
     * @brief Add a marker to a BW64 file
     *
     * @param cuePoint CuePoint to add
     */
    inline void addMarker(const CuePoint &cuePoint) {
      auto cueChunkPtr = cueChunk();
      if (!cueChunkPtr) {
        throw std::runtime_error("No cue chunk preallocated. Create writer with maxMarkers > 0.");
      }
      cueChunkPtr->addCuePoint(cuePoint);
    }

    /**
     * @brief Add multiple markers to a BW64 file
     *
     * @param markers Vector of markers to add
     */
    inline void addMarkers(const std::vector<CuePoint>& markers) {
      auto cueChunkPtr = cueChunk();
      if (!cueChunkPtr) {
        throw std::runtime_error("No cue chunk preallocated. Create writer with maxMarkers > 0.");
      }
      for (const auto& marker : markers) {
        cueChunkPtr->addCuePoint(marker);
      }
    }

   private:
    std::ofstream fileStream_;
    std::vector<char> rawDataBuffer_;
    std::vector<std::shared_ptr<Chunk>> chunks_;
    std::vector<ChunkHeader> chunkHeaders_;
    std::vector<std::shared_ptr<Chunk>> postDataChunks_;
    bool useRf64Id_{false};
  };

}  // namespace bw64
