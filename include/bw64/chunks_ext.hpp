/**
 * @file chunks_ext.hpp
 * This file contains a Cue and Label chunk class for the BW64 file format.
 * The Cue chunk contains a list of cue points that can be used to mark specific
 * positions in the audio file. The Label chunk contains a list of labels that
 * can be associated with cue points.
 *
 */
#pragma once
#include "bw64.hpp"
#include <vector>
#include <string>
#include <cstring>

namespace bw64 {

/**
 * @brief Helper structure to represent a WAV cue point
 */
struct CuePoint {
  uint32_t id;             // Unique identifier
  uint32_t position;       // Sample position
  uint32_t dataChunkId;    // Chunk ID (usually 'data')
  uint32_t chunkStart;     // Offset to start of chunk (usually 0)
  uint32_t blockStart;     // Offset to start of block (usually 0)
  uint32_t sampleOffset;   // Offset to sample of interest
  std::string label;       // Associated label (optional)

  CuePoint() : id(0), position(0), dataChunkId(0), chunkStart(0), blockStart(0), sampleOffset(0), label("") {}
  CuePoint(uint32_t id, uint32_t position, const std::string& label = "")
    : id(id), position(position), dataChunkId(0), chunkStart(0), blockStart(0), sampleOffset(position), label(label) {}

  inline bool operator==(const CuePoint& other) const {
    return id == other.id && position == other.position &&
           dataChunkId == other.dataChunkId && chunkStart == other.chunkStart &&
           blockStart == other.blockStart && sampleOffset == other.sampleOffset &&
           label == other.label;
  }
  inline bool operator!=(const CuePoint& other) const {
    return !(*this == other);
  }
};

/**
 * @brief Class for cue chunk
 */
class CueChunk : public Chunk {
public:
  CueChunk(const std::vector<CuePoint>& cuePoints = std::vector<CuePoint>())
    : cuePoints_(cuePoints) {}

  uint32_t id() const override { return utils::fourCC("cue "); }

  uint64_t size() const override {
    return 4 + (cuePoints_.size() * 24); // 4 bytes for count + 24 bytes per cue point
  }

  void write(std::ostream& stream) const override {
    uint32_t numCuePoints = static_cast<uint32_t>(cuePoints_.size());
    utils::writeValue(stream, numCuePoints);

    for (const auto& cue : cuePoints_) {
      utils::writeValue(stream, cue.id);
      utils::writeValue(stream, cue.position);
      utils::writeValue(stream, cue.dataChunkId);
      utils::writeValue(stream, cue.chunkStart);
      utils::writeValue(stream, cue.blockStart);
      utils::writeValue(stream, cue.sampleOffset);
    }
  }

  const std::vector<CuePoint>& cuePoints() const { return cuePoints_; }

  // Get all labels as a map (for creating LIST chunk)
  std::map<uint32_t, std::string> getLabels() const {
    std::map<uint32_t, std::string> result;
    for (const auto& cue : cuePoints_) {
      if (!cue.label.empty()) {
        result[cue.id] = cue.label;
      }
    }
    return result;
  }

  // Add a cue point with optional label
  void addCuePoint(uint32_t id, uint64_t position, const std::string& label = "") {
    auto it = std::find_if(cuePoints_.begin(), cuePoints_.end(),
                           [id](const CuePoint& cp) { return cp.id == id; });
    if (it != cuePoints_.end()) {
      throw std::runtime_error("Cue point ID already exists");
    }

    CuePoint cue;
    cue.id = id;
    cue.position = static_cast<uint32_t>(position);
    cue.dataChunkId = utils::fourCC("data");
    cue.chunkStart = 0;
    cue.blockStart = 0;
    cue.sampleOffset = static_cast<uint32_t>(position);
    cue.label = label;

    cuePoints_.push_back(cue);

    std::sort(cuePoints_.begin(), cuePoints_.end(),
              [](const CuePoint& a, const CuePoint& b) {
      return a.position < b.position;
    });
  }

