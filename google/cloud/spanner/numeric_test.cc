// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/numeric.h"
#include "absl/numeric/int128.h"
#include <gmock/gmock.h>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::HasSubstr;
using ::testing::Matches;

auto constexpr kNumericIntMin =  // 1 - 10^Numeric::kIntPrec
    absl::MakeInt128(-5421010863, 10560352017195204609U);
auto constexpr kNumericIntMax =  // 10^Numeric::kIntPrec - 1
    absl::MakeInt128(5421010862, 7886392056514347007U);

// NOLINTNEXTLINE(readability-redundant-string-init)
MATCHER_P2(HasStatus, code, message, "") {
  return Matches(code)(arg.code()) &&
         Matches(HasSubstr(message))(arg.message());
}

TEST(Numeric, DefaultCtor) {
  Numeric n;
  EXPECT_EQ(0, ToInteger<int>(n).value());
  EXPECT_EQ(0U, ToInteger<unsigned>(n).value());
  EXPECT_EQ(0.0, ToDouble(n));
  EXPECT_EQ("0", n.ToString());
}

TEST(Numeric, RegularSemantics) {
  Numeric n = MakeNumeric(42).value();

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  Numeric const copy1(n);
  EXPECT_EQ(copy1, n);

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  Numeric const copy2 = n;
  EXPECT_EQ(copy2, n);

  Numeric assign;
  assign = n;
  EXPECT_EQ(assign, n);
}

TEST(Numeric, RelationalOperators) {
  EXPECT_EQ(MakeNumeric(1).value(), MakeNumeric(1U).value());
  EXPECT_NE(MakeNumeric(-2).value(), MakeNumeric(2U).value());
}

TEST(Numeric, OutputStreaming) {
  auto stream = [](Numeric const& n) {
    std::ostringstream ss;
    ss << n;
    return std::move(ss).str();
  };

  // These are just like `ToString()`, so no need to be extensive.
  EXPECT_EQ("0", stream(MakeNumeric("-0").value()));
  EXPECT_EQ("0", stream(MakeNumeric(0.0).value()));
  EXPECT_EQ("0", stream(MakeNumeric(0).value()));
  EXPECT_EQ("-1.5", stream(MakeNumeric("-1.5").value()));
  EXPECT_EQ("-1.5", stream(MakeNumeric(-1.5).value()));
  EXPECT_EQ("-1", stream(MakeNumeric(-1).value()));
  EXPECT_EQ(
      "99999999999999999999999999999.999999999",
      stream(MakeNumeric("99999999999999999999999999999.999999999").value()));
}

