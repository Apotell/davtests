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

// Tests for the IEEE 1800-2023 Clause 29 (User-defined primitives) error
// scenarios catalogued in sv_error_catalog_Latest.xlsx (rows 991, 995).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// both UDPs are compiled. Row 991's reversed port order draws no complaint at
// all. Row 995's two-edge table row is rejected by the grammar with a single
// syntax error at 28:9; a control fixture whose sequential table rows each
// carry exactly one edge parses cleanly, which is what makes that enforcement
// of the rule rather than a missing production for edge specifiers.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter29ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "29--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 991: the output port comes first (29.3.1) --------------------------

TEST_F(Chapter29ErrorRulesTest, Row991_OutputPortMustBeFirstInTheUdpPortList) {
  // catalog row 991 | 29.3.1 | COMP
  // "The output port shall be the first port in the port list." r991_p lists
  // the input i first on line 9; the directions are only declared afterwards,
  // which is why this cannot be a grammar rule -- the parser has no way to
  // know which port is the output while reading the list.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_UDP_DECLARATION, "r991_p", 9, 11), nullptr)
      << "the output port must come first in a UDP port list (IEEE 1800-2023 29.3.1)";
}

// --- row 995: one transition per table row (29.3.4) -------------------------

TEST_F(Chapter29ErrorRulesTest, Row995_TwoEdgesInOneUdpTableRowAreRejected) {
  // catalog row 995 | 29.3.4 | COMP
  // "Each table entry can have a transition specified for, at most, one
  // input." The row on line 28 specifies (01) on a and (10) on b at once.
  //
  // HLC enforces this in the grammar: the sequential table entry production
  // admits at most one edge_indicator, so the second one is a syntax error.
  // Verified against a control fixture whose rows each carry exactly one edge
  // -- it parses cleanly -- so this is the rule being enforced, not edge
  // specifiers being unsupported.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 28, 9), nullptr)
      << "a UDP table row may specify a transition on at most one input (IEEE 1800-2023 29.3.4)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
