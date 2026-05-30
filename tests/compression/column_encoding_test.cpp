#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "global/columnar_types.h"
#include "transport/compression/bit_packing_encoding.h"
#include "transport/compression/column_encoding.h"
#include "transport/kio/kio_db_reader.h"
#include "transport/kio/kio_db_writer.h"

namespace {

void ExpectRoundTrip(const ctp::Column& column, Schema::Types type,
                     kio::Encoding encoding, uint64_t row_count) {
    const IColumnEncoding& codec =
        GetEncoding(encoding);
    const std::vector<char> payload = codec.Encode(column, type);
    EXPECT_EQ(codec.Decode(payload, type, row_count), column);
}

void ExpectUnsignedBitPackingRoundTrip(
    const std::vector<uint64_t>& values) {
    const std::vector<char> payload = EncodeUnsignedValues(values);
    size_t offset = 0;
    EXPECT_EQ(DecodeUnsignedValues(payload, offset, values.size()), values);
    EXPECT_EQ(offset, payload.size());
}

std::filesystem::path CompressionTestOutputPath(
    const std::string& filename) {
    return std::filesystem::temp_directory_path() / filename;
}

}  // namespace

TEST(ColumnEncodingTest, RoundTripsPlain) {
    ctp::Column column{std::vector<int64_t>{10, -2, 30}};
    ExpectRoundTrip(column, Schema::BIGINT, kio::Encoding::PLAIN, 3);
}

TEST(ColumnEncodingTest, RoundTripsDelta) {
    ctp::Column column{std::vector<int64_t>{100, 105, 103, 200}};
    ExpectRoundTrip(column, Schema::BIGINT, kio::Encoding::DELTA, 4);
}

TEST(ColumnEncodingTest, RoundTripsDictionary) {
    ctp::Column column{std::vector<std::string>{"a", "b", "a", "c"}};
    ExpectRoundTrip(column, Schema::TEXT, kio::Encoding::DICTIONARY, 4);
}

TEST(ColumnEncodingTest, RoundTripsRle) {
    ctp::Column column{std::vector<char>{'a', 'a', 'b', 'b', 'b', 'c'}};
    ExpectRoundTrip(column, Schema::CHAR, kio::Encoding::RLE, 6);
}

TEST(ColumnEncodingTest, RoundTripsBitPacking) {
    ctp::Column column{std::vector<int32_t>{0, 1, -1, 32, -32}};
    ExpectRoundTrip(column, Schema::INTEGER, kio::Encoding::BIT_PACKING, 5);
}

TEST(ColumnEncodingTest, RoundTripsUnsignedBitPackingBoundaryWidths) {
    ExpectUnsignedBitPackingRoundTrip(std::vector<uint64_t>{0, 0, 0, 0});
    ExpectUnsignedBitPackingRoundTrip(std::vector<uint64_t>{0, 1, 1, 0, 1});
    ExpectUnsignedBitPackingRoundTrip(
        std::vector<uint64_t>{0, 63, 127, 5, 91});
    ExpectUnsignedBitPackingRoundTrip(
        std::vector<uint64_t>{0, 255, 256, 511, 17});
    ExpectUnsignedBitPackingRoundTrip(
        std::vector<uint64_t>{0, 65535, 131071, 42});
    ExpectUnsignedBitPackingRoundTrip(
        std::vector<uint64_t>{0, (uint64_t{1} << 31) - 1,
                              (uint64_t{1} << 30) + 7});
    ExpectUnsignedBitPackingRoundTrip(
        std::vector<uint64_t>{0, (uint64_t{1} << 63) - 1,
                              (uint64_t{1} << 62) + 123});
    ExpectUnsignedBitPackingRoundTrip(
        std::vector<uint64_t>{0, ~uint64_t{0}, uint64_t{1} << 63});
}

TEST(ColumnEncodingTest, RoundTripsUnsignedBitPackingAcrossWordBoundaries) {
    std::vector<uint64_t> width9_values;
    width9_values.reserve(101);
    for (uint64_t idx = 0; idx < 100; ++idx) {
        width9_values.push_back((idx * 37) % 512);
    }
    width9_values.push_back(511);
    ExpectUnsignedBitPackingRoundTrip(width9_values);

    std::vector<uint64_t> width17_values;
    width17_values.reserve(96);
    for (uint64_t idx = 0; idx < 96; ++idx) {
        width17_values.push_back((idx * 4099) % 131072);
    }
    ExpectUnsignedBitPackingRoundTrip(width17_values);
}

TEST(ColumnEncodingTest, RoundTripsDeltaLengthByteArray) {
    ctp::Column column{std::vector<std::string>{"", "alpha", "bb"}};
    ExpectRoundTrip(
        column, Schema::VARCHAR, kio::Encoding::DELTA_LENGTH_BYTE_ARRAY, 3);
}