TEST(Numeric, MakeNumericString) {
  // Various forms of zero.
  EXPECT_EQ("0", MakeNumeric("0").value().ToString());
  EXPECT_EQ("0", MakeNumeric("+0").value().ToString());
  EXPECT_EQ("0", MakeNumeric("-0").value().ToString());
  EXPECT_EQ("0", MakeNumeric(".0").value().ToString());
  EXPECT_EQ("0", MakeNumeric("0.").value().ToString());
  EXPECT_EQ("0", MakeNumeric("0.0").value().ToString());
  EXPECT_EQ("0", MakeNumeric("-00.00e100").value().ToString());

  // Fixed-point notation.
  EXPECT_EQ("1", MakeNumeric("1").value().ToString());
  EXPECT_EQ("12", MakeNumeric("12").value().ToString());
  EXPECT_EQ("12", MakeNumeric("12.").value().ToString());
  EXPECT_EQ("1", MakeNumeric("+1").value().ToString());
  EXPECT_EQ("1", MakeNumeric("+01").value().ToString());
  EXPECT_EQ("12", MakeNumeric("+12").value().ToString());
  EXPECT_EQ("12", MakeNumeric("+12.").value().ToString());
  EXPECT_EQ("-1", MakeNumeric("-1").value().ToString());
  EXPECT_EQ("-1", MakeNumeric("-01").value().ToString());
  EXPECT_EQ("-12", MakeNumeric("-12").value().ToString());
  EXPECT_EQ("-12", MakeNumeric("-12.").value().ToString());
  EXPECT_EQ("1.3", MakeNumeric("1.3").value().ToString());
  EXPECT_EQ("12.3", MakeNumeric("12.3").value().ToString());
  EXPECT_EQ("1.3", MakeNumeric("+1.3").value().ToString());
  EXPECT_EQ("12.3", MakeNumeric("+12.3").value().ToString());
  EXPECT_EQ("-1.3", MakeNumeric("-1.3").value().ToString());
  EXPECT_EQ("-12.3", MakeNumeric("-12.3").value().ToString());
  EXPECT_EQ("1.34", MakeNumeric("1.34").value().ToString());
  EXPECT_EQ("12.34", MakeNumeric("12.34").value().ToString());
  EXPECT_EQ("1.34", MakeNumeric("+1.34").value().ToString());
  EXPECT_EQ("12.34", MakeNumeric("+12.34").value().ToString());
  EXPECT_EQ("-1.34", MakeNumeric("-1.34").value().ToString());
  EXPECT_EQ("-12.34", MakeNumeric("-12.34").value().ToString());

  // Floating-point notation with single-digit exponent.
  EXPECT_EQ("10", MakeNumeric("1e1").value().ToString());
  EXPECT_EQ("10", MakeNumeric("1.e1").value().ToString());
  EXPECT_EQ("1", MakeNumeric(".1e1").value().ToString());
  EXPECT_EQ("120", MakeNumeric("12E1").value().ToString());
  EXPECT_EQ("120", MakeNumeric("12.E1").value().ToString());
  EXPECT_EQ("1.2", MakeNumeric(".12E1").value().ToString());
  EXPECT_EQ("10", MakeNumeric("+1e1").value().ToString());
  EXPECT_EQ("10", MakeNumeric("+1.e1").value().ToString());
  EXPECT_EQ("1", MakeNumeric("+.1e1").value().ToString());
  EXPECT_EQ("120", MakeNumeric("+12E1").value().ToString());
  EXPECT_EQ("120", MakeNumeric("+12.E1").value().ToString());
  EXPECT_EQ("1.2", MakeNumeric("+.12E1").value().ToString());
  EXPECT_EQ("-10", MakeNumeric("-1e1").value().ToString());
  EXPECT_EQ("-10", MakeNumeric("-1.e1").value().ToString());
  EXPECT_EQ("-1", MakeNumeric("-.1e1").value().ToString());
  EXPECT_EQ("-120", MakeNumeric("-12E1").value().ToString());
  EXPECT_EQ("-120", MakeNumeric("-12.E1").value().ToString());
  EXPECT_EQ("-1.2", MakeNumeric("-.12E1").value().ToString());
  EXPECT_EQ("13", MakeNumeric("1.3e1").value().ToString());
  EXPECT_EQ("123", MakeNumeric("12.3E1").value().ToString());
  EXPECT_EQ("13", MakeNumeric("+1.3e1").value().ToString());
  EXPECT_EQ("123", MakeNumeric("+12.3E1").value().ToString());
  EXPECT_EQ("-13", MakeNumeric("-1.3e1").value().ToString());
  EXPECT_EQ("-123", MakeNumeric("-12.3E1").value().ToString());
  EXPECT_EQ("13.4", MakeNumeric("1.34e1").value().ToString());
  EXPECT_EQ("123.4", MakeNumeric("12.34E1").value().ToString());
  EXPECT_EQ("13.4", MakeNumeric("+1.34e1").value().ToString());
  EXPECT_EQ("123.4", MakeNumeric("+12.34E1").value().ToString());
  EXPECT_EQ("-13.4", MakeNumeric("-1.34e1").value().ToString());
  EXPECT_EQ("-123.4", MakeNumeric("-12.34E1").value().ToString());

  // Floating-point notation with double-digit exponent.
  EXPECT_EQ("10000000000", MakeNumeric("1e+10").value().ToString());
  EXPECT_EQ("1000000000", MakeNumeric(".1e+10").value().ToString());
  EXPECT_EQ("120000000000", MakeNumeric("12E+10").value().ToString());
  EXPECT_EQ("1200000000", MakeNumeric(".12E+10").value().ToString());
  EXPECT_EQ("10000000000", MakeNumeric("+1e+10").value().ToString());
  EXPECT_EQ("1000000000", MakeNumeric("+.1e+10").value().ToString());
  EXPECT_EQ("120000000000", MakeNumeric("+12E+10").value().ToString());
  EXPECT_EQ("1200000000", MakeNumeric("+.12E+10").value().ToString());
  EXPECT_EQ("-10000000000", MakeNumeric("-1e+10").value().ToString());
  EXPECT_EQ("-1000000000", MakeNumeric("-.1e+10").value().ToString());
  EXPECT_EQ("-120000000000", MakeNumeric("-12E+10").value().ToString());
  EXPECT_EQ("-1200000000", MakeNumeric("-.12E+10").value().ToString());
  EXPECT_EQ("13000000000", MakeNumeric("1.3e+10").value().ToString());
  EXPECT_EQ("123000000000", MakeNumeric("12.3E+10").value().ToString());
  EXPECT_EQ("13000000000", MakeNumeric("+1.3e+10").value().ToString());
  EXPECT_EQ("123000000000", MakeNumeric("+12.3E+10").value().ToString());
  EXPECT_EQ("-13000000000", MakeNumeric("-1.3e+10").value().ToString());
  EXPECT_EQ("-123000000000", MakeNumeric("-12.3E+10").value().ToString());
  EXPECT_EQ("13400000000", MakeNumeric("1.34e+10").value().ToString());
  EXPECT_EQ("123400000000", MakeNumeric("12.34E+10").value().ToString());
  EXPECT_EQ("13400000000", MakeNumeric("+1.34e+10").value().ToString());
  EXPECT_EQ("123400000000", MakeNumeric("+12.34E+10").value().ToString());
  EXPECT_EQ("-13400000000", MakeNumeric("-1.34e+10").value().ToString());
  EXPECT_EQ("-123400000000", MakeNumeric("-12.34E+10").value().ToString());

  // Floating-point notation with negative exponent.
  EXPECT_EQ("0.001", MakeNumeric("1e-3").value().ToString());
  EXPECT_EQ("0.0001", MakeNumeric(".1e-3").value().ToString());
  EXPECT_EQ("0.012", MakeNumeric("12E-3").value().ToString());
  EXPECT_EQ("0.00012", MakeNumeric(".12E-3").value().ToString());
  EXPECT_EQ("0.001", MakeNumeric("+1e-3").value().ToString());
  EXPECT_EQ("0.0001", MakeNumeric("+.1e-3").value().ToString());
  EXPECT_EQ("0.012", MakeNumeric("+12E-3").value().ToString());
  EXPECT_EQ("0.00012", MakeNumeric("+.12E-3").value().ToString());
  EXPECT_EQ("-0.001", MakeNumeric("-1e-3").value().ToString());
  EXPECT_EQ("-0.0001", MakeNumeric("-.1e-3").value().ToString());
  EXPECT_EQ("-0.012", MakeNumeric("-12E-3").value().ToString());
  EXPECT_EQ("-0.00012", MakeNumeric("-.12E-3").value().ToString());
  EXPECT_EQ("0.0013", MakeNumeric("1.3e-3").value().ToString());
  EXPECT_EQ("0.0123", MakeNumeric("12.3E-3").value().ToString());
  EXPECT_EQ("0.0013", MakeNumeric("+1.3e-3").value().ToString());
  EXPECT_EQ("0.0123", MakeNumeric("+12.3E-3").value().ToString());
  EXPECT_EQ("-0.0013", MakeNumeric("-1.3e-3").value().ToString());
  EXPECT_EQ("-0.0123", MakeNumeric("-12.3E-3").value().ToString());
  EXPECT_EQ("0.00134", MakeNumeric("1.34e-3").value().ToString());
  EXPECT_EQ("0.01234", MakeNumeric("12.34E-3").value().ToString());
  EXPECT_EQ("0.00134", MakeNumeric("+1.34e-3").value().ToString());
  EXPECT_EQ("0.01234", MakeNumeric("+12.34E-3").value().ToString());
  EXPECT_EQ("-0.00134", MakeNumeric("-1.34e-3").value().ToString());
  EXPECT_EQ("-0.01234", MakeNumeric("-12.34E-3").value().ToString());

  // Floating-point notation with large exponent.
  EXPECT_EQ("-9.9",
            MakeNumeric("-0.0000000000000000000000000000000000000000099e42")
                .value()
                .ToString());
  EXPECT_EQ("9.9",
            MakeNumeric("9900000000000000000000000000000000000000000e-42")
                .value()
                .ToString());

  // The extreme `Numeric` values.
  EXPECT_EQ("-99999999999999999999999999999.999999999",
            MakeNumeric("-99999999999999999999999999999.999999999")
                .value()
                .ToString());
  EXPECT_EQ("99999999999999999999999999999.999999999",
            MakeNumeric("99999999999999999999999999999.999999999")
                .value()
                .ToString());
}

