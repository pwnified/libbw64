#include <catch2/catch.hpp>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include "bw64/bw64.hpp"

using namespace bw64;

TEST_CASE("read_file_not_found") {
  REQUIRE_THROWS_AS(readFile("file_not_found.wav"), std::runtime_error);
}

TEST_CASE("read_rect_16bit") {
  auto bw64File = readFile("rect_16bit.wav");
  REQUIRE(bw64File->formatTag() == 1u);
  REQUIRE(bw64File->bitDepth() == 16u);
  REQUIRE(bw64File->sampleRate() == 44100u);
  REQUIRE(bw64File->channels() == 2u);
  REQUIRE(bw64File->numberOfFrames() == 22050u);
  bw64File->close();
}

TEST_CASE("read_rect_24bit") {
  auto bw64File = readFile("rect_24bit.wav");
  REQUIRE(bw64File->formatTag() == 1u);
  REQUIRE(bw64File->bitDepth() == 24u);
  REQUIRE(bw64File->sampleRate() == 44100u);
  REQUIRE(bw64File->channels() == 2u);
  REQUIRE(bw64File->numberOfFrames() == 22050u);
  bw64File->close();
}

TEST_CASE("read_rect_32bit") {
  auto bw64File = readFile("rect_32bit.wav");
  REQUIRE(bw64File->formatTag() == 0xfffeu);
  REQUIRE(bw64File->bitDepth() == 32u);
  REQUIRE(bw64File->sampleRate() == 44100u);
  REQUIRE(bw64File->channels() == 2u);
  REQUIRE(bw64File->numberOfFrames() == 22050u);
  bw64File->close();
}

TEST_CASE("read_rect_24bit_rf64") {
  auto bw64File = readFile("rect_24bit_rf64.wav");
  REQUIRE(bw64File->formatTag() == 1u);
  REQUIRE(bw64File->bitDepth() == 24u);
  REQUIRE(bw64File->sampleRate() == 44100u);
  REQUIRE(bw64File->channels() == 2u);
  REQUIRE(bw64File->numberOfFrames() == 22050u);
  bw64File->close();
}

TEST_CASE("read_rect_24bit_noriff") {
  REQUIRE_THROWS_AS(Bw64Reader("rect_24bit_noriff.wav"), std::runtime_error);
}

TEST_CASE("read_rect_24bit_nowave") {
  REQUIRE_THROWS_AS(Bw64Reader("rect_24bit_nowave.wav"), std::runtime_error);
}

TEST_CASE("read_rect_24bit_wrong_fmt_size") {
  REQUIRE_THROWS_AS(Bw64Reader("rect_24bit_wrong_fmt_size.wav"),
                    std::runtime_error);
}

TEST_CASE("read_noise_24bit_uneven_data_chunk_size") {
  auto bw64File = readFile("noise_24bit_uneven_data_chunk_size.wav");
  REQUIRE(bw64File->bitDepth() == 24);
  REQUIRE(bw64File->sampleRate() == 44100);
  REQUIRE(bw64File->channels() == 1);
  REQUIRE(bw64File->numberOfFrames() == 13);
  REQUIRE(bw64File->hasChunk(utils::fourCC("chna")) == true);
  REQUIRE(bw64File->chnaChunk() != nullptr);
  REQUIRE(bw64File->hasChunk(utils::fourCC("axml")) == false);
  REQUIRE(bw64File->axmlChunk() == nullptr);
  bw64File->close();
}

TEST_CASE("read_check_chunks") {
  auto bw64File = readFile("rect_16bit.wav");
  REQUIRE(bw64File->chunks().size() == 2);
  REQUIRE(utils::fourCCToStr(bw64File->chunks().at(0).id) == "fmt ");
  REQUIRE(utils::fourCCToStr(bw64File->chunks().at(1).id) == "data");
  REQUIRE(bw64File->hasChunk(utils::fourCC("fmt ")) == true);
  REQUIRE(bw64File->hasChunk(utils::fourCC("chna")) == false);
  REQUIRE(bw64File->hasChunk(utils::fourCC("axml")) == false);
  bw64File->close();
}

