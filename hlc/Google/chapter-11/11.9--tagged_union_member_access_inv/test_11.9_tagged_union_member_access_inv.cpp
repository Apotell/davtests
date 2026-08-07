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

// Tests for 11.9--tagged_union_member_access_inv.sv (tags: 11.9)
// (:should_fail_because: accessing wrong member should result in run-time
//  error; :type: simulation, not "simulation parsing")
//   typedef union tagged { void Invalid; int Valid; } u_int;
//   u_int a, b;
//   int c;
//
//   initial begin
//     a = tagged Invalid;
//     b = tagged Valid(42);
//     c = a.Valid;
//   end
//
// Per IEEE 1800-2017 11.9, reading a tagged-union member other than the one
// most recently assigned ("a" was set "tagged Invalid", then read as
// ".Valid") is a run-time error -- the tag is only known dynamically. This
// file's source annotation says so explicitly. The critical, and slightly
// surprising, structural fact this file exists to pin down: "c = a.Valid"
// (the *invalid* access) produces the exact same AST/HLDB shape as
// "c = b.Valid" (the *valid* access) in the "_member_access" sibling --
// there is no field anywhere distinguishing "this access is well-formed"
// from "this access reads a currently-inactive tag". HLC compiles it
// cleanly (0 errors), because that check is inherently a run-time concern
// this static compiler/elaborator cannot perform.
//
// Neither "u_int a, b;" nor "int c;" carries an explicit net-type keyword,
// so per IEEE 1800-2023 6.7/6.8 all three are hldb::Variable, not hldb::Net.
//
// Checked:
//   - module top has exactly 3 variables: "a", "b" ("u_int") and "c" (int),
//     none decl-assigned
//   - the initial block's Begin has exactly 3 statements: "a = tagged
//     Invalid", "b = tagged Valid(42)", and "c = a.Valid" -- the last one
//     is a blocking Assignment whose rhs is a HierPath name "a.Valid" with
//     the same shape as the valid-access case: getPathElems() has 2 items,
//     RefObj "a" (resolving to Variable "a") and RefObj "Valid" (resolving
//     to the union's TypespecMember "Valid", not a variable) -- i.e. the
//     HierPath references variable "a", which was actually tagged Invalid,
//     not "b", which was actually tagged Valid
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors (confirming the invalidity is not a
//     compile-time-detectable condition)
//
// Not checked (GTEST_SKIP, with a real reason, not just "no time"):
//   - Whether executing "c = a.Valid" actually raises the run-time error
//     the source's ":should_fail_because:" annotation calls for. Per IEEE
//     1800-2023 11.9, "an attempt to read or assign a value whose type is
//     inconsistent with the tag results in a run-time error" -- the standard
//     itself specifies this as a run-time check, not a static one. HLC is a
//     static compiler/elaborator with no notion of "which tag is currently
//     active" tracked anywhere in the object model (confirmed against
//     union_typespec.h, typespec_member.h, hier_path.h, variable.h) -- there
//     is no field to assert against for "was this read invalid". The nearest
//     currently-checkable proxy is that error reporting stays at zero,
//     which is the opposite of what should happen once runtime tag
//     checking exists; that assertion is written below so it starts
//     failing (for the right reason) the moment such checking is added.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TaggedUnionMemberAccessInvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.9--tagged_union_member_access_inv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / variables ------------------------------------------------------

TEST_F(TaggedUnionMemberAccessInvTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TaggedUnionMemberAccessInvTest, ModuleHasThreeVariablesNoneDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 3u);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(c->getValue<hldb::Constant>(), nullptr);
}

// --- initial block: assign a=Invalid, b=Valid(42), then misread a.Valid ----

TEST_F(TaggedUnionMemberAccessInvTest, InitialBlockHasThreeStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);
}

TEST_F(TaggedUnionMemberAccessInvTest, FirstTwoStatementsTagAInvalidAndBValid) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const aAssign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(aAssign, nullptr);
  EXPECT_EQ(aAssign->getLhs<hldb::RefObj>()->getName(), "a");
  EXPECT_EQ(aAssign->getRhs<hldb::TaggedPattern>()->getName(), "Invalid");

  const hldb::Assignment *const bAssign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(bAssign, nullptr);
  EXPECT_EQ(bAssign->getLhs<hldb::RefObj>()->getName(), "b");
  EXPECT_EQ(bAssign->getRhs<hldb::TaggedPattern>()->getName(), "Valid");
}

TEST_F(TaggedUnionMemberAccessInvTest, ThirdStatementReadsAdotValidEvenThoughAIsTaggedInvalid) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "c");

  const hldb::HierPath *const path = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->getName(), "a.Valid") << "the HierPath references variable 'a', which was tagged Invalid, not 'b'";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const base = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "a");
  EXPECT_NE(base->getActual<hldb::Variable>(), nullptr);

  const hldb::RefObj *const member = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "Valid");
  EXPECT_NE(member->getActual<hldb::TypespecMember>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(TaggedUnionMemberAccessInvTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(TaggedUnionMemberAccessInvTest, CompilerReportsZeroErrorsBecauseTheInvalidityIsRuntimeOnly) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: the "should_fail_because" runtime error -

TEST_F(TaggedUnionMemberAccessInvTest, ReadingInactiveTagShouldRaiseARuntimeError) {
  GTEST_SKIP() << "The source is annotated ':should_fail_because: accessing "
                  "wrong member should result in run-time error' -- 'a' was "
                  "tagged Invalid, so reading 'a.Valid' reads an inactive "
                  "tag, which IEEE 1800-2017 11.9 makes a run-time error. "
                  "HLC is a static compiler/elaborator with no 'currently "
                  "active tag' field tracked anywhere in the object model "
                  "(confirmed against variable.h, tagged_pattern.h, "
                  "hier_path.h) -- there is nothing to assert against for "
                  "'this read was invalid' today. If runtime tag-checking is ever added, "
                  "compiling and running this program should report at "
                  "least one runtime error; the assertion below documents "
                  "that expectation and fails now because no such checking "
                  "exists yet.";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbError, 0) << "once runtime tag-checking exists, reading 'a.Valid' should be reported as an error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
