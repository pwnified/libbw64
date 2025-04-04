#include <catch2/catch.hpp>
#include "bw64/bw64.hpp"
#include "bw64/parser.hpp"
#include "bw64/utils.hpp"

using namespace bw64;

TEST_CASE("format_info_chunk") {
  // basic test
  {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00";  // blockAlignment = 2; bitsPerSample = 16
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 16));
    auto formatInfoChunk =
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 16);
    REQUIRE(formatInfoChunk->formatTag() == 1);
    REQUIRE(formatInfoChunk->channelCount() == 1);
    REQUIRE(formatInfoChunk->sampleRate() == 48000);
    REQUIRE(formatInfoChunk->bytesPerSecond() == 96000);
    REQUIRE(formatInfoChunk->blockAlignment() == 2);
    REQUIRE(formatInfoChunk->bitsPerSample() == 16);
    REQUIRE(formatInfoChunk->isExtensible() == false);
    REQUIRE(formatInfoChunk->extraData() == nullptr);
  }
  // wrong chunkSize
  {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00"  // blockAlignment = 2; bitsPerSample = 16
        "\x00\x00\x00\x00";
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 20));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 20),
        std::runtime_error);
  }
  // illegal formatTag
  {
    const char* formatChunkByteArray =
        "\x02\x00\x01\x00"  // formatTag = 2; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00";  // blockAlignment = 2; bitsPerSample = 16
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 16));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 16),
        std::runtime_error);
  }
  // wrong channelCount
  {
    const char* formatChunkByteArray =
        "\x01\x00\x00\x00"  // formatTag = 1; channelCount = 0
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00";  // blockAlignment = 2; bitsPerSample = 16
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 16));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 16),
        std::runtime_error);
  }
  // wrong samplerate
  {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x00\x00\x00\x00"  // sampleRate = 0
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00";  // blockAlignment = 2; bitsPerSample = 16
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 16));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 16),
        std::runtime_error);
  }
  // wrong bytesPerSecond
  {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x01\x77\x01\x00"  // bytesPerSecond = 96001
        "\x02\x00\x10\x00";  // blockAlignment = 2; bitsPerSample = 16
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 16));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 16),
        std::runtime_error);
  }
  // wrong blockAlignment
  {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x00\x00\x10\x00";  // blockAlignment = 2; bitsPerSample = 16
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 16));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 16),
        std::runtime_error);
  }
  // read/write
  {
    std::stringstream stream;
    auto formatChunk = std::make_shared<FormatInfoChunk>(2, 48000, 24);
    formatChunk->write(stream);
    auto formatChunkReread =
        parseFormatInfoChunk(stream, utils::fourCC("fmt "), 16);
    REQUIRE(formatChunkReread->channelCount() == 2);
    REQUIRE(formatChunkReread->sampleRate() == 48000);
    REQUIRE(formatChunkReread->bitsPerSample() == 24);
  }
  // blockAlignment error
  {
    REQUIRE_THROWS_WITH(
        FormatInfoChunk(0xffff, 48000, 24),
        "channelCount and bitsPerSample would overflow blockAlignment");
  }
  // bytesPerSecond error
  {
    REQUIRE_THROWS_WITH(FormatInfoChunk(0x1000, 0xffffffff, 16),
                        "sampleRate, channelCount and bitsPerSample would "
                        "overflow bytesPerSecond");
  }
}