TEST_CASE("read_seek_tell") {
  auto bw64File = readFile("rect_16bit.wav");
  // should be positioned at the beginning after opening
  REQUIRE(bw64File->tell() == 0);

  // std::ios::beg
  bw64File->seek(INT32_MIN);
  REQUIRE(bw64File->tell() == 0);
  bw64File->seek(11025);
  REQUIRE(bw64File->tell() == 11025);
  bw64File->seek(22060);
  REQUIRE(bw64File->tell() == 22050);

  // std::ios::cur
  bw64File->seek(INT32_MIN, std::ios::cur);
  REQUIRE(bw64File->tell() == 0);
  bw64File->seek(11025, std::ios::cur);
  REQUIRE(bw64File->tell() == 11025);
  bw64File->seek(INT32_MAX, std::ios::cur);
  REQUIRE(bw64File->tell() == 22050);

  // std::ios::end
  bw64File->seek(INT32_MIN, std::ios::end);
  REQUIRE(bw64File->tell() == 0);
  bw64File->seek(-11025, std::ios::end);
  REQUIRE(bw64File->tell() == 11025);
  bw64File->seek(INT32_MAX, std::ios::end);
  REQUIRE(bw64File->tell() == 22050);

  bw64File->close();
}

TEST_CASE("write_16bit") {
  auto bw64File = writeFile("zeros_16bit.wav", 2u, 48000u, 16u);

  REQUIRE(bw64File->channels() == 2);
  REQUIRE(bw64File->sampleRate() == 48000);
  REQUIRE(bw64File->bitDepth() == 16);

  int frames = 4800;
  std::vector<float> data(frames * bw64File->channels(), 0.0);
  bw64File->write(&data[0], frames);
  REQUIRE(bw64File->framesWritten() == frames);
  bw64File->close();
}

TEST_CASE("write_24bit") {
  auto bw64File = writeFile("zeros_24bit.wav", 2u, 48000u, 24u);

  REQUIRE(bw64File->channels() == 2);
  REQUIRE(bw64File->sampleRate() == 48000);
  REQUIRE(bw64File->bitDepth() == 24);

  int frames = 4800;
  std::vector<float> data(frames * bw64File->channels(), 0.0);
  bw64File->write(&data[0], frames);
  REQUIRE(bw64File->framesWritten() == frames);
  bw64File->close();
}

TEST_CASE("write_32bit") {
  auto bw64File = writeFile("zeros_32bit.wav", 2u, 48000u, 32u);

  REQUIRE(bw64File->channels() == 2);
  REQUIRE(bw64File->sampleRate() == 48000);
  REQUIRE(bw64File->bitDepth() == 32);

  int frames = 4800;
  std::vector<float> data(frames * bw64File->channels(), 0.0);
  bw64File->write(&data[0], frames);
  REQUIRE(bw64File->framesWritten() == frames);
  bw64File->close();
}

void writeClipped(const std::string& filename, uint16_t bitDepth,
                  uint64_t frames, uint16_t channels = 1u,
                  uint32_t sampleRate = 48000u) {
  auto bw64File = writeFile(filename, channels, sampleRate, bitDepth);
  std::vector<float> data(frames * channels, 2.f);
  bw64File->write(&data[0], frames);
  bw64File->close();
}

TEST_CASE("write_read_16bit_clipped") {
  int frames = 4800;
  writeClipped("write_read_16bit_clipped.wav", 16, frames);
  auto bw64File = readFile("write_read_16bit_clipped.wav");
  std::vector<float> data(frames * bw64File->channels(), 0.f);
  auto readFrames = bw64File->read(&data[0], frames);
  REQUIRE(readFrames == frames);
  REQUIRE(data.at(0) == Approx(1.f).epsilon(1e-2));
  REQUIRE(data.at(100) == Approx(1.f).epsilon(1e-2));
  REQUIRE(data.at(200) == Approx(1.f).epsilon(1e-2));
  REQUIRE(data.at(400) == Approx(1.f).epsilon(1e-2));
  REQUIRE(data.at(800) == Approx(1.f).epsilon(1e-2));
  REQUIRE(data.at(1600) == Approx(1.f).epsilon(1e-2));
  REQUIRE(data.at(3200) == Approx(1.f).epsilon(1e-2));
}