TEST(Numeric, MakeNumericStringFail) {
  // Valid chars, but incomplete.
  EXPECT_THAT(MakeNumeric("").status(),
              HasStatus(StatusCode::kInvalidArgument, ""));
  EXPECT_THAT(MakeNumeric("+").status(),
              HasStatus(StatusCode::kInvalidArgument, "+"));
  EXPECT_THAT(MakeNumeric("-").status(),
              HasStatus(StatusCode::kInvalidArgument, "-"));
  EXPECT_THAT(MakeNumeric(".").status(),
              HasStatus(StatusCode::kInvalidArgument, "."));

  // Invalid char in input.
  EXPECT_THAT(MakeNumeric("X").status(),
              HasStatus(StatusCode::kInvalidArgument, "X"));
  EXPECT_THAT(MakeNumeric("12345.6789X").status(),
              HasStatus(StatusCode::kInvalidArgument, "12345.6789X"));
  EXPECT_THAT(MakeNumeric("1.2e3X").status(),
              HasStatus(StatusCode::kInvalidArgument, "1.2e3X"));

  // Values beyond the allowed range.
  EXPECT_THAT(MakeNumeric("-1e30").status(),
              HasStatus(StatusCode::kOutOfRange, "-1e30"));
  EXPECT_THAT(MakeNumeric("1e30").status(),
              HasStatus(StatusCode::kOutOfRange, "1e30"));
  EXPECT_THAT(MakeNumeric("1e9223372036854775808").status(),
              HasStatus(StatusCode::kOutOfRange, "1e9223372036854775808"));

  // Values beyond the allowed range after rounding.
  EXPECT_THAT(MakeNumeric("-99999999999999999999999999999.9999999995").status(),
              HasStatus(StatusCode::kOutOfRange,
                        "-99999999999999999999999999999.9999999995"));
  EXPECT_THAT(MakeNumeric("99999999999999999999999999999.9999999995").status(),
              HasStatus(StatusCode::kOutOfRange,
                        "99999999999999999999999999999.9999999995"));
}