TEST_CASE("format_info_chunk_extradata") {
  SECTION("cbSize = 0") {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00"  // blockAlignment = 2; bitsPerSample = 16
        "\x00\x00";  // cbSize = 0
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 18));
    auto formatInfoChunk =
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 18);
    REQUIRE(formatInfoChunk->formatTag() == 1);
    REQUIRE(formatInfoChunk->channelCount() == 1);
    REQUIRE(formatInfoChunk->sampleRate() == 48000);
    REQUIRE(formatInfoChunk->bytesPerSecond() == 96000);
    REQUIRE(formatInfoChunk->blockAlignment() == 2);
    REQUIRE(formatInfoChunk->bitsPerSample() == 16);
    REQUIRE(formatInfoChunk->extraData() == nullptr);
  }
  SECTION("cbSize too large") {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00"  // blockAlignment = 2; bitsPerSample = 16
        "\x16\x00";  // cbSize = 22
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 18));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 18),
        std::runtime_error);
  }
  SECTION("extensible") {
    const char* formatChunkByteArray =
        "\xfe\xff\x01\x00"  // formatTag = 0xfffe; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00"  // blockAlignment = 2; bitsPerSample = 16
        "\x16\x00"  // cbSize = 22
        "\x10\x00"  // validBitsPerSample = 16
        "\x04\x00\x00\x00"  // dwChannelMask = SPEAKER_FRONT_CENTER
        "\x01\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71";  // KSDATAFORMAT_SUBTYPE_PCM
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 40));
    auto formatInfoChunk =
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 40);
    REQUIRE(formatInfoChunk->isExtensible() == true);
    auto extraData = formatInfoChunk->extraData();
    REQUIRE(extraData != nullptr);
    REQUIRE(extraData->validBitsPerSample() == 16);
    REQUIRE(extraData->dwChannelMask() == 4);
    REQUIRE(extraData->subFormat().Data1 == 1);
    REQUIRE(guidsEqual(extraData->subFormat(), KSDATAFORMAT_SUBTYPE_PCM) == true);

    SECTION("write") {
      std::ostringstream written;
      formatInfoChunk->write(written);
      REQUIRE(formatChunkStream.str() == written.str());
    }
  }
  SECTION("PCM with extradata") {
    const char* formatChunkByteArray =
        "\x01\x00\x01\x00"  // formatTag = 1; channelCount = 1
        "\x80\xbb\x00\x00"  // sampleRate = 48000
        "\x00\x77\x01\x00"  // bytesPerSecond = 96000
        "\x02\x00\x10\x00"  // blockAlignment = 2; bitsPerSample = 16
        "\x16\x00"  // cbSize = 22
        "\x10\x00"  // validBitsPerSample = 16
        "\x04\x00\x00\x00"  // dwChannelMask = SPEAKER_FRONT_CENTER
        "\x01\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71";  // KSDATAFORMAT_SUBTYPE_PCM
    std::istringstream formatChunkStream(std::string(formatChunkByteArray, 40));
    REQUIRE_THROWS_AS(
        parseFormatInfoChunk(formatChunkStream, utils::fourCC("fmt "), 40),
        std::runtime_error);
  }
}

