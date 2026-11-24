/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Tests for tests/Google/chapter-6/6.6.8--ic_err_syntax_three_names.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - ERR1 line 32: interconnect a, b, c; -> a parser error (PaSyntaxError or PaReservedKeyword) is
//     reported on that line,
//     because Syntax 6-1 allows net_identifier [ , net_identifier ] -- at most two names per declaration
// Not checked:
//   - Nothing else: a syntax error stops the parser, so the file holds only this one check
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_err_syntax_three_names
//  19  :description: three net identifiers in one interconnect declaration
//  20  :should_fail_because: the interconnect declaration grammar allows at most two net identifiers
//  21  :tags: 6.6.8
//  22  */
//  23
//  24  // Errors in this file:
//  25  //   ERR1: interconnect a, b, c - grammar allows only [ , net_identifier ] once
//  26  // Syntax error: kept alone in its own file so it does not hide other checks.
//  27
//  28  module leaf(inout wire p);
//  29  endmodule
//  30
//  31  module top;
//  32    interconnect a, b, c;
//  33    leaf la(a);
//  34    leaf lb(b);
//  35    leaf lc(c);
//  36  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IcErrSyntaxThreeNamesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_err_syntax_three_names.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ERR1: interconnect a, b, c;
// Syntax 6-1 allows net_identifier [ , net_identifier ] -- at most two names per declaration.
// HLC reports a grammar violation as PaSyntaxError, or as PaReservedKeyword when the offending token
// is a keyword; either one is the parser rejecting the line.
TEST_F(IcErrSyntaxThreeNamesTest, SyntaxErrorIsReported) {
  const bool syntax = !findEvents(EventType::PaSyntaxError, 32).empty();
  const bool keyword = !findEvents(EventType::PaReservedKeyword, 32).empty();
  EXPECT_TRUE(syntax || keyword) << "line 32 violates the interconnect grammar and must be rejected by the parser";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