TEST(Numeric, MakeNumericStringRounding) {
  // If the argument has more than 9 digits after the decimal point
  // it will be rounded, with halfway cases rounding away from zero.
  EXPECT_EQ("0.899989999", MakeNumeric("0.8999899994").value().ToString());
  EXPECT_EQ("0.89999", MakeNumeric("0.8999899995").value().ToString());
  EXPECT_EQ("0.899999999", MakeNumeric("0.8999999994").value().ToString());
  EXPECT_EQ("0.9", MakeNumeric("0.8999999995").value().ToString());
  EXPECT_EQ("0.999989999", MakeNumeric(".9999899994").value().ToString());
  EXPECT_EQ("0.99999", MakeNumeric(".9999899995").value().ToString());
  EXPECT_EQ("0.999999999", MakeNumeric(".9999999994").value().ToString());
  EXPECT_EQ("1", MakeNumeric(".9999999995").value().ToString());
  EXPECT_EQ("99.999999999", MakeNumeric("99.9999999994").value().ToString());
  EXPECT_EQ("100", MakeNumeric("99.9999999995").value().ToString());

  EXPECT_EQ("90000000000000000000000000000",
            MakeNumeric("89999999999999999999999999999.9999999999")
                .value()
                .ToString());
  EXPECT_EQ("-99999999999999999999999999999.999999999",
            MakeNumeric("-99999999999999999999999999999.9999999989")
                .value()
                .ToString());

  EXPECT_EQ(
      std::int64_t{-9223372036854775807} - 1,
      ToInteger<std::int64_t>(MakeNumeric("-9223372036854775807.5").value())
          .value());
  EXPECT_EQ(
      std::uint64_t{18446744073709551615ULL},
      ToInteger<std::uint64_t>(MakeNumeric("18446744073709551614.5").value())
          .value());
}

