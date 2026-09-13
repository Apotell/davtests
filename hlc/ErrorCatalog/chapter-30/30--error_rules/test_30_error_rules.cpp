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

// Tests for the IEEE 1800-2023 Clause 30 error scenarios catalogued in
// docs/error_catalog.xml (row 1013).
//
// Scope: this file asserts ONLY that the diagnostic the catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F for the catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the catalog and this file.
//
// Fixture: 30--error_rules.sv (row 1013), compiled in one run by
// 30--error_rules.hlc.
//
// COMP_ILLEGAL_EXPRESSION_CONTEXT has no call site anywhere in src/ outside
// its own ErrorDefinition.cpp registration, so the assertion below is
// expected to be red today -- that is the intended, documented state of
// this exercise, not a bug for this file to work around. No test asserts
// the absence of a diagnostic, which would lock the gap in.

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

// --- row 1013: output port is not a legal state-dependent path condition operand (30.4.4.1)

TEST_F(Chapter30ErrorRulesTest, Row1013_OutputPortIsNotALegalStateDependentPathConditionOperand) {
  // catalog row 1013 | 30.4.4.1 | COMP
  // The operands of a state-dependent path conditional expression shall be
  // only module input/inout ports (or their selects), locally defined
  // variables/nets (or their selects), and compile-time constants; 'ctrl'
  // is an output port and is not a legal operand here.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r1013_m"), nullptr)
      << "an output port cannot be a state-dependent path condition operand (IEEE 1800-2023 30.4.4.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