TEST_CASE("chna_chunk") {
  // basic test
  {
    const char* chnaChunkByteArray =
        "\x01\x00\x01\x00"  // numTracks = 1; numUids = 1
        "\x01\x00"  // trackIndex = 1
        "\x41\x54\x55\x5f\x30\x30\x30\x30\x30\x30\x30"  // ATU_00000001
        "\x31\x41\x54\x5f\x30\x30\x30\x33\x31\x30\x30\x31\x5f\x30"  // AT_00031001_01
        "\x31\x41\x50\x5f\x30\x30\x30\x33\x31\x30\x30\x31"  // AP_00031001
        "\x00";  // padding
    std::istringstream chnaChunkStream(std::string(chnaChunkByteArray, 44));
    auto chnaChunk = parseChnaChunk(chnaChunkStream, utils::fourCC("chna"), 44);
    REQUIRE(chnaChunk->numTracks() == 1);
    REQUIRE(chnaChunk->numUids() == 1);
    REQUIRE(chnaChunk->audioIds().size() == 1);
    REQUIRE(chnaChunk->audioIds()[0].trackIndex() == 1);
    REQUIRE(chnaChunk->audioIds()[0].uid() == "ATU_00000001");
    REQUIRE(chnaChunk->audioIds()[0].trackRef() == "AT_00031001_01");
    REQUIRE(chnaChunk->audioIds()[0].packRef() == "AP_00031001");
  }
  // read/write
  {
    std::stringstream stream;
    auto chnaChunk = std::make_shared<ChnaChunk>();
    chnaChunk->addAudioId(
        AudioId(1, "ATU_00000001", "AT_00031001_01", "AP_00031001"));
    chnaChunk->addAudioId(
        AudioId(1, "ATU_00000002", "AT_00031002_01", "AP_00031002"));
    chnaChunk->addAudioId(
        AudioId(2, "ATU_00000003", "AT_00031003_01", "AP_00031003"));
    chnaChunk->write(stream);

    auto chnaChunkReread = parseChnaChunk(stream, utils::fourCC("chna"), 124);
    REQUIRE(chnaChunkReread->numTracks() == 2);
    REQUIRE(chnaChunkReread->numUids() == 3);
    REQUIRE(chnaChunkReread->audioIds()[0].trackIndex() == 1);
    REQUIRE(chnaChunkReread->audioIds()[0].uid() == "ATU_00000001");
    REQUIRE(chnaChunkReread->audioIds()[0].trackRef() == "AT_00031001_01");
    REQUIRE(chnaChunkReread->audioIds()[0].packRef() == "AP_00031001");
    REQUIRE(chnaChunkReread->audioIds()[1].trackIndex() == 1);
    REQUIRE(chnaChunkReread->audioIds()[1].uid() == "ATU_00000002");
    REQUIRE(chnaChunkReread->audioIds()[1].trackRef() == "AT_00031002_01");
    REQUIRE(chnaChunkReread->audioIds()[1].packRef() == "AP_00031002");
    REQUIRE(chnaChunkReread->audioIds()[2].trackIndex() == 2);
    REQUIRE(chnaChunkReread->audioIds()[2].uid() == "ATU_00000003");
    REQUIRE(chnaChunkReread->audioIds()[2].trackRef() == "AT_00031003_01");
    REQUIRE(chnaChunkReread->audioIds()[2].packRef() == "AP_00031003");
  }
  // throws
  {  // wrong fourCC
    const char* chnaChunkByteArray = "\x00\x00";  // padding
    std::istringstream chnaChunkStream(std::string(chnaChunkByteArray, 2));
    REQUIRE_THROWS_AS(parseChnaChunk(chnaChunkStream, utils::fourCC("chni"), 2),
                      std::runtime_error);
  }
  {  // wrong size
    const char* chnaChunkByteArray = "\x00\x00";  // padding
    std::istringstream chnaChunkStream(std::string(chnaChunkByteArray, 2));
    REQUIRE_THROWS_AS(parseChnaChunk(chnaChunkStream, utils::fourCC("chna"), 2),
                      std::runtime_error);
  }
  {  // wrong size
    const char* chnaChunkByteArray = "\x00\x00";  // padding
    std::istringstream chnaChunkStream(std::string(chnaChunkByteArray, 2));
    REQUIRE_THROWS_AS(parseChnaChunk(chnaChunkStream, utils::fourCC("chna"), 2),
                      std::runtime_error);
  }
  {  // wrong numTracks
    const char* chnaChunkByteArray =
        "\x02\x00\x01\x00"  // numTracks = 2; numUids = 1
        "\x01\x00"  // trackIndex = 1
        "\x41\x54\x55\x5f\x30\x30\x30\x30\x30\x30\x30"  // ATU_00000001
        "\x31\x41\x54\x5f\x30\x30\x30\x33\x31\x30\x30\x31\x5f\x30"  // AT_00031001_01
        "\x31\x41\x50\x5f\x30\x30\x30\x33\x31\x30\x30\x31"  // AP_00031001
        "\x00";  // padding
    std::istringstream chnaChunkStream(std::string(chnaChunkByteArray, 44));
    REQUIRE_THROWS_AS(
        parseChnaChunk(chnaChunkStream, utils::fourCC("chna"), 44),
        std::runtime_error);
  }
  {  // wrong numUids
    const char* chnaChunkByteArray =
        "\x01\x00\x02\x00"  // numTracks = 1; numUids = 2
        "\x01\x00"  // trackIndex = 1
        "\x41\x54\x55\x5f\x30\x30\x30\x30\x30\x30\x30"  // ATU_00000001
        "\x31\x41\x54\x5f\x30\x30\x30\x33\x31\x30\x30\x31\x5f\x30"  // AT_00031001_01
        "\x31\x41\x50\x5f\x30\x30\x30\x33\x31\x30\x30\x31"  // AP_00031001
        "\x00";  // padding
    std::istringstream chnaChunkStream(std::string(chnaChunkByteArray, 44));
    REQUIRE_THROWS_AS(
        parseChnaChunk(chnaChunkStream, utils::fourCC("chna"), 44),
        std::runtime_error);
  }
  {  // zero trackIndex throws when writing
    std::stringstream stream;
    auto chnaChunk = std::make_shared<ChnaChunk>();
    chnaChunk->addAudioId(
        AudioId(0, "ATU_00000001", "AT_00031001_01", "AP_00031001"));
    REQUIRE_THROWS_AS(chnaChunk->write(stream), std::runtime_error);
  }
}