TEST(Numeric, MakeNumericStringRoundingFail) {
  EXPECT_THAT(
      ToInteger<std::int64_t>(MakeNumeric("-9223372036854775808.5").value())
          .status(),
      HasStatus(StatusCode::kDataLoss, "-9223372036854775808.5"));
  EXPECT_THAT(
      ToInteger<std::uint64_t>(MakeNumeric("18446744073709551615.5").value())
          .status(),
      HasStatus(StatusCode::kDataLoss, "18446744073709551615.5"));
}

TEST(Numeric, MakeNumericDouble) {
  // Zero can be matched exactly.
  EXPECT_EQ(0.0, ToDouble(MakeNumeric(0.0).value()));
  EXPECT_EQ(0.0,
            ToDouble(MakeNumeric(std::numeric_limits<double>::min()).value()));
  EXPECT_EQ(
      0.0,
      ToDouble(MakeNumeric(std::numeric_limits<double>::epsilon()).value()));

  // Values near the allowed limits.
  EXPECT_DOUBLE_EQ(-0.9999999999999999e29,
                   ToDouble(MakeNumeric(-0.9999999999999999e29).value()));
  EXPECT_DOUBLE_EQ(0.9999999999999999e29,
                   ToDouble(MakeNumeric(0.9999999999999999e29).value()));
  EXPECT_DOUBLE_EQ(
      -0.9999999999999999e+29,
      ToDouble(MakeNumeric(-99999999999999999999999999999.999999999).value()));
  EXPECT_DOUBLE_EQ(
      0.9999999999999999e+29,
      ToDouble(MakeNumeric(99999999999999999999999999999.999999999).value()));

  // Extract values at the allowed limits.
  EXPECT_DOUBLE_EQ(
      -9.999999999999999e+28,
      ToDouble(
          MakeNumeric("-99999999999999999999999999999.999999999").value()));
  EXPECT_DOUBLE_EQ(
      9.999999999999999e+28,
      ToDouble(MakeNumeric("99999999999999999999999999999.999999999").value()));

  // If the argument has more than 9 digits after the decimal point
  // it will be rounded, with halfway cases rounding away from zero.
  EXPECT_DOUBLE_EQ(12345679e-9, ToDouble(MakeNumeric(12345678.9e-9).value()));
  EXPECT_DOUBLE_EQ(1234568e-9, ToDouble(MakeNumeric(1234567.89e-9).value()));
  EXPECT_DOUBLE_EQ(123457e-9, ToDouble(MakeNumeric(123456.789e-9).value()));
  EXPECT_DOUBLE_EQ(12346e-9, ToDouble(MakeNumeric(12345.6789e-9).value()));
  EXPECT_DOUBLE_EQ(1235e-9, ToDouble(MakeNumeric(1234.56789e-9).value()));
  EXPECT_DOUBLE_EQ(123e-9, ToDouble(MakeNumeric(123.456789e-9).value()));
  EXPECT_DOUBLE_EQ(12e-9, ToDouble(MakeNumeric(12.3456789e-9).value()));
  EXPECT_DOUBLE_EQ(1e-9, ToDouble(MakeNumeric(1.23456789e-9).value()));
  EXPECT_EQ(0.0, ToDouble(MakeNumeric(0.123456789e-9).value()));
}

