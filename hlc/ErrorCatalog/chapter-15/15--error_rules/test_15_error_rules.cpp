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

// Tests for the IEEE 1800-2023 Clause 15 error scenarios catalogued in
// docs/error_catalog.xml.
//
// Scope: this file asserts ONLY that the diagnostic each catalog row requires
// is emitted. It deliberately makes no assertion about the shape of the
// compiled model. Exactly one TEST_F per catalog row, named Row<N>_... after
// that row and carrying a "catalog row N | clause | category" comment; that
// is the link between the catalog and this file.
//
// Fixture (compiled in one run by 15--error_rules.hlc):
//   15--error_rules.sv   row 460
//
// Catalog row covered here:
//   460 | 15.4.5 | COMP_ILLEGAL_ASSIGNMENT_LHS
//
// Per project convention, no test below uses GTEST_SKIP(): the assertion
// states the diagnostic IEEE 1800-2023 requires, even though
// ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS currently has zero call
// sites anywhere in src/ (confirmed by grep) -- i.e. it is expected to be
// red today. No test here asserts the absence of a diagnostic.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter15ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "15--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 460: mailbox get()/try_get()/peek()/try_peek() ref argument must
//              be a valid LHS expression (15.4.5) --------------------------

TEST_F(Chapter15ErrorRulesTest, Row460_MailboxGetMessageArgumentMustBeAValidLhsExpression) {
  // catalog row 460 | 15.4.5 | COMP
  // The ref message argument of mailbox get()/try_get()/peek()/try_peek()
  // shall be a valid left-hand expression; "mb.get(a + b);" binds an
  // arithmetic expression, which is not an lvalue.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r460_m"), nullptr)
      << "the ref message argument of mailbox get() must be a valid left-hand expression "
         "(IEEE 1800-2023 15.4.5)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
