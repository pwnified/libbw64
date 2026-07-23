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

  /// @brief Encoding of samples stored in the data chunk.
  enum class SampleEncoding {
    Pcm,
    IeeeFloat
  };

  /// @brief RIFF identifier to use if the file grows beyond 4 GB.
  enum class LargeFileContainer {
    Bw64,
    Rf64
  };

  /**
   * @brief Explicit description of the WAVE format written by Bw64Writer.
   *
   * `channelMask` is written verbatim for WAVE_FORMAT_EXTENSIBLE. A zero mask
   * denotes direct-out/discrete channels, and a mask whose population differs
   * from the channel count is legal. libbw64 does not infer speaker positions.
   */
  struct FormatDescriptor {
    FormatDescriptor(
        SampleEncoding sampleEncoding,
        uint16_t containerBits,
        uint16_t validBits,
        bool extensible,
        uint32_t channelMask,
        LargeFileContainer largeFileContainer)
        : sampleEncoding(sampleEncoding),
          containerBits(containerBits),
          validBits(validBits),
          extensible(extensible),
          channelMask(channelMask),
          largeFileContainer(largeFileContainer) {}

    SampleEncoding sampleEncoding;
    uint16_t containerBits;
    uint16_t validBits;
    bool extensible;
    uint32_t channelMask;
    LargeFileContainer largeFileContainer;
  };

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
     * file. Cue chunks are the exception: they are retained in memory and
     * appended safely after audio during finalization.
     *
     * @note For convenience, you might consider using the `writeFile` helper
     * function.
     */
    Bw64Writer(const char* filename,
               uint16_t channels,
               uint32_t sampleRate,
               const FormatDescriptor& format,
               std::vector<std::shared_ptr<Chunk>> preDataChunks = {},
               uint32_t maxMarkers = 0)
        : formatDescriptor_(format),
          useRf64Id_(format.largeFileContainer == LargeFileContainer::Rf64),
          cueChunk_(std::make_shared<CueChunk>()) {
      // Construct and validate the format before opening (and potentially
      // truncating) the destination file.
      auto formatChunk = makeFormatChunk(channels, sampleRate, format);

      fileStream_.open(filename, std::fstream::out | std::fstream::binary);
      if (!fileStream_.is_open()) {
        std::stringstream errorString;
        errorString << "Could not open file: " << filename;
        throw std::runtime_error(errorString.str());
      }
      writeRiffHeader();

      // 28 byte ds64 header + 12 byte entry for axml
      writeChunkPlaceholder(utils::fourCC("JUNK"), 40u);

      writeChunk(formatChunk);

      if (formatChunk->isFloat()) {
        factChunk_ = std::make_shared<FactChunk>();
        writeChunk(factChunk_);
      }

      for (auto chunk : preDataChunks) {
        if (!chunk) {
          continue;
        }
        if (chunk->id() == utils::fourCC("cue ")) {
          auto suppliedCue = std::dynamic_pointer_cast<CueChunk>(chunk);
          if (!suppliedCue) {
            throw std::runtime_error("cue chunk has unexpected type");
          }
          for (const auto& cue : suppliedCue->cuePoints()) {
            cueChunk_->addCuePoint(cue);
          }
        } else if (chunk->id() == utils::fourCC("fact") && factChunk_) {
          throw std::runtime_error(
              "fact chunk is managed automatically for IEEE float");
        } else {
          writeChunk(chunk);
        }
      }

      // Retained in the legacy API for source compatibility. Marker chunks
      // now grow in memory and are appended during close().
      (void)maxMarkers;

      // Write the data chunk header
      auto dataChunk = std::make_shared<DataChunk>();
      writeChunk(dataChunk);
    }

    /**
     * @brief Backwards-compatible writer constructor.
     *
     * New code should use FormatDescriptor so that the sample encoding,
     * container bits, valid bits and large-file RIFF identifier are explicit.
     * The supplied channel mask is preserved verbatim.
     */
    Bw64Writer(const char* filename,
               uint16_t channels,
               uint32_t sampleRate,
               uint16_t bitDepth,
               std::vector<std::shared_ptr<Chunk>> preDataChunks,
               bool useExtensible = false,
               bool useFloat = false,
               uint32_t channelMask = 0,
               uint32_t maxMarkers = 0)
        : Bw64Writer(filename, channels, sampleRate,
                     legacyFormatDescriptor(bitDepth, useExtensible, useFloat,
                                            channelMask),
                     preDataChunks, maxMarkers) {}

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
        finalizeFactChunk();
        finalizeCueChunk();
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
    /// @brief Get the explicit format description used to create this writer
    const FormatDescriptor& formatDescriptor() const {
      return formatDescriptor_;
    }
    /// @brief Get number of frames
    uint64_t framesWritten() const {
      return dataChunk()->size() / formatChunk()->blockAlignment();
    }

    template <typename ChunkType>
    std::vector<std::shared_ptr<ChunkType>> chunksWithId(
        const std::vector<std::shared_ptr<Chunk>>& chunks,
        uint32_t chunkId) const {
      std::vector<std::shared_ptr<ChunkType>> foundChunks;
      for (const auto& candidate : chunks) {
        if (candidate && candidate->id() == chunkId) {
          auto typed = std::dynamic_pointer_cast<ChunkType>(candidate);
          if (typed) {
            foundChunks.push_back(typed);
          }
        }
      }
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
    std::shared_ptr<FactChunk> factChunk() const { return factChunk_; }
    std::shared_ptr<ChnaChunk> chnaChunk() const {
      return chunk<ChnaChunk>(chunks_, utils::fourCC("chna"));
    }
    std::shared_ptr<AxmlChunk> axmlChunk() const {
      return chunk<AxmlChunk>(chunks_, utils::fourCC("axml"));
    }
    std::shared_ptr<CueChunk> cueChunk() const {
      return cueChunk_;
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
    void useRf64Id(bool state) {
      if (!state &&
          (formatDescriptor_.sampleEncoding != SampleEncoding::Pcm ||
           formatDescriptor_.extensible)) {
        throw std::runtime_error(
            "float and extensible formats must promote to RF64");
      }
      useRf64Id_ = state;
      formatDescriptor_.largeFileContainer =
          state ? LargeFileContainer::Rf64 : LargeFileContainer::Bw64;
    }

    void setChnaChunk(std::shared_ptr<ChnaChunk> chunk) {
      if (!chunk) {
        throw std::runtime_error("chna chunk must not be null");
      }
      if (chnaChunk()) {
        throw std::runtime_error("chna chunk already supplied");
      }
      auto data = dataChunk();
      if (!data || data->size() != 0u) {
        throw std::runtime_error(
            "chna chunk must be supplied before writing audio");
      }

      const uint64_t dataHeaderPosition =
          chunkHeader(utils::fourCC("data")).position;
      chunkHeaders_.erase(
          std::remove_if(chunkHeaders_.begin(), chunkHeaders_.end(),
                         [](const ChunkHeader& header) {
                           return header.id == utils::fourCC("data");
                         }),
          chunkHeaders_.end());
      chunks_.erase(
          std::remove_if(chunks_.begin(), chunks_.end(),
                         [&data](const std::shared_ptr<Chunk>& candidate) {
                           return candidate == data;
                         }),
          chunks_.end());

      fileStream_.seekp(utils::safeCast<std::streamoff>(dataHeaderPosition));
      if (!fileStream_.good()) {
        throw std::runtime_error("file error while inserting chna chunk");
      }
      writeChunk(chunk);
      writeChunk(data);
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


    /// @brief Append cue and associated label chunks after audio.
    void finalizeCueChunk() {
      auto cueChunkPtr = cueChunk();
      if (cueChunkPtr && !cueChunkPtr->cuePoints().empty()) {
        writeChunk(cueChunkPtr);
        auto labels = cueChunkPtr->getLabels();

        // If we have labels, create a LIST chunk
        if (!labels.empty()) {
          std::vector<std::shared_ptr<Chunk>> labelChunks;
          for (const auto& label : labels) {
            auto labelChunk = std::make_shared<LabelChunk>(label.first, label.second);
            labelChunks.push_back(labelChunk);
          }

          auto listChunk = std::make_shared<ListChunk>(utils::fourCC("adtl"), labelChunks);
          writeChunk(listChunk);
        }
      }
    }

    void finalizeFactChunk() {
      if (!factChunk_) {
        return;
      }

      const uint64_t frames = framesWritten();
      factChunk_->sampleLength(
          frames > (std::numeric_limits<uint32_t>::max)()
              ? (std::numeric_limits<uint32_t>::max)()
              : static_cast<uint32_t>(frames));
      overwriteChunk(utils::fourCC("fact"), factChunk_);
    }


    void overwriteJunkWithDs64Chunk() {
      auto ds64Chunk = std::make_shared<DataSize64Chunk>();
      ds64Chunk->bw64Size(riffChunkSize());
      // write data size even if it's not too big
      ds64Chunk->dataSize(dataChunk()->size());
      // RF64 uses this field as the 64-bit replacement for fact's sample
      // count. BW64 defines it as a zero dummy value.
      ds64Chunk->sampleCount(useRf64Id_ ? framesWritten() : 0u);

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
      if (frames == 0u) {
        return 0u;
      }
      if (!inBuffer) {
        throw std::runtime_error("input buffer must not be null");
      }

      const uint64_t bytesWritten = dataBytesForFrames(frames);
      rawDataBuffer_.resize(utils::safeCast<size_t>(bytesWritten));
      if (formatChunk()->isFloat()) {
        utils::encodeFloatSamples(inBuffer, &rawDataBuffer_[0],
                                  frames * formatChunk()->channelCount(),
                                  formatChunk()->bitsPerSample());
      } else {
        utils::encodePcmSamples(inBuffer, &rawDataBuffer_[0],
                                frames * formatChunk()->channelCount(),
                                formatChunk()->bitsPerSample());
      }
      fileStream_.write(rawDataBuffer_.data(), streamSize(bytesWritten));
      if (!fileStream_.good()) {
        throw std::runtime_error("file error while writing frames");
      }
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
     * `ConstByteSpan` is the preferred overload because it validates the
     * caller's byte capacity, including for packed 24-bit samples.
     */
    uint64_t writeRaw(ConstByteSpan inBuffer, uint64_t frames) {
      const uint64_t bytesToWrite = dataBytesForFrames(frames);
      if (bytesToWrite > inBuffer.size) {
        throw std::runtime_error("raw input buffer is too small");
      }
      return writeRaw(inBuffer.data, frames);
    }

    /// @brief Backwards-compatible untyped raw-frame writer.
    uint64_t writeRaw(const void* inBuffer, uint64_t frames) {
      if (frames == 0u) {
        return 0u;
      }
      if (!inBuffer) {
        throw std::runtime_error("raw input buffer must not be null");
      }

      const uint64_t bytesToWrite = dataBytesForFrames(frames);
      fileStream_.write(static_cast<const char*>(inBuffer),
                        streamSize(bytesToWrite));
      if (!fileStream_.good()) {
        throw std::runtime_error("file error while writing raw frames");
      }
      dataChunk()->setSize(dataChunk()->size() + bytesToWrite);
      chunkHeader(utils::fourCC("data")).size = dataChunk()->size();
      return frames;
    }

    /**
     * @brief Add a marker to a BW64 file
     *
     * @param id Marker ID
     * @param position Sample position
     * @param label Optional label
     */
    inline void addMarker(uint32_t id, uint64_t position, const std::string& label = "") {
      cueChunk_->addCuePoint(id, position, label);
    }

    /**
     * @brief Add a marker to a BW64 file
     *
     * @param cuePoint CuePoint to add
     */
    inline void addMarker(const CuePoint &cuePoint) {
      cueChunk_->addCuePoint(cuePoint);
    }

    /**
     * @brief Add multiple markers to a BW64 file
     *
     * @param markers Vector of markers to add
     */
    inline void addMarkers(const std::vector<CuePoint>& markers) {
      for (const auto& marker : markers) {
        cueChunk_->addCuePoint(marker);
      }
    }

   private:
    static FormatDescriptor legacyFormatDescriptor(uint16_t bitDepth,
                                                    bool useExtensible,
                                                    bool useFloat,
                                                    uint32_t channelMask) {
      return FormatDescriptor(
          useFloat ? SampleEncoding::IeeeFloat : SampleEncoding::Pcm,
          bitDepth, bitDepth, useExtensible, channelMask,
          (useExtensible || useFloat) ? LargeFileContainer::Rf64
                                     : LargeFileContainer::Bw64);
    }

    static void validateFormatDescriptor(const FormatDescriptor& format) {
      switch (format.sampleEncoding) {
        case SampleEncoding::Pcm:
        case SampleEncoding::IeeeFloat:
          break;
        default:
          throw std::runtime_error("unsupported sample encoding");
      }
      switch (format.largeFileContainer) {
        case LargeFileContainer::Bw64:
        case LargeFileContainer::Rf64:
          break;
        default:
          throw std::runtime_error("unsupported large-file container");
      }
      if (format.containerBits != 16u && format.containerBits != 24u &&
          format.containerBits != 32u) {
        std::stringstream errorString;
        errorString << "container bits not supported: "
                    << format.containerBits;
        throw std::runtime_error(errorString.str());
      }
      if (format.validBits == 0u ||
          format.validBits > format.containerBits) {
        throw std::runtime_error(
            "valid bits must be between 1 and container bits");
      }
      if (!format.extensible &&
          format.validBits != format.containerBits) {
        throw std::runtime_error(
            "valid bits require WAVE_FORMAT_EXTENSIBLE");
      }
      if (!format.extensible && format.channelMask != 0u) {
        throw std::runtime_error(
            "channel mask requires WAVE_FORMAT_EXTENSIBLE");
      }
      if (format.sampleEncoding == SampleEncoding::IeeeFloat) {
        if (format.containerBits != 32u) {
          throw std::runtime_error(
              "IEEE float writing supports only 32 container bits");
        }
        if (format.validBits != format.containerBits) {
          throw std::runtime_error(
              "IEEE float valid bits must equal container bits");
        }
      }
      if (format.largeFileContainer == LargeFileContainer::Bw64 &&
          (format.sampleEncoding != SampleEncoding::Pcm ||
           format.extensible)) {
        throw std::runtime_error(
            "BW64 output supports only non-extensible PCM");
      }
    }

    uint64_t dataBytesForFrames(uint64_t frames) const {
      const uint64_t frameSize = formatChunk()->blockAlignment();
      if (frames > (std::numeric_limits<uint64_t>::max)() / frameSize) {
        throw std::runtime_error("frame byte count overflow");
      }
      return frames * frameSize;
    }

    static std::streamsize streamSize(uint64_t bytes) {
      return utils::safeCast<std::streamsize>(bytes);
    }

    static std::shared_ptr<FormatInfoChunk> makeFormatChunk(
        uint16_t channels, uint32_t sampleRate,
        const FormatDescriptor& format) {
      validateFormatDescriptor(format);

      const bool useFloat =
          format.sampleEncoding == SampleEncoding::IeeeFloat;
      if (format.extensible) {
        return std::make_shared<FormatInfoChunk>(
            channels, sampleRate, format.containerBits,
            std::make_shared<ExtraData>(
                format.validBits, format.channelMask,
                useFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
                         : KSDATAFORMAT_SUBTYPE_PCM),
            WAVE_FORMAT_EXTENSIBLE);
      }

      return std::make_shared<FormatInfoChunk>(
          channels, sampleRate, format.containerBits, nullptr,
          useFloat ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM);
    }

    std::ofstream fileStream_;
    std::vector<char> rawDataBuffer_;
    std::vector<std::shared_ptr<Chunk>> chunks_;
    std::vector<ChunkHeader> chunkHeaders_;
    std::vector<std::shared_ptr<Chunk>> postDataChunks_;
    FormatDescriptor formatDescriptor_;
    bool useRf64Id_{false};
    std::shared_ptr<FactChunk> factChunk_;
    std::shared_ptr<CueChunk> cueChunk_;
  };

}  // namespace bw64
