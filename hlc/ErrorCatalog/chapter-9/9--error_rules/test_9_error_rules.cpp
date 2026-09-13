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

// Tests for the IEEE 1800-2023 Clause 9 error scenarios catalogued in
// docs/error_catalog.xml (row 275).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model -- no typespecs, no net-vs-variable, no recovery
// behaviour. Exactly one TEST_F per catalog row, named Row<N>_... after that
// row and carrying a "catalog row N | clause | category" comment; that is
// the link between the workbook and this file.
//
// Fixture (compiled in one run by 9--error_rules.hlc):
//   9--error_rules.sv   row 275
//
// This chapter's dut source file (dut_ch_9_3_2.sv) has a single test case,
// so no sibling _inv*.sv fixtures are needed here.
//
// COMP_ILLEGAL_REF_IN_FORK is registered in ErrorDefinition.cpp (code 5923,
// message "Illegal reference to a by-reference argument in fork: %s") but,
// as of this writing, has no other call site anywhere in src/ -- nothing in
// Phase2ModelBuilder or ObjectBinder currently raises it, so the assertion
// below is expected to fail (red) today. Per project convention the
// assertion is still written to what IEEE 1800-2023 9.3.2 actually requires,
// not to what HLC currently outputs: no GTEST_SKIP(), and no assertion of
// the diagnostic's absence, which would lock the gap in.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter9ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 275: forbidden reference to a by-reference formal inside fork-join_none (9.3.2) ---

TEST_F(Chapter9ErrorRulesTest, Row275_ReferenceToByRefFormalInsideForkJoinNoneIsRejected) {
  // catalog row 275 | 9.3.2 | COMP
  // Within a fork-join_any or fork-join_none block it shall be illegal to
  // refer to formal arguments passed by reference, other than in the
  // initialization value expressions of variables declared in a
  // block_item_declaration of the fork, unless the argument is declared ref
  // static. 'r' is a plain 'ref int' formal (not 'ref static'), and its
  // assignment on line 23 of 9--error_rules.sv sits directly inside a
  // join_none block, not in an initializer of a block-local declaration.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_REF_IN_FORK), nullptr)
      << "a plain 'ref' formal cannot be referenced inside fork-join_any/"
         "join_none except in a block-local initializer (IEEE 1800-2023 9.3.2)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