TEST_CASE("write_read_24bit_clipped") {
  int frames = 4800;
  writeClipped("write_read_24bit_clipped.wav", 24, frames);
  auto bw64File = readFile("write_read_24bit_clipped.wav");
  std::vector<float> data(frames * bw64File->channels(), 0.f);
  auto readFrames = bw64File->read(&data[0], frames);
  REQUIRE(readFrames == frames);
  REQUIRE(data.at(0) == Approx(1.f).epsilon(1e-3));
  REQUIRE(data.at(100) == Approx(1.f).epsilon(1e-3));
  REQUIRE(data.at(200) == Approx(1.f).epsilon(1e-3));
  REQUIRE(data.at(400) == Approx(1.f).epsilon(1e-3));
  REQUIRE(data.at(800) == Approx(1.f).epsilon(1e-3));
  REQUIRE(data.at(1600) == Approx(1.f).epsilon(1e-3));
  REQUIRE(data.at(3200) == Approx(1.f).epsilon(1e-3));
}

TEST_CASE("write_read_32bit_clipped") {
  int frames = 4800;
  writeClipped("write_read_32bit_clipped.wav", 32, frames);
  auto bw64File = readFile("write_read_32bit_clipped.wav");
  std::vector<float> data(frames * bw64File->channels(), 0.f);
  auto readFrames = bw64File->read(&data[0], frames);
  REQUIRE(readFrames == frames);
  REQUIRE(data.at(0) == Approx(1.f).epsilon(1e-6));
  REQUIRE(data.at(100) == Approx(1.f).epsilon(1e-6));
  REQUIRE(data.at(200) == Approx(1.f).epsilon(1e-6));
  REQUIRE(data.at(400) == Approx(1.f).epsilon(1e-6));
  REQUIRE(data.at(800) == Approx(1.f).epsilon(1e-6));
  REQUIRE(data.at(1600) == Approx(1.f).epsilon(1e-6));
  REQUIRE(data.at(3200) == Approx(1.f).epsilon(1e-6));
}

void writeRandom(const std::string& filename, uint16_t bitDepth,
                 uint64_t frames, uint16_t channels = 1u,
                 uint32_t sampleRate = 48000u) {
  // Generate vector with random values between -1.f and 1.f
  std::random_device rd;
  std::mt19937 engine(rd());
  std::uniform_real_distribution<float> dist(-1.f, 1.f);
  auto gen = [&]() { return dist(engine); };
  std::vector<float> data(frames * channels);
  generate(begin(data), end(data), gen);

  auto bw64File = writeFile(filename, channels, sampleRate, bitDepth);
  bw64File->write(&data[0], frames);
  bw64File->close();
}

TEST_CASE("write_read_riff_header") {
  int frames = 4800;
  writeRandom("write_read_riff_header.wav", 32, frames);
  auto bw64File = readFile("write_read_riff_header.wav");
  REQUIRE(bw64File->fileFormat() == utils::fourCC("RIFF"));
  // RIFF size excludes the 8-byte RIFF ID/size prefix. No CHNA reservation is
  // present unless the caller explicitly supplies CHNA metadata.
  REQUIRE(bw64File->fileSize() == 19284);
}

TEST_CASE("write_read_96000") {
  int frames = 9600;
  writeRandom("write_read_96000.wav", 32, frames, 1u, 96000u);
  auto bw64File = readFile("write_read_96000.wav");
  REQUIRE(bw64File->sampleRate() == 96000u);
}

TEST_CASE("can_read_all_frames") {
  int frames = 13;
  auto bw64File = readFile("noise_24bit_uneven_data_chunk_size.wav");
  REQUIRE(bw64File->numberOfFrames() == 13);
  std::vector<float> data(frames * bw64File->channels(), 0.f);
  auto readSampleCount = bw64File->read(&data[0], frames);
  REQUIRE(readSampleCount == 13);
}