TEST(ColumnEncodingTest, PrepareColumnUsesFixedMapping) {
    ctp::Column numbers{std::vector<int32_t>{1, 2, 4}};
    PreparedColumn number_prepared =
        PrepareColumnForWrite(numbers, Schema::INTEGER);
    EXPECT_EQ(number_prepared.encoding, kio::Encoding::DELTA);
    EXPECT_EQ(number_prepared.compression, kio::Compression::NONE);
    EXPECT_EQ(DecodeColumnForRead(
                  number_prepared.payload, Schema::INTEGER,
                  number_prepared.encoding, number_prepared.compression, 3,
                  number_prepared.uncompressed_size),
              numbers);

    ctp::Column strings{std::vector<std::string>{"x", "x", "y"}};
    PreparedColumn string_prepared =
        PrepareColumnForWrite(strings, Schema::TEXT);
    EXPECT_EQ(string_prepared.encoding, kio::Encoding::DICTIONARY);

    ctp::Column chars{std::vector<char>{'q', 'q', 'z'}};
    PreparedColumn char_prepared =
        PrepareColumnForWrite(chars, Schema::CHAR);
    EXPECT_EQ(char_prepared.encoding, kio::Encoding::RLE);

    ctp::Column doubles{std::vector<double>{1.5, -2.25, 3.75}};
    PreparedColumn double_prepared =
        PrepareColumnForWrite(doubles, Schema::DOUBLE);
    EXPECT_EQ(double_prepared.encoding, kio::Encoding::PLAIN);
    EXPECT_EQ(double_prepared.compression, kio::Compression::NONE);
    EXPECT_EQ(DecodeColumnForRead(
                  double_prepared.payload, Schema::DOUBLE,
                  double_prepared.encoding, double_prepared.compression, 3,
                  double_prepared.uncompressed_size),
              doubles);
}

TEST(ColumnEncodingTest, KioRoundTripsEncodedColumnsAndMetadata) {
    const std::filesystem::path path_db =
        CompressionTestOutputPath("kio_db_encoded_columns_test.kiodb");

    std::error_code ec;
    std::filesystem::remove(path_db, ec);

    Schema schema({
        {"big", "BIGINT"},
        {"integer", "INTEGER"},
        {"small", "SMALLINT"},
        {"text", "TEXT"},
        {"varchar", "VARCHAR"},
        {"ch", "CHAR"},
        {"ts", "TIMESTAMP"},
        {"date", "DATE"},
        {"double_col", "DOUBLE"},
    });

    ctp::ColumnarBatch expected = {
        ctp::Column{std::vector<int64_t>{100, 101, 110}},
        ctp::Column{std::vector<int32_t>{1, 2, 3}},
        ctp::Column{std::vector<int16_t>{4, 4, 5}},
        ctp::Column{std::vector<std::string>{"alpha", "beta", "alpha"}},
        ctp::Column{std::vector<std::string>{"x", "x", "y"}},
        ctp::Column{std::vector<char>{'a', 'a', 'b'}},
        ctp::Column{std::vector<int64_t>{1000, 1060, 1120}},
        ctp::Column{std::vector<int32_t>{10, 11, 12}},
        ctp::Column{std::vector<double>{1.25, -2.5, 3.75}},
    };

    {
        KioDbWriter writer(path_db.string(), schema);
        writer.WriteBatchToFile(expected);
    }

    KioDbReader reader(path_db.string());
    const kio::FileMetadata& metadata = reader.GetMetadata();
    ASSERT_EQ(metadata.row_groups.size(), 1u);
    ASSERT_EQ(metadata.row_groups[0].columns.size(), schema.ColumnCount());
    EXPECT_EQ(metadata.row_groups[0].columns[0].encoding,
              kio::Encoding::DELTA);
    EXPECT_EQ(metadata.row_groups[0].columns[3].encoding,
              kio::Encoding::DICTIONARY);
    EXPECT_EQ(metadata.row_groups[0].columns[5].encoding,
              kio::Encoding::RLE);
    EXPECT_EQ(metadata.row_groups[0].columns[8].encoding,
              kio::Encoding::PLAIN);
    EXPECT_EQ(metadata.row_groups[0].columns[0].compression,
              kio::Compression::NONE);

    uint64_t expected_offset = 0;
    uint64_t expected_batch_size = 0;
    for (const auto& column : metadata.row_groups[0].columns) {
        EXPECT_EQ(column.local_offset, expected_offset);
        EXPECT_EQ(column.size, column.compressed_size);
        EXPECT_EQ(column.size, column.uncompressed_size);
        EXPECT_EQ(column.compression, kio::Compression::NONE);
        expected_offset += column.size;
        expected_batch_size += column.size;
    }
    EXPECT_EQ(metadata.row_groups[0].batch.batch_size, expected_batch_size);

    std::optional<ctp::ColumnarBatch> actual = reader.ReadNextBatch();
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(*actual, expected);
    EXPECT_EQ(metadata.row_groups[0].batch.row_num, 3u);
    EXPECT_FALSE(reader.ReadNextBatch().has_value());

    std::filesystem::remove(path_db, ec);
}
