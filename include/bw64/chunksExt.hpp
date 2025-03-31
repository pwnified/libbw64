/**
 * @file chunksExt.hpp
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

struct CuePoint {
  uint32_t id;             // Unique identifier
  uint32_t position;       // Sample position
  uint32_t dataChunkId;    // Chunk ID (usually 'data')
  uint32_t chunkStart;     // Offset to start of chunk (usually 0)
  uint32_t blockStart;     // Offset to start of block (usually 0)
  uint32_t sampleOffset;   // Offset to sample of interest
};

class CueChunk : public bw64::Chunk {
public:
  CueChunk(const std::vector<CuePoint>& cuePoints) : cuePoints_(cuePoints) {}

  uint32_t id() const override { return bw64::utils::fourCC("cue "); }

  uint64_t size() const override {
    return 4 + (cuePoints_.size() * 24); // 4 bytes for count + 24 bytes per cue point
  }

  void write(std::ostream& stream) const override {
    uint32_t numCuePoints = static_cast<uint32_t>(cuePoints_.size());
    bw64::utils::writeValue(stream, numCuePoints);

    for (const auto& cue : cuePoints_) {
      bw64::utils::writeValue(stream, cue.id);
      bw64::utils::writeValue(stream, cue.position);
      bw64::utils::writeValue(stream, cue.dataChunkId);
      bw64::utils::writeValue(stream, cue.chunkStart);
      bw64::utils::writeValue(stream, cue.blockStart);
      bw64::utils::writeValue(stream, cue.sampleOffset);
    }
  }

  const std::vector<CuePoint>& cuePoints() const { return cuePoints_; }

private:
  std::vector<CuePoint> cuePoints_;
};


class LabelChunk : public bw64::Chunk {
public:
  LabelChunk(uint32_t cuePointId, const std::string& label)
  : cuePointId_(cuePointId), label_(label) {}

  uint32_t id() const override { return bw64::utils::fourCC("labl"); }

  uint64_t size() const override {
    return 4 + label_.size() + 1; // 4 bytes for cue point ID + label length + null terminator
  }

  void write(std::ostream& stream) const override {
    // Write cue point ID
    bw64::utils::writeValue(stream, cuePointId_);

    // Write label text (including null terminator)
    stream.write(label_.c_str(), label_.size() + 1);
  }

  uint32_t cuePointId() const { return cuePointId_; }
  const std::string& label() const { return label_; }

private:
  uint32_t cuePointId_;
  std::string label_;
};