TEST_CASE("ds64_chunk") {
  // basic test
  {
    const char* ds64ChunkByteArray =
        "\x9a\xc6\x22\x31\xa5\x00\x00\x00"  // bw64Size = 709493966490
        "\xa4\x25\x87\xcc\x86\x00\x00\x00"  // dataSize = 578957026724
        "\x00\x00\x00\x00\x00\x00\x00\x00"  // dummySize = 0
        "\x01\x00\x00\x00"  // tableSize = 1
        "\x61\x78\x6d\x6c"  // chunkId = "axml"
        "\x30\x5a\xc8\x00\x00\x00\x00\x00";  // axmlChunkSize = 48
    std::istringstream ds64ChunkStream(std::string(ds64ChunkByteArray, 40));
    auto ds64Chunk =
        parseDataSize64Chunk(ds64ChunkStream, utils::fourCC("ds64"), 40);
    REQUIRE(ds64Chunk->bw64Size() == 709493966490);
    REQUIRE(ds64Chunk->dataSize() == 578957026724);
    REQUIRE(ds64Chunk->dummySize() == 0);
    auto axmlId = utils::fourCC("axml");
    REQUIRE(ds64Chunk->getChunkSize(axmlId) == 13130288);
  }
  // read/write
  {
    std::stringstream stream;
    auto dataSize64Chunk =
        std::make_shared<DataSize64Chunk>(987654321, 123456789);
    auto axmlId = utils::fourCC("axml");
    dataSize64Chunk->setChunkSize(axmlId, 654321);
    dataSize64Chunk->write(stream);
    auto dataSize64ChunkReread =
        parseDataSize64Chunk(stream, utils::fourCC("ds64"), 40);
    REQUIRE(dataSize64ChunkReread->bw64Size() == 987654321);
    REQUIRE(dataSize64ChunkReread->dataSize() == 123456789);
    REQUIRE(dataSize64ChunkReread->tableLength() == 1);
    REQUIRE(dataSize64ChunkReread->getChunkSize(axmlId) == 654321);
  }
  // throws
  {  // wrong fourCC
    const char* ds64ChunkByteArray =
        "\x9a\xc6\x22\x31\xa5\x00\x00\x00";  // bw64Size = 709493966490
    std::istringstream ds64ChunkStream(std::string(ds64ChunkByteArray, 8));
    REQUIRE_THROWS_AS(
        parseDataSize64Chunk(ds64ChunkStream, utils::fourCC("ds65"), 8),
        std::runtime_error);
  }
  {  // wrong size
    const char* ds64ChunkByteArray =
        "\x9a\xc6\x22\x31\xa5\x00\x00\x00";  // bw64Size = 709493966490
    std::istringstream ds64ChunkStream(std::string(ds64ChunkByteArray, 8));
    REQUIRE_THROWS_AS(
        parseDataSize64Chunk(ds64ChunkStream, utils::fourCC("ds64"), 8),
        std::runtime_error);
  }
}

TEST_CASE("axml_chunk") {
  size_t size = 200;

  std::string str_data(size, 0);
  const char* axml_str = "AXML";
  for (size_t i = 0; i < size; i++) str_data[i] = axml_str[i % 4];

  // check that null bytes are passed correctly
  str_data[100] = 0;

  AxmlChunk chunk(str_data);

  REQUIRE(chunk.size() == size);
  REQUIRE(chunk.data() == str_data);

  std::ostringstream stream;
  chunk.write(stream);

  REQUIRE(stream.tellp() == size);
  REQUIRE(stream.str() == str_data);
}

TEST_CASE("axml_chunk_bench", "[.bench]") {
  size_t size = 10000000;

  std::string str_data(size, 0);
  const char* pattern = "AXML";
  for (size_t i = 0; i < size; i++) str_data[i] = pattern[i % 4];

  BENCHMARK("from string copy") {
    AxmlChunk chunk(str_data);
    return chunk.size();
  };

  BENCHMARK_ADVANCED("from string move")(Catch::Benchmark::Chronometer meter) {
    std::string str_data_copy(str_data);

    meter.measure([&] {
      AxmlChunk chunk(std::move(str_data_copy));
      return chunk.size();
    });
  };

  BENCHMARK_ADVANCED("write")(Catch::Benchmark::Chronometer meter) {
    AxmlChunk chunk(str_data);
    std::ostringstream stream;

    meter.measure([&] {
      chunk.write(stream);
      return stream.tellp();
    });
  };
}