TEST_CASE("write_read_odd_chunks_after") {
  int frames = 13;

  {
    std::vector<float> data(frames, 0.5);
    auto writer = writeFile("write_read_odd_chunks_after.wav", 1, 48000, 24);
    writer->setAxmlChunk(std::make_shared<AxmlChunk>("axml"));
    writer->write(&data[0], frames);
    writer->close();
  }

  {
    auto reader = readFile("write_read_odd_chunks_after.wav");
    REQUIRE(reader->numberOfFrames() == frames);
    auto axml = reader->axmlChunk();
    REQUIRE(axml);
    REQUIRE(axml->data() == "axml");
  }
}

TEST_CASE("write_read_big", "[.big]") {
  uint64_t frames = 0x90000000UL;
  uint64_t blockSize = 0x1000UL;

  std::string filename = "big.wav";
  {
    auto bw64File = writeFile(filename, 1, 48000, 16);

    std::vector<float> block(blockSize);
    for (size_t i = 0; i < block.size(); i++)
      block[i] = (i % 2 == 0) ? 0.5f : 0.0f;

    for (uint64_t frame = 0; frame < frames; frame += block.size())
      bw64File->write(&block[0], block.size());

    bw64File->close();
  }

  {
    auto bw64File = readFile(filename);
    REQUIRE(bw64File->numberOfFrames() == frames);

    std::vector<float> block(blockSize);
    for (uint64_t frame = 0; frame < frames; frame += block.size()) {
      auto readFrames = bw64File->read(&block[0], blockSize);
      REQUIRE(readFrames == blockSize);
      for (size_t i = 0; i < blockSize; i++) {
        if (i % 2 == 0)
          REQUIRE(block[i] == Approx(0.5f).epsilon(1e-2));
        else
          REQUIRE(block[i] == Approx(0.0f).epsilon(1e-2));
      }
    }
  }

  remove(filename.c_str());
}

TEST_CASE("write_read_big_axml", "[.big]") {
  std::string filename = "big_axml.wav";
  size_t blockSize = 1000;
  uint64_t axml_size = 0x100000000ull;

  const char* pattern = "AXML";

  {
    std::string axml_data(axml_size, 0);
    for (size_t i = 0; i < axml_data.size(); i++) axml_data[i] = pattern[i % 4];

    auto axml = std::make_shared<AxmlChunk>(std::move(axml_data));

    auto bw64File = writeFile(filename, 1, 48000, 16, nullptr, axml);

    std::vector<float> block(blockSize);
    for (size_t i = 0; i < block.size(); i++)
      block[i] = (i % 2 == 0) ? 0.5f : 0.0f;
    bw64File->write(&block[0], block.size());

    bw64File->close();
  }

  {
    auto bw64File = readFile(filename);

    // check samples
    REQUIRE(bw64File->numberOfFrames() == blockSize);
    std::vector<float> block(blockSize);
    auto readFrames = bw64File->read(&block[0], blockSize);
    REQUIRE(readFrames == blockSize);
    for (size_t i = 0; i < blockSize; i++) {
      if (i % 2 == 0)
        REQUIRE(block[i] == Approx(0.5f).epsilon(1e-2));
      else
        REQUIRE(block[i] == Approx(0.0f).epsilon(1e-2));
    }

    // check axml
    auto axml = bw64File->axmlChunk();
    REQUIRE(axml);

    auto& axml_data = axml->data();
    REQUIRE(axml_data.size() == axml_size);

    for (size_t i = 0; i < axml_data.size(); i++) {
      REQUIRE(axml_data[i] == pattern[i % 4]);
    }
  }

  remove(filename.c_str());
}

/// dummy chunk that just writes spaces
class MegaChunk : public Chunk {
 public:
  MegaChunk(uint32_t id, uint64_t size) : id_(id), size_(size) {}

  uint32_t id() const override { return id_; }
  uint64_t size() const override { return size_; }

  void write(std::ostream& stream) const override {
    for (size_t i = 0; i < size_; i++) stream << ' ';
  }

 private:
  uint32_t id_;
  uint64_t size_;
};