TEST(Numeric, MakeNumericDoubleFail) {
  EXPECT_THAT(MakeNumeric(1e30).status(),
              HasStatus(StatusCode::kOutOfRange, "1e+30"));
  EXPECT_THAT(MakeNumeric(-1e30).status(),
              HasStatus(StatusCode::kOutOfRange, "-1e+30"));

  // Assumes that `double` can hold at least 1e+30.
  EXPECT_THAT(MakeNumeric(std::numeric_limits<double>::max()).status(),
              HasStatus(StatusCode::kOutOfRange, "e+"));
  EXPECT_THAT(MakeNumeric(std::numeric_limits<double>::lowest()).status(),
              HasStatus(StatusCode::kOutOfRange, "e+"));

  // NaN and infinities count as outside the allowable range.
  EXPECT_THAT(MakeNumeric(std::numeric_limits<double>::quiet_NaN()).status(),
              HasStatus(StatusCode::kOutOfRange, "nan"));
  EXPECT_THAT(MakeNumeric(std::numeric_limits<double>::infinity()).status(),
              HasStatus(StatusCode::kOutOfRange, "inf"));
  EXPECT_THAT(MakeNumeric(-std::numeric_limits<double>::infinity()).status(),
              HasStatus(StatusCode::kOutOfRange, "-inf"));
}

TEST(Numeric, MakeNumericInteger) {
  // Zero, signed and unsigned.
  EXPECT_EQ(0, ToInteger<int>(MakeNumeric(0).value()).value());
  EXPECT_EQ(0U, ToInteger<unsigned>(MakeNumeric(0U).value()).value());

  // 8-bit types.
  EXPECT_EQ(std::numeric_limits<std::int8_t>::min(),
            ToInteger<std::int8_t>(
                MakeNumeric(std::numeric_limits<std::int8_t>::min()).value())
                .value());
  EXPECT_EQ(std::numeric_limits<std::int8_t>::max(),
            ToInteger<std::int8_t>(
                MakeNumeric(std::numeric_limits<std::int8_t>::max()).value())
                .value());
  EXPECT_EQ(std::numeric_limits<std::uint8_t>::max(),
            ToInteger<std::uint8_t>(
                MakeNumeric(std::numeric_limits<std::uint8_t>::max()).value())
                .value());

  // 64-bit types.
  EXPECT_EQ(std::numeric_limits<std::int64_t>::min(),
            ToInteger<std::int64_t>(
                MakeNumeric(std::numeric_limits<std::int64_t>::min()).value())
                .value());
  EXPECT_EQ(std::numeric_limits<std::int64_t>::max(),
            ToInteger<std::int64_t>(
                MakeNumeric(std::numeric_limits<std::int64_t>::max()).value())
                .value());
  EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(),
            ToInteger<std::uint64_t>(
                MakeNumeric(std::numeric_limits<std::uint64_t>::max()).value())
                .value());

  // 128-bit types, which can represent the full integral NUMERIC range.
  EXPECT_EQ(
      kNumericIntMin,
      ToInteger<absl::int128>(MakeNumeric(kNumericIntMin).value()).value());
  EXPECT_EQ(
      kNumericIntMax,
      ToInteger<absl::int128>(MakeNumeric(kNumericIntMax).value()).value());
  EXPECT_EQ(absl::uint128(kNumericIntMax),
            ToInteger<absl::uint128>(
                MakeNumeric(absl::uint128(kNumericIntMax)).value())
                .value());

  // Rounding.
  EXPECT_EQ(-1, ToInteger<int>(MakeNumeric(-0.5).value()).value());
  EXPECT_EQ(0, ToInteger<int>(MakeNumeric(-0.4).value()).value());
  EXPECT_EQ(0, ToInteger<int>(MakeNumeric(0.4).value()).value());
  EXPECT_EQ(1, ToInteger<int>(MakeNumeric(0.5).value()).value());
  EXPECT_EQ(0, ToInteger<unsigned>(MakeNumeric(0.4).value()).value());
  EXPECT_EQ(1, ToInteger<unsigned>(MakeNumeric(0.5).value()).value());
  EXPECT_EQ(0, ToInteger<unsigned>(MakeNumeric(49, -2).value()).value());
  EXPECT_EQ(1, ToInteger<unsigned>(MakeNumeric(50, -2).value()).value());
}