TEST_CASE("cue_chunk") {
  // basic test
  {
    const char* cueChunkByteArray =
    "\x02\x00\x00\x00"  // numCuePoints = 2
    // cue point 1
    "\x01\x00\x00\x00"  // id = 1
    "\x20\x4E\x00\x00"  // position = 20000
    "\x64\x61\x74\x61"  // dataChunkId = "data"
    "\x00\x00\x00\x00"  // chunkStart = 0
    "\x00\x00\x00\x00"  // blockStart = 0
    "\x20\x4E\x00\x00"  // sampleOffset = 20000
    // cue point 2
    "\x02\x00\x00\x00"  // id = 2
    "\x40\x9C\x00\x00"  // position = 40000
    "\x64\x61\x74\x61"  // dataChunkId = "data"
    "\x00\x00\x00\x00"  // chunkStart = 0
    "\x00\x00\x00\x00"  // blockStart = 0
    "\x40\x9C\x00\x00";  // sampleOffset = 40000
    std::istringstream cueChunkStream(std::string(cueChunkByteArray, 52));
    auto cueChunk = parseCueChunk(cueChunkStream, utils::fourCC("cue "), 52);
    REQUIRE(cueChunk->cuePoints().size() == 2);
    REQUIRE(cueChunk->cuePoints()[0].id == 1);
    REQUIRE(cueChunk->cuePoints()[0].position == 20000);
    REQUIRE(cueChunk->cuePoints()[0].dataChunkId == utils::fourCC("data"));
    REQUIRE(cueChunk->cuePoints()[0].chunkStart == 0);
    REQUIRE(cueChunk->cuePoints()[0].blockStart == 0);
    REQUIRE(cueChunk->cuePoints()[0].sampleOffset == 20000);
    REQUIRE(cueChunk->cuePoints()[1].id == 2);
    REQUIRE(cueChunk->cuePoints()[1].position == 40000);
    REQUIRE(cueChunk->cuePoints()[1].dataChunkId == utils::fourCC("data"));
    REQUIRE(cueChunk->cuePoints()[1].chunkStart == 0);
    REQUIRE(cueChunk->cuePoints()[1].blockStart == 0);
    REQUIRE(cueChunk->cuePoints()[1].sampleOffset == 40000);
  }
  // read/write
  {
    std::stringstream stream;
    std::vector<CuePoint> cuePoints;

    CuePoint cue1;
    cue1.id = 1;
    cue1.position = 20000;
    cue1.dataChunkId = utils::fourCC("data");
    cue1.chunkStart = 0;
    cue1.blockStart = 0;
    cue1.sampleOffset = 20000;
    cuePoints.push_back(cue1);

    CuePoint cue2;
    cue2.id = 2;
    cue2.position = 40000;
    cue2.dataChunkId = utils::fourCC("data");
    cue2.chunkStart = 0;
    cue2.blockStart = 0;
    cue2.sampleOffset = 40000;
    cuePoints.push_back(cue2);

    auto cueChunk = std::make_shared<CueChunk>(cuePoints);
    cueChunk->write(stream);

    auto cueChunkReread = parseCueChunk(stream, utils::fourCC("cue "), 52);
    REQUIRE(cueChunkReread->cuePoints().size() == 2);
    REQUIRE(cueChunkReread->cuePoints()[0].id == 1);
    REQUIRE(cueChunkReread->cuePoints()[0].position == 20000);
    REQUIRE(cueChunkReread->cuePoints()[0].dataChunkId == utils::fourCC("data"));
    REQUIRE(cueChunkReread->cuePoints()[0].sampleOffset == 20000);
    REQUIRE(cueChunkReread->cuePoints()[1].id == 2);
    REQUIRE(cueChunkReread->cuePoints()[1].position == 40000);
    REQUIRE(cueChunkReread->cuePoints()[1].sampleOffset == 40000);
  }
  // throws
  {  // wrong fourCC
    const char* cueChunkByteArray = "\x00\x00\x00\x00";  // numCuePoints = 0
    std::istringstream cueChunkStream(std::string(cueChunkByteArray, 4));
    REQUIRE_THROWS_AS(parseCueChunk(cueChunkStream, utils::fourCC("cuee"), 4),
                      std::runtime_error);
  }
  {  // wrong size (too small)
    const char* cueChunkByteArray = "\x01\x00\x00\x00";  // numCuePoints = 1
    std::istringstream cueChunkStream(std::string(cueChunkByteArray, 4));
    REQUIRE_THROWS_AS(parseCueChunk(cueChunkStream, utils::fourCC("cue "), 4),
                      std::runtime_error);
  }
  {  // size doesn't match number of cue points
    const char* cueChunkByteArray =
    "\x02\x00\x00\x00"  // numCuePoints = 2, but only data for 1
    "\x01\x00\x00\x00"  // id = 1
    "\x20\x4E\x00\x00"  // position = 20000
    "\x64\x61\x74\x61"  // dataChunkId = "data"
    "\x00\x00\x00\x00"  // chunkStart = 0
    "\x00\x00\x00\x00"  // blockStart = 0
    "\x20\x4E\x00\x00";  // sampleOffset = 20000
    std::istringstream cueChunkStream(std::string(cueChunkByteArray, 28));
    REQUIRE_THROWS_AS(parseCueChunk(cueChunkStream, utils::fourCC("cue "), 28),
                      std::runtime_error);
  }
}