/// A logically large chunk backed by a sparse file hole for fast tests.
class SparseChunk : public Chunk {
 public:
  SparseChunk(uint32_t id, uint64_t size) : id_(id), size_(size) {}

  uint32_t id() const override { return id_; }
  uint64_t size() const override { return size_; }

  void write(std::ostream& stream) const override {
    if (size_ == 0u) return;
    stream.seekp(utils::safeCast<std::streamoff>(size_ - 1u), std::ios::cur);
    utils::writeValue(stream, '\0');
  }

 private:
  uint32_t id_;
  uint64_t size_;
};

TEST_CASE("large_extensible_float_promotes_to_rf64") {
  const std::string filename = "large_extensible_float_rf64.wav";
  FormatDescriptor format(SampleEncoding::IeeeFloat, 32u, 32u, true, 0u,
                          LargeFileContainer::Rf64);

  {
    Bw64Writer writer(filename.c_str(), 2u, 48000u, format);
    float frame[2] = {0.0f, 0.0f};
    writer.write(frame, 1u);
    writer.postDataChunk(std::make_shared<SparseChunk>(
        utils::fourCC("sprs"), 0x100000000ull));
    writer.close();
  }

  {
    std::ifstream stream(filename.c_str(), std::ios::binary);
    uint32_t riffId;
    uint32_t riffSize;
    uint32_t waveId;
    uint32_t ds64Id;
    uint32_t ds64Size;
    uint64_t size64;
    uint64_t dataSize64;
    uint64_t sampleCount64;
    utils::readValue(stream, riffId);
    utils::readValue(stream, riffSize);
    utils::readValue(stream, waveId);
    utils::readValue(stream, ds64Id);
    utils::readValue(stream, ds64Size);
    utils::readValue(stream, size64);
    utils::readValue(stream, dataSize64);
    utils::readValue(stream, sampleCount64);

    REQUIRE(riffId == utils::fourCC("RF64"));
    REQUIRE(riffSize == (std::numeric_limits<uint32_t>::max)());
    REQUIRE(waveId == utils::fourCC("WAVE"));
    REQUIRE(ds64Id == utils::fourCC("ds64"));
    REQUIRE(ds64Size == 40u);
    REQUIRE(size64 > (std::numeric_limits<uint32_t>::max)());
    REQUIRE(dataSize64 == 8u);
    REQUIRE(sampleCount64 == 1u);
  }

  remove(filename.c_str());
}

TEST_CASE("write_too_many_big_chunks", "[.big]") {
  std::string filename = "too_many_big_chunks.wav";

  auto bw64File = writeFile(filename, 1, 48000, 16, nullptr);

  // TODO: setAxmlChunk should really be renamed...
  bw64File->setAxmlChunk(
      std::make_shared<MegaChunk>(utils::fourCC("mega"), 0x100000000ull));
  bw64File->setAxmlChunk(
      std::make_shared<MegaChunk>(utils::fourCC("megb"), 0x100000000ull));

  REQUIRE_THROWS_WITH(
      bw64File->close(),
      "ds64 chunk is too large (52 bytes) to overwrite JUNK chunk (40 bytes)");

  remove(filename.c_str());
}

TEST_CASE("write_extensible_preserves_channel_mask") {
  std::string filename = "test_extensible_channel_mask.wav";

  // A mask with fewer positions than channels is legal and must not be
  // reinterpreted by the library.
  {
    Bw64Writer writer(filename.c_str(), 2, 48000, 24, {}, true, false, 1);
    std::vector<float> data(100 * 2, 0.0f);
    writer.write(&data[0], 100);
    writer.close();
  }

  {
    auto reader = readFile(filename);
    REQUIRE(reader->channels() == 2);
    REQUIRE(reader->formatTag() == WAVE_FORMAT_EXTENSIBLE);

    auto formatChunk = reader->formatChunk();
    REQUIRE(formatChunk->isExtensible());
    auto extraData = formatChunk->extraData();
    REQUIRE(extraData);
    REQUIRE(extraData->dwChannelMask() == 0x1u);

    reader->close();
  }

  remove(filename.c_str());
}

