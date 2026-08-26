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

// Tests for the IEEE 1800-2023 Clause 16 (Assertions) error scenarios
// catalogued in sv_error_catalog_Latest.xlsx (rows 468, 476, 515).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// all three modules compile with no syntax errors. The only diagnostic is a
// CP5811 "Port b definition missing its direction" warning at 33:39, which is
// about how r515_m's header is written and has nothing to do with any of these
// three rules; it is not asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter16ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 468: deferred assertion action blocks (16.4) -----------------------

TEST_F(Chapter16ErrorRulesTest, Row468_BeginEndActionBlockOnDeferredAssertionIsRejected) {
  // catalog row 468 | 16.4 | COMP
  // For a deferred immediate assertion, "the pass and fail statements ... shall
  // each consist of a single subroutine call." A begin-end block is not a
  // subroutine call, so the else branch starting on line 13 is illegal. Note
  // this restriction is specific to the deferred form (assert #0); the same
  // block on a plain immediate assertion would be legal.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 16.4 requires the pass and fail "
                  "statements of a deferred immediate assertion to each be a single subroutine call";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ACTION_BLOCK, "r468_a1", 13, 33), nullptr)
      << "a deferred assertion action block must be a single subroutine call "
         "(IEEE 1800-2023 16.4)";
}

// --- row 476: assertion expressions are side-effect free (16.6) -------------

TEST_F(Chapter16ErrorRulesTest, Row476_SideEffectInAssertionExpressionIsRejected) {
  // catalog row 476 | 16.6 | COMP
  // "Evaluation of an expression ... shall not have any side effects, e.g.,
  // increment and decrement operators are not allowed." The sole exemption is
  // a sequence match item whose variable_lvalue is a local variable; cnt on
  // line 26 is a module-level int in the property expression itself, not a
  // match item, so the exemption does not reach it.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 16.6 forbids side effects in a "
                  "concurrent assertion expression, exempting only sequence match items whose "
                  "lvalue is a local variable";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "cnt", 26, 44), nullptr)
      << "an increment operator is a side effect and is illegal in an assertion expression "
         "(IEEE 1800-2023 16.6)";
}

// --- row 515: assertion variable declaration types (16.10) ------------------

TEST_F(Chapter16ErrorRulesTest, Row515_IllegalAssertionVariableTypeIsRejected) {
  // catalog row 515 | 16.10 | COMP
  // "The data type of an assertion variable declaration shall be specified
  // explicitly and shall be one of the types allowed within assertions as
  // defined in 16.6." 16.6 admits the integral types and their arrays; chandle
  // is not among them, so the local declaration on line 35 is illegal.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 16.10 restricts an assertion variable "
                  "declaration to the types allowed by 16.6, which excludes chandle";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DECLARED_TYPE, "h", 35, 13), nullptr)
      << "chandle is not a legal assertion variable type (IEEE 1800-2023 16.10)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
