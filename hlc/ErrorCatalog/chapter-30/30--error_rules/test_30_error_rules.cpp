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

// Tests for the IEEE 1800-2023 Clause 30 (Specify blocks) error scenarios
// catalogued in sv_error_catalog_Latest.xlsx (row 1010).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// r1010_m compiles. The only diagnostic is a CP5810 note about q taking an
// implicit wire type, which is unrelated to the rule and not asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter30ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "30--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 1010: module path sources (30.4.1) ---------------------------------

TEST_F(Chapter30ErrorRulesTest, Row1010_InternalNetAsModulePathSourceIsRejected) {
  // catalog row 1010 | 30.4.1 | COMP
  // "The module path source shall be a net that is connected to a module input
  // port or inout port." r1010_m's path on line 15 sources from the internal
  // wire declared on line 12, which is driven by the input a but is not itself
  // a port.
  //
  // The rule turns on what the net is connected to, not on where it is
  // declared, so the check has to look at the port list rather than at the
  // specify block alone; the legal form of this design would name a directly
  // in the path.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 30.4.1 requires a module path source "
                  "to be a net connected to a module input or inout port";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SPECIFY_PATH, "internal", 15, 6), nullptr)
      << "a module path source must be an input or inout port net (IEEE 1800-2023 30.4.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