TEST_CASE("write_extensible_preserves_zero_channel_mask") {
  std::string filename = "test_extensible_many_channels.wav";

  // Zero means direct-out/discrete channels, including for channel counts that
  // cannot be represented as individual WAVE speaker bits.
  {
    Bw64Writer writer(filename.c_str(), 32, 48000, 24, {}, true, false, 0);
    std::vector<float> data(100 * 32, 0.0f);
    writer.write(&data[0], 100);
    writer.close();
  }

  {
    auto reader = readFile(filename);
    REQUIRE(reader->channels() == 32);
    REQUIRE(reader->formatTag() == WAVE_FORMAT_EXTENSIBLE);

    auto formatChunk = reader->formatChunk();
    REQUIRE(formatChunk->isExtensible());
    auto extraData = formatChunk->extraData();
    REQUIRE(extraData);
    REQUIRE(extraData->dwChannelMask() == 0u);

    reader->close();
  }

  remove(filename.c_str());
}

TEST_CASE("write_with_explicit_format_descriptor") {
  std::string filename = "test_explicit_format_descriptor.wav";
  FormatDescriptor format(SampleEncoding::Pcm, 24u, 20u, true, 0x5u,
                          LargeFileContainer::Rf64);

  {
    auto writer = writeFile(filename, 3, 48000, format);
    REQUIRE(writer->formatDescriptor().sampleEncoding == SampleEncoding::Pcm);
    REQUIRE(writer->formatDescriptor().containerBits == 24u);
    REQUIRE(writer->formatDescriptor().validBits == 20u);
    REQUIRE(writer->formatDescriptor().extensible);
    REQUIRE(writer->formatDescriptor().channelMask == 0x5u);
    REQUIRE(writer->formatDescriptor().largeFileContainer ==
            LargeFileContainer::Rf64);
    writer->close();
  }

  {
    auto reader = readFile(filename);
    auto extraData = reader->formatChunk()->extraData();
    REQUIRE(extraData);
    REQUIRE(extraData->validBitsPerSample() == 20u);
    REQUIRE(extraData->dwChannelMask() == 0x5u);
    reader->close();
  }

  remove(filename.c_str());
}

TEST_CASE("invalid_write_format_does_not_create_file") {
  std::string filename = "test_invalid_write_format.wav";
  remove(filename.c_str());

  SECTION("unsupported float width") {
    FormatDescriptor format(SampleEncoding::IeeeFloat, 24u, 24u, false, 0u,
                            LargeFileContainer::Rf64);
    REQUIRE_THROWS_WITH(Bw64Writer(filename.c_str(), 2, 48000, format),
                        "IEEE float writing supports only 32 container bits");
  }

  SECTION("non-extensible valid bits") {
    FormatDescriptor format(SampleEncoding::Pcm, 24u, 20u, false, 0u,
                            LargeFileContainer::Bw64);
    REQUIRE_THROWS_WITH(Bw64Writer(filename.c_str(), 2, 48000, format),
                        "valid bits require WAVE_FORMAT_EXTENSIBLE");
  }

  SECTION("non-extensible channel mask") {
    FormatDescriptor format(SampleEncoding::Pcm, 24u, 24u, false, 0x3u,
                            LargeFileContainer::Bw64);
    REQUIRE_THROWS_WITH(Bw64Writer(filename.c_str(), 2, 48000, format),
                        "channel mask requires WAVE_FORMAT_EXTENSIBLE");
  }

  SECTION("float cannot promote to BW64") {
    FormatDescriptor format(SampleEncoding::IeeeFloat, 32u, 32u, false, 0u,
                            LargeFileContainer::Bw64);
    REQUIRE_THROWS_WITH(Bw64Writer(filename.c_str(), 2, 48000, format),
                        "BW64 output supports only non-extensible PCM");
  }

  SECTION("extensible PCM cannot promote to BW64") {
    FormatDescriptor format(SampleEncoding::Pcm, 24u, 24u, true, 0x3u,
                            LargeFileContainer::Bw64);
    REQUIRE_THROWS_WITH(Bw64Writer(filename.c_str(), 2, 48000, format),
                        "BW64 output supports only non-extensible PCM");
  }

  std::ifstream file(filename.c_str(), std::ios::binary);
  REQUIRE_FALSE(file.good());
}

