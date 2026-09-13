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

// Tests for the IEEE 1800-2023 Clause 29 error scenarios catalogued in
// docs/error_catalog.xml (row 1005).
//
// Scope: this file asserts ONLY that the diagnostic the catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F for the catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the catalog and this file.
//
// Fixture: 29--error_rules.sv (row 1005), compiled in one run by
// 29--error_rules.hlc.
//
// COMP_ILLEGAL_ASSIGNMENT_LHS has no call site anywhere in src/ outside its
// own ErrorDefinition.cpp registration, so the assertion below is expected
// to be red today -- that is the intended, documented state of this
// exercise, not a bug for this file to work around. No test asserts the
// absence of a diagnostic, which would lock the gap in.

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

// --- row 1005: UDP initial statement must assign to the output port reg (29.7)

TEST_F(Chapter29ErrorRulesTest, Row1005_UdpInitialStatementMustAssignToOutputPortReg) {
  // catalog row 1005 | 29.7 | COMP
  // The procedural assignment in a UDP initial statement shall assign to a
  // reg whose identifier matches the identifier of the output port; here
  // the assignment targets 'tmp', not the output port reg 'q'.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "tmp"), nullptr)
      << "a UDP initial statement must assign to the output port reg, not any other reg (IEEE 1800-2023 29.7)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