TEST(Numeric, MakeNumericIntegerFail) {
  // Negative to unsigned.
  EXPECT_THAT(ToInteger<unsigned>(MakeNumeric(-1).value()).status(),
              HasStatus(StatusCode::kDataLoss, "-1"));

  // Beyond the 8-bit limits.
  EXPECT_THAT(ToInteger<std::int8_t>(MakeNumeric(-129).value()).status(),
              HasStatus(StatusCode::kDataLoss, "-129"));
  EXPECT_THAT(ToInteger<std::int8_t>(MakeNumeric(128).value()).status(),
              HasStatus(StatusCode::kDataLoss, "128"));
  EXPECT_THAT(ToInteger<std::uint8_t>(MakeNumeric(256).value()).status(),
              HasStatus(StatusCode::kDataLoss, "256"));

  // Beyond the 32-bit limits.
  // Beyond the 32-bit limits (requires string input on 32-bit platforms).
  EXPECT_THAT(
      ToInteger<std::int32_t>(MakeNumeric("-2147483649").value()).status(),
      HasStatus(StatusCode::kDataLoss, "-2147483649"));
  EXPECT_THAT(
      ToInteger<std::int32_t>(MakeNumeric("2147483648").value()).status(),
      HasStatus(StatusCode::kDataLoss, "2147483648"));
  EXPECT_THAT(
      ToInteger<std::uint32_t>(MakeNumeric("4294967296").value()).status(),
      HasStatus(StatusCode::kDataLoss, "4294967296"));

  // Beyond the 64-bit limits (requires string input on 64-bit platforms).
  EXPECT_THAT(
      ToInteger<std::int64_t>(MakeNumeric("-9223372036854775809").value())
          .status(),
      HasStatus(StatusCode::kDataLoss, "-9223372036854775809"));
  EXPECT_THAT(
      ToInteger<std::int64_t>(MakeNumeric("9223372036854775808").value())
          .status(),
      HasStatus(StatusCode::kDataLoss, "9223372036854775808"));
  EXPECT_THAT(
      ToInteger<std::uint64_t>(MakeNumeric("18446744073709551616").value())
          .status(),
      HasStatus(StatusCode::kDataLoss, "18446744073709551616"));

  // Beyond the NUMERIC limits using 128-bit integers.
  EXPECT_THAT(
      MakeNumeric(kNumericIntMin - 1).status(),
      HasStatus(StatusCode::kOutOfRange, "-100000000000000000000000000000"));
  EXPECT_THAT(
      MakeNumeric(kNumericIntMax + 2).status(),
      HasStatus(StatusCode::kOutOfRange, "100000000000000000000000000001"));
  EXPECT_THAT(
      MakeNumeric(absl::uint128(kNumericIntMax) + 3).status(),
      HasStatus(StatusCode::kOutOfRange, "100000000000000000000000000002"));
}