TEST_CASE("writer_emits_chna_only_when_explicitly_supplied") {
  std::string filename = "test_chna_tracks.wav";

  // Marker helpers must not invent ADM channel allocation metadata.
  {
    auto writer = createSharedWriterWithMarkers(filename, 2, 48000, 24);
    std::vector<float> data(100 * 2, 0.0f);
    writer->write(&data[0], 100);
    writer->close();
  }

  // Read back and verify CHNA chunk has correct track count
  {
    auto reader = readFile(filename);
    REQUIRE_FALSE(reader->chnaChunk());
    reader->close();
  }

  // Genuine caller-supplied CHNA remains supported.
  {
    std::vector<AudioId> audioIds;
    audioIds.emplace_back(1u, "ATU_00000001", "AT_00031001_01",
                          "AP_00031001");
    audioIds.emplace_back(2u, "ATU_00000002", "AT_00031002_01",
                          "AP_00031002");
    auto chna = std::make_shared<ChnaChunk>(audioIds);
    auto writer = writeFile(filename, 2, 48000, 24, chna);
    writer->close();
  }

  {
    auto reader = readFile(filename);
    auto chna = reader->chnaChunk();
    REQUIRE(chna);
    REQUIRE(chna->numTracks() == 2u);
    REQUIRE(chna->numUids() == 2u);
    reader->close();
  }

  // The compatibility setter can still insert CHNA before data, but only
  // before any audio has been written.
  {
    std::vector<AudioId> audioIds;
    audioIds.emplace_back(1u, "ATU_00000001", "AT_00031001_01",
                          "AP_00031001");
    auto writer = writeFile(filename, 1, 48000, 24);
    writer->setChnaChunk(std::make_shared<ChnaChunk>(audioIds));
    float frame = 0.0f;
    writer->write(&frame, 1u);
    writer->close();
  }

  {
    auto reader = readFile(filename);
    uint64_t chnaPosition = 0u;
    uint64_t dataPosition = 0u;
    for (const auto& header : reader->chunks()) {
      if (header.id == utils::fourCC("chna")) chnaPosition = header.position;
      if (header.id == utils::fourCC("data")) dataPosition = header.position;
    }
    REQUIRE(chnaPosition > 0u);
    REQUIRE(dataPosition > chnaPosition);
    reader->close();
  }

  {
    std::vector<AudioId> audioIds;
    audioIds.emplace_back(1u, "ATU_00000001", "AT_00031001_01",
                          "AP_00031001");
    auto writer = writeFile(filename, 1, 48000, 24);
    float frame = 0.0f;
    writer->write(&frame, 1u);
    REQUIRE_THROWS_WITH(
        writer->setChnaChunk(std::make_shared<ChnaChunk>(audioIds)),
        "chna chunk must be supplied before writing audio");
    writer->close();
  }

  remove(filename.c_str());
}

TEST_CASE("raw_byte_spans_support_packed_24_bit_frames") {
  const std::string filename = "raw_packed_24.wav";
  const uint64_t frames = 3u;
  std::vector<char> source{
      0x01, 0x02, 0x03, 0x11, 0x12, 0x13,
      0x21, 0x22, 0x23, 0x31, 0x32, 0x33,
      0x41, 0x42, 0x43, 0x51, 0x52, 0x53};

  {
    auto writer = writeFile(filename, 2u, 48000u, 24u);
    REQUIRE(writer->writeRaw(ConstByteSpan(source.data(), source.size()),
                             frames) == frames);
    writer->close();
  }

  {
    auto reader = readFile(filename);
    std::vector<char> destination(source.size(), 0);
    REQUIRE(reader->readRaw(ByteSpan(destination.data(), destination.size()),
                            frames) == frames);
    REQUIRE(destination == source);
    reader->close();
  }

  remove(filename.c_str());
}
