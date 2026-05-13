#include "csv_work/csv_row_parser.h"
#include "gtest/gtest.h"

TEST(RowParserTest, JustWorks) {
    CSVRowParser parser("a,a,b\n");
    std::vector<std::string> expected{"a", "a", "b"};
    EXPECT_EQ(parser.ParseNext(), expected);
}

TEST(RowParserTest, NoNewLine) {
    CSVRowParser parser("a,a,b");
    std::vector<std::string> expected{"a", "a", "b"};
    EXPECT_EQ(parser.ParseNext(), expected);
}

TEST(RowParserTest, InQuote) {
    CSVRowParser parser(R"("a""a""",b," b""b""")");
    std::vector<std::string> expected{"a\"a\"", "b", " b\"b\""};
    EXPECT_EQ(parser.ParseNext(), expected);
}

TEST(RowParserTest, Comma) {
    CSVRowParser parser(R"("a,b",c,d,"e,f")");
    std::vector<std::string> expected{"a,b", "c", "d", "e,f"};
    EXPECT_EQ(parser.ParseNext(), expected);
}

TEST(RowParserTest, NewLineInQuotedField) {
    CSVRowParser parser(
        "\"first line\nsecond line\",value\n"
        "next,row\n");

    std::vector<std::string> expected{"first line\nsecond line", "value"};
    EXPECT_EQ(parser.ParseNext(), expected);

    expected = {"next", "row"};
    EXPECT_EQ(parser.ParseNext(), expected);
}

TEST(RowParserTest, EmptyField) {
    CSVRowParser parser(
        "a,,\n"
        "a,b,\n");

    std::vector<std::string> expected = {"a", "", ""};
    EXPECT_EQ(parser.ParseNext(), expected);

    expected = {"a", "b", ""};
    EXPECT_EQ(parser.ParseNext(), expected);
}

TEST(RowParserTest, EmptyLineIsARow) {
    CSVRowParser parser("\na,b\n");

    std::vector<std::string> expected{""};
    EXPECT_EQ(parser.ParseNext(), expected);

    expected = {"a", "b"};
    EXPECT_EQ(parser.ParseNext(), expected);

    expected.clear();
    EXPECT_EQ(parser.ParseNext(), expected);
}

TEST(RowParserTest, UnclosedQuoteAtEndThrows) {
    CSVRowParser parser("\"");
    EXPECT_THROW(parser.ParseNext(), std::invalid_argument);
}

TEST(RowParserTest, SeveralRows) {
    CSVRowParser parser(R"("a",a,b
b,a,"c"
d,g,c
)");

    std::vector<std::string> expected{"a", "a", "b"};
    EXPECT_EQ(parser.ParseNext(), expected);

    expected = {"b", "a", "c"};
    EXPECT_EQ(parser.ParseNext(), expected);

    expected = {"d", "g", "c"};
    EXPECT_EQ(parser.ParseNext(), expected);

    expected.clear();
    EXPECT_EQ(parser.ParseNext(), expected);
}