TEST(Numeric, MakeNumericIntegerScaled) {
  EXPECT_EQ(10, ToInteger<int>(MakeNumeric(1, 1).value()).value());
  EXPECT_EQ(1, ToInteger<int>(MakeNumeric(10, -1).value()).value());
  EXPECT_EQ("0.922337204",
            MakeNumeric(std::numeric_limits<std::int64_t>::max(), -19)
                .value()
                .ToString());
  EXPECT_EQ("-92233720368547758080000000000",
            MakeNumeric(std::numeric_limits<std::int64_t>::min(), 10)
                .value()
                .ToString());

  EXPECT_DOUBLE_EQ(1e-9, ToDouble(MakeNumeric(1, -9).value()));

  EXPECT_EQ(1, ToInteger<int>(MakeNumeric(1).value(), 0).value());
  EXPECT_EQ(10, ToInteger<int>(MakeNumeric(1).value(), 1).value());
  EXPECT_EQ(100, ToInteger<int>(MakeNumeric(1).value(), 2).value());
  EXPECT_EQ(1000, ToInteger<int>(MakeNumeric(1).value(), 3).value());
  EXPECT_EQ(10000, ToInteger<int>(MakeNumeric(1).value(), 4).value());
  EXPECT_EQ(100000, ToInteger<int>(MakeNumeric(1).value(), 5).value());
  EXPECT_EQ(1000000, ToInteger<int>(MakeNumeric(1).value(), 6).value());
  EXPECT_EQ(10000000, ToInteger<int>(MakeNumeric(1).value(), 7).value());

  EXPECT_EQ(345679, ToInteger<int>(MakeNumeric(3456789).value(), -1).value());
  EXPECT_EQ(34568, ToInteger<int>(MakeNumeric(3456789).value(), -2).value());
  EXPECT_EQ(3457, ToInteger<int>(MakeNumeric(3456789).value(), -3).value());
  EXPECT_EQ(346, ToInteger<int>(MakeNumeric(3456789).value(), -4).value());
  EXPECT_EQ(35, ToInteger<int>(MakeNumeric(3456789).value(), -5).value());
  EXPECT_EQ(3, ToInteger<int>(MakeNumeric(3456789).value(), -6).value());
  EXPECT_EQ(0, ToInteger<int>(MakeNumeric(3456789).value(), -7).value());

  EXPECT_EQ(1U, ToInteger<unsigned>(MakeNumeric(1U).value(), 0).value());
  EXPECT_EQ(10U, ToInteger<unsigned>(MakeNumeric(1U).value(), 1).value());
  EXPECT_EQ(100U, ToInteger<unsigned>(MakeNumeric(1U).value(), 2).value());
  EXPECT_EQ(1000U, ToInteger<unsigned>(MakeNumeric(1U).value(), 3).value());
  EXPECT_EQ(10000U, ToInteger<unsigned>(MakeNumeric(1U).value(), 4).value());
  EXPECT_EQ(100000U, ToInteger<unsigned>(MakeNumeric(1U).value(), 5).value());
  EXPECT_EQ(1000000U, ToInteger<unsigned>(MakeNumeric(1U).value(), 6).value());
  EXPECT_EQ(10000000U, ToInteger<unsigned>(MakeNumeric(1U).value(), 7).value());

  EXPECT_EQ(345679U,
            ToInteger<unsigned>(MakeNumeric(3456789U).value(), -1).value());
  EXPECT_EQ(34568U,
            ToInteger<unsigned>(MakeNumeric(3456789U).value(), -2).value());
  EXPECT_EQ(3457U,
            ToInteger<unsigned>(MakeNumeric(3456789U).value(), -3).value());
  EXPECT_EQ(346U,
            ToInteger<unsigned>(MakeNumeric(3456789U).value(), -4).value());
  EXPECT_EQ(35U,
            ToInteger<unsigned>(MakeNumeric(3456789U).value(), -5).value());
  EXPECT_EQ(3U, ToInteger<unsigned>(MakeNumeric(3456789U).value(), -6).value());
  EXPECT_EQ(0U, ToInteger<unsigned>(MakeNumeric(3456789U).value(), -7).value());

  // Demonstrate how to use scaled integers as "precise fractional" values.
  auto const n = MakeNumeric(9223372036854775807, -9).value();
  EXPECT_EQ("9223372036.854775807", n.ToString());
  EXPECT_DOUBLE_EQ(9223372036.8547764, ToDouble(n));  // precision loss
  EXPECT_EQ(9223372036854775807, ToInteger<std::int64_t>(n, 9).value());
}

TEST(Numeric, MakeNumericIntegerScaledFail) {
  // Beyond the integer-scaling limit (message is rendered with exponent).
  EXPECT_THAT(MakeNumeric(1, 29).status(),
              HasStatus(StatusCode::kOutOfRange, "1e29"));

  // Beyond the integer-scaling limit on output.
  EXPECT_THAT(ToInteger<int>(MakeNumeric(1, 1).value(), 28).status(),
              HasStatus(StatusCode::kOutOfRange, "10e28"));
  EXPECT_THAT(ToInteger<unsigned>(MakeNumeric(1U, 1).value(), 28).status(),
              HasStatus(StatusCode::kOutOfRange, "10e28"));

  // Beyond the fractional-scaling limit (value is truncated).
  EXPECT_EQ(0.0, ToDouble(MakeNumeric(1, -10).value()));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
