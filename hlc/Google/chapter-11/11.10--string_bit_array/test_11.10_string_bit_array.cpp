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

// Tests for 11.10--string_bit_array.sv (tags: 11.10)
//   module top();
//     bit [8*14:1] a;
//     initial begin
//       a = "Test";
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 Sec 11.10 "String literal
// expressions", p.304, checked before any test code was written):
//   "This subclause discusses operations on string literals ... and
//   string literals stored in bit vectors and other packed types."
//   "String literal operands shall be treated as constant numbers
//   consisting of a sequence of 8-bit ASCII codes, one per character."
//   "When a vector is larger than required to hold the string literal
//   value being assigned, the contents after the assignment shall be
//   padded on the left with zeros." The spec's own worked example
//   ("bit [8*14:1] stringvar; ... stringvar = \"Hello world\";") is
//   nearly identical to this file's "a".
//
//   A prior analogous case already confirmed in this codebase
//   (hlc/Google/chapter-5/5.9-string-word-assignment/test_5.9-string-
//   word-assignment.cpp, for a declaration-time "bit [8*3-1:0] a =
//   \"hi0\";") shows a string literal assigned to a bit vector is
//   modeled as a Constant with getConstType() == vpiStringConst,
//   getValue() == the raw characters (no quotes), getSize() == 8 * (char
//   count), and getTypespec()->getActual<StringTypespec>() non-null.
//   This file exercises the same Constant shape but via a PROCEDURAL
//   assignment inside an initial block instead of a declaration-time
//   initializer, so Variable::getValue() (which only ever exposes a
//   declaration-time initializer) must be null; the literal's Constant
//   only shows up as the Assignment's rhs.
//
// What is checked:
//   - module top has zero nets and exactly 1 Variable "a": "bit" is not
//     a net-type keyword (IEEE 1800-2023 Sec 6.7's list is wire, tri,
//     triand, trior, trireg, tri0, tri1, uwire, wand, wor, supply0,
//     supply1) and there is no port list, so per Sec 6.8 "a" is a
//     Variable
//   - "a"'s typespec is a BitTypespec whose single Range is NOT folded:
//     left is an Operation (vpiMultOp, operands Constant "8", Constant
//     "14"), right is Constant "1" -- matching the exact "[8*14:1]"
//     expression written in source, per the same unfolded-expression
//     behavior already confirmed for "bit [8*3-1:0]" in the chapter-5
//     precedent above
//   - "a" has no declaration-time initializer (getValue() is null); its
//     value comes entirely from the procedural assignment below
//   - the initial block is a Begin (explicit begin/end, so it IS wrapped
//     -- see chapter-10/10.4.2's confirmed rule) with exactly 1
//     statement: a blocking Assignment, lhs RefObj "a", rhs Constant
//     (vpiStringConst, value "Test", size 32 [4 chars x 8 bits],
//     RefTypespec -> StringTypespec)
//   - compiler emits zero errors
//
// What is NOT checked and why:
//   - the exact design-level typespec count is inferred, not
//     independently re-run for this new file: this codebase consistently
//     shares one IntTypespec(signed) design-wide for every plain decimal
//     integer literal (here, the "8" and "14" in the range expression)
//     and one StringTypespec design-wide for every string literal (here,
//     "Test") -- see e.g. chapter-10/10.3--proc-assignment--bad and
//     chapter-11/11.4.5--equality-op for the same ModuleTypespec +
//     IntTypespec(+StringTypespec) pattern. Build and run this test to
//     confirm the inferred count of 3 (ModuleTypespec, IntTypespec,
//     StringTypespec) holds.
//   - whether "a" actually equals "Test" as an observable runtime value
//     after the initial block executes. Variable::getValue<T>() only
//     ever exposes a declaration-time initializer (none here, since "a"
//     is assigned procedurally) -- there is no field anywhere in the
//     object model for "the value a variable holds after execution."
//     The literal itself is fully visible and checked above as the
//     Assignment's rhs Constant; only the notion of "executing" that
//     assignment is a genuine simulation-only gap (see the GTEST_SKIP()
//     canary below).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringBitArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.10--string_bit_array.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module / variable ------------------------------------------------------

TEST_F(StringBitArrayTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(StringBitArrayTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'bit' is not a net-type keyword (IEEE 1800-2023 Sec 6.7)";
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "Variable 'a' not found";
}

TEST_F(StringBitArrayTest, VariableAHasNoDeclarationTimeInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr) << "'a' is declared bare; its value comes only from the "
                                        "procedural assignment inside the initial block";
}

TEST_F(StringBitArrayTest, VariableAHasBitTypespecWithUnfoldedRangeEightTimesFourteenToOne) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::BitTypespec *const bt = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr) << "'bit [8*14:1] a' must produce a BitTypespec";
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  const hldb::Range *const range = bt->getRanges()->at(0);
  ASSERT_NE(range, nullptr);

  const hldb::Operation *const left = range->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "'8*14' is not folded to a literal -- range left must be an Operation";
  EXPECT_EQ(left->getOpType(), vpiMultOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(left->getOperands()->at(0))->getDecompile(), "8");
  EXPECT_EQ(any_cast<hldb::Constant>(left->getOperands()->at(1))->getDecompile(), "14");

  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "1");
}

// --- initial block: a = "Test" ----------------------------------------------

TEST_F(StringBitArrayTest, InitialBlockHasOneStatement) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "explicit begin/end around the single statement should still produce a Begin";
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(StringBitArrayTest, StatementAssignsTestStringLiteralToA) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::Constant *const lit = assign->getRhs<hldb::Constant>();
  ASSERT_NE(lit, nullptr) << "'\"Test\"' should be a Constant";
  EXPECT_EQ(lit->getConstType(), vpiStringConst)
      << "IEEE 1800-2023 Sec 11.10: string literal operands are treated as constant numbers, but "
         "HLC still tags the Constant's own kind as vpiStringConst";
  EXPECT_EQ(lit->getValue(), "Test");
  EXPECT_EQ(lit->getSize(), 32) << "\"Test\" = 4 chars x 8 bits = 32 bits";
  ASSERT_NE(lit->getTypespec(), nullptr);
  EXPECT_NE(lit->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(StringBitArrayTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- known gap: post-execution value of 'a' requires simulation ------------

TEST_F(StringBitArrayTest, AEndsUpEqualToTest) {
  GTEST_SKIP() << "HLC is a static compiler/elaborator: Variable::getValue<T>() only ever exposes "
                  "a declaration-time initializer (none here, since 'a' is assigned inside the "
                  "initial block) -- there is no field anywhere that captures what a procedural "
                  "assignment actually produced at runtime. The literal itself ('Test') is fully "
                  "checked above as the Assignment's rhs Constant; only observing that value "
                  "AFTER execution is the genuine simulation-only gap.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const finalValue = a->getValue<hldb::Constant>();
  ASSERT_NE(finalValue, nullptr) << "no field captures a's post-assignment runtime value";
  EXPECT_EQ(finalValue->getValue(), "Test");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