TEST_CASE("label_chunk") {
  // basic test
  {
    const char* labelChunkByteArray =
    "\x01\x00\x00\x00"  // cuePointId = 1
    "\x4D\x61\x72\x6B\x65\x72\x20\x31\x00";  // label = "Marker 1" + null terminator
    std::istringstream labelChunkStream(std::string(labelChunkByteArray, 13));
    auto labelChunk = parseLabelChunk(labelChunkStream, utils::fourCC("labl"), 13);
    REQUIRE(labelChunk->cuePointId() == 1);
    REQUIRE(labelChunk->label() == "Marker 1");
  }
  // read/write
  {
    std::stringstream stream;
    auto labelChunk = std::make_shared<LabelChunk>(2, "Test Label");
    labelChunk->write(stream);

    auto labelChunkReread = parseLabelChunk(stream, utils::fourCC("labl"), 15);
    REQUIRE(labelChunkReread->cuePointId() == 2);
    REQUIRE(labelChunkReread->label() == "Test Label");
  }
  // throws
  {  // wrong fourCC
    const char* labelChunkByteArray =
    "\x01\x00\x00\x00"  // cuePointId = 1
    "\x54\x65\x73\x74\x00";  // label = "Test" + null terminator
    std::istringstream labelChunkStream(std::string(labelChunkByteArray, 9));
    REQUIRE_THROWS_AS(parseLabelChunk(labelChunkStream, utils::fourCC("label"), 9),
                      std::runtime_error);
  }
  {  // wrong size (too small)
    const char* labelChunkByteArray = "\x01\x00\x00\x00";  // only cuePointId, no label
    std::istringstream labelChunkStream(std::string(labelChunkByteArray, 4));
    REQUIRE_THROWS_AS(parseLabelChunk(labelChunkStream, utils::fourCC("labl"), 4),
                      std::runtime_error);
  }
  // test with empty label (should still have null terminator)
  {
    const char* labelChunkByteArray =
    "\x03\x00\x00\x00"  // cuePointId = 3
    "\x00";  // empty label with null terminator
    std::istringstream labelChunkStream(std::string(labelChunkByteArray, 5));
    auto labelChunk = parseLabelChunk(labelChunkStream, utils::fourCC("labl"), 5);
    REQUIRE(labelChunk->cuePointId() == 3);
    REQUIRE(labelChunk->label() == "");
  }
  // test with padding bytes after null terminator
  {
    const char* labelChunkByteArray =
    "\x04\x00\x00\x00"  // cuePointId = 4
    "\x54\x65\x73\x74\x00\x00\x00";  // label = "Test" + null terminator + 2 padding bytes
    std::istringstream labelChunkStream(std::string(labelChunkByteArray, 11));
    auto labelChunk = parseLabelChunk(labelChunkStream, utils::fourCC("labl"), 11);
    REQUIRE(labelChunk->cuePointId() == 4);
    REQUIRE(labelChunk->label() == "Test");  // padding should be ignored
  }
}

