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

// Tests for tests/Google/chapter-6/6.6.8--ic_err_syntax_int_port.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - ERR1 line 28: module m(input interconnect int p); -> a parser error (PaSyntaxError or PaReservedKeyword) is
//     reported on that line,
//     because net_port_type allows only 'interconnect implicit_data_type' (23.2.2), so a data type such as int is not
//     allowed on an interconnect port
// Not checked:
//   - Nothing else: a syntax error stops the parser, so the file holds only this one check
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_err_syntax_int_port
//  19  :description: interconnect port declared with an explicit data type
//  20  :should_fail_because: an interconnect port shall have no data type
//  21  :tags: 23.2.2
//  22  */
//  23
//  24  // Errors in this file:
//  25  //   ERR1: input interconnect int p - data type on an interconnect port
//  26  // Syntax error: kept alone in its own file so it does not hide other checks.
//  27
//  28  module m(input interconnect int p);
//  29  endmodule
//  30
//  31  module top;
//  32    wire [31:0] w;
//  33    m u(w);
//  34  endmodule

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

class IcErrSyntaxIntPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_err_syntax_int_port.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ERR1: module m(input interconnect int p);
// net_port_type allows only 'interconnect implicit_data_type' (23.2.2), so a data type such as int is not allowed on an
// interconnect port. HLC reports a grammar violation as PaSyntaxError, or as PaReservedKeyword when the offending token
// is a keyword; either one is the parser rejecting the line.
TEST_F(IcErrSyntaxIntPortTest, SyntaxErrorIsReported) {
  const bool syntax = !findEvents(EventType::PaSyntaxError, 28).empty();
  const bool keyword = !findEvents(EventType::PaReservedKeyword, 28).empty();
  EXPECT_TRUE(syntax || keyword) << "line 28 violates the interconnect grammar and must be rejected by the parser";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
