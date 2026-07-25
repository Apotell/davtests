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

// Tests for 11.3.6--assign_in_expr_inv.sv (tags: 11.3.6)
//   :should_fail_because: blocking assignments within expression must be
//                          enclosed in parentheses
//   module top();
//     int a;
//     int b;
//     int c;
//
//     initial begin
//       a = b = c = 5;
//     endmodule
//
// This file is the deliberate negative case for IEEE 1800-2017 11.3.6:
// "The use of assignment operators in expressions is permitted only in
// certain cases; in these cases, the assignment shall be enclosed in
// parentheses." Chained assignment written *without* the required parens
// ("a = b = c = 5;") is not legal SystemVerilog, unlike its parenthesized
// sibling "a = (b = (c = 5));" tested in 11.3.6--assign_in_expr.sv. So
// unlike most files in this batch, the interesting corner here is not what
// the AST looks like -- it is whether the parser actually rejects the
// construct instead of silently accepting it.
//
// Ground truth from the compiler log: parsing fails with two
// [SNT:PA0207] "no viable alternative" / "mismatched input" syntax errors,
// both pointing at line 23 (the "a = b = c = 5;" statement) and the
// following "initial begin" that the parser could no longer make sense of
// once the chained assignment broke the grammar. Because the parse
// aborted mid-module, the design ends up with two incomplete/duplicated
// Module stubs (no name, no body) instead of the single "work@top" module
// every other file in this chapter produces -- that malformed shape is a
// direct, checkable side effect of the syntax error, not a separate bug.
//
// Checked:
//   - the compiler reports exactly 2 syntax errors, 0 fatal, 0 semantic
//     errors, 0 warnings -- i.e. this is caught at the parser stage, which
//     is exactly where IEEE 11.3.6's parenthesization requirement is a
//     grammar-level rule, not a later semantic check
//   - as a direct, mechanical consequence of the failed parse: the design
//     ends up with 2 malformed vpiAllModules entries (both unnamed, empty
//     body) and 3 typespecs (2 ModuleTypespec each pointing back to one of
//     the malformed modules, plus 1 IntTypespec) -- confirming the parser
//     did not recover a well-formed single "work@top" module the way
//     11.3.6--assign_in_expr.sv and friends do
//
// Not checked:
//   - no simulation-only gap applies here: this file never produces a
//     valid module body to evaluate, so there is nothing further to skip.
//
// This test is intentionally NOT a GTEST_SKIP: it is a real, currently
// PASSING assertion that HLC correctly enforces IEEE 11.3.6's
// parenthesization rule (unlike some other constructs in this codebase
// where a `:should_fail_because:` tag does NOT match the compiler's actual
// (0-error) output -- this is the opposite, confirmed-correct case).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/typespec.h>

namespace hlc {

class AssignInExprInvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assign_in_expr_inv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- the point of the file: the parenthesization rule is enforced ---------

TEST_F(AssignInExprInvTest, CompilerCorrectlyRejectsUnparenthesizedChainedAssignment) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 2) << "IEEE 11.3.6 requires 'a = b = c = 5;' (no parens) to be rejected "
                                  "as a syntax error, matching this file's :should_fail_because: tag";
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- side effect of the failed parse: no well-formed module survives ------

TEST_F(AssignInExprInvTest, FailedParseLeavesNoWellFormedTopModule) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_EQ(top, nullptr) << "the syntax error prevents a well-formed 'work@top' module from "
                             "being elaborated, unlike every clean file in this chapter";
}

TEST_F(AssignInExprInvTest, DesignHasTwoMalformedModuleStubs) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u) << "the parser produced two incomplete module "
                                                       "records instead of recovering one clean module";
}

TEST_F(AssignInExprInvTest, DesignHasThreeTypespecsMatchingTheMalformedModules) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_EQ(m_design->getTypespecs()->size(), 3u);
  uint32_t moduleTypespecCount = 0;
  uint32_t intTypespecCount = 0;
  for (const hldb::Typespec *const ts : *m_design->getTypespecs()) {
    if (any_cast<hldb::ModuleTypespec>(ts) != nullptr) ++moduleTypespecCount;
    if (any_cast<hldb::IntTypespec>(ts) != nullptr) ++intTypespecCount;
  }
  EXPECT_EQ(moduleTypespecCount, 2u) << "one ModuleTypespec per malformed module stub";
  EXPECT_EQ(intTypespecCount, 1u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