  // Add an existing cue point
  void addCuePoint(const CuePoint& cue) {
    auto it = std::find_if(cuePoints_.begin(), cuePoints_.end(),
                           [cue](const CuePoint& cp) { return cp.id == cue.id; });
    if (it != cuePoints_.end()) {
      throw std::runtime_error("Cue point ID already exists");
    }

    
    cuePoints_.push_back(cue);

    std::sort(cuePoints_.begin(), cuePoints_.end(),
              [](const CuePoint& a, const CuePoint& b) {
      return a.position < b.position;
    });
  }

  // Set a label for an existing cue point
  bool setLabel(uint32_t id, const std::string& label) {
    auto it = std::find_if(cuePoints_.begin(), cuePoints_.end(),
                           [id](const CuePoint& cp) { return cp.id == id; });
    if (it != cuePoints_.end()) {
      it->label = label;
      return true;
    }
    return false;
  }

  // Remove a cue point
  void removeCuePoint(uint32_t id) {
    cuePoints_.erase(std::remove_if(cuePoints_.begin(), cuePoints_.end(),
                                    [id](const CuePoint& cue) { return cue.id == id; }),
                     cuePoints_.end());
  }

  // Clear
  void clearCuePoints() {
    cuePoints_.clear();
  }

  // Access the internal vector directly
  std::vector<CuePoint>& cuePointsRef() {
    return cuePoints_;
  }

private:
  std::vector<CuePoint> cuePoints_;
};


/**
 * @brief Class for label chunk
 */
class LabelChunk : public Chunk {
public:
  LabelChunk(uint32_t cuePointId, const std::string& label)
  : cuePointId_(cuePointId), label_(label) {}

  uint32_t id() const override { return utils::fourCC("labl"); }

  uint64_t size() const override {
    return 4 + label_.size() + 1; // 4 bytes for cue point ID + label length + null terminator
  }

  void write(std::ostream& stream) const override {
    // Write cue point ID
    utils::writeValue(stream, cuePointId_);

    // Write label text (including null terminator)
    stream.write(label_.c_str(), label_.size() + 1);
  }

  uint32_t cuePointId() const { return cuePointId_; }
  const std::string& label() const { return label_; }

private:
  uint32_t cuePointId_;
  std::string label_;
};


/**
 * @brief Class for LIST chunk
 */
class ListChunk : public Chunk {
public:
  ListChunk(uint32_t listType, const std::vector<std::shared_ptr<Chunk>>& subChunks)
  : listType_(listType), subChunks_(subChunks) {}

  uint32_t id() const override { return utils::fourCC("LIST"); }

  uint64_t size() const override {
    // size is list type (4 bytes) plus the size of sub-chunks
    uint64_t size = 4; // List type
    for (const auto& chunk : subChunks_) {
      size += 8; // Chunk ID (4) + Chunk Size (4)
      size += chunk->size();

      // Padding
      if (chunk->size() % 2 == 1) {
        size += 1;
      }
    }
    return size;
  }

  void write(std::ostream& stream) const override {
    utils::writeValue(stream, listType_);

    for (const auto& chunk : subChunks_) {
      // chunk ID
      utils::writeValue(stream, chunk->id());

      // chunk size
      uint32_t chunkSize = static_cast<uint32_t>(chunk->size());
      utils::writeValue(stream, chunkSize);

      // chunk data
      chunk->write(stream);

      // Padding
      if (chunk->size() % 2 == 1) {
        utils::writeValue(stream, '\0');
      }
    }
  }

  uint32_t listType() const { return listType_; }
  const std::vector<std::shared_ptr<Chunk>>& subChunks() const { return subChunks_; }

  // Add a sub-chunk
  void addSubChunk(std::shared_ptr<Chunk> chunk) {
    subChunks_.push_back(chunk);
  }

  // Remove sub-chunks of a specific type
  void clearSubChunksOfType(uint32_t chunkId) {
    subChunks_.erase(std::remove_if(subChunks_.begin(), subChunks_.end(),
                                    [chunkId](const std::shared_ptr<Chunk>& chunk) {
      return chunk->id() == chunkId;
    }), subChunks_.end());
  }

  // Clear all sub-chunks
  void clearSubChunks() {
    subChunks_.clear();
  }
  
private:
  uint32_t listType_;
  std::vector<std::shared_ptr<Chunk>> subChunks_;
};

} // namespace bw64
