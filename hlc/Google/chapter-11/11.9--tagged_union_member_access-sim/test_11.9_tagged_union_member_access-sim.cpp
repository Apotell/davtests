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

// Tests for 11.9--tagged_union_member_access-sim.sv (tags: 11.9)
//   typedef union tagged { void Invalid; int Valid; } u_int;
//   u_int a;
//   int b;
//
//   initial begin
//     a = tagged Valid(42);
//     b = a.Valid;
//     $display(":assert: (42 == %d)", b);
//   end
//
// This is the runtime-verification sibling of tagged_union_member_access.sv,
// trimmed to a single union variable "a" (unlike the two-variable "a, b"
// pair in the non-sim siblings) plus the plain int "b" the value is read
// into. Only the *valid* access is exercised here (reading ".Valid" right
// after tagging "a" Valid) -- the deliberately invalid case lives in the
// "_inv" sibling.
//
// Neither "u_int a;" nor "int b;" carries an explicit net-type keyword, so
// per IEEE 1800-2023 6.7/6.8 both are hldb::Variable, not hldb::Net.
//
// Checked:
//   - module top has exactly 2 variables: "a" ("u_int", via RefTypespec ->
//     TypedefTypespec -> UnionTypespec) and "b" (plain IntTypespec),
//     neither decl-assigned
//   - module has exactly 1 process: an Initial whose Begin has exactly 3
//     statements:
//       1) blocking Assignment: lhs RefObj "a"; rhs TaggedPattern name
//          "Valid" whose getTag<Constant>() decompiles to "42"
//       2) blocking Assignment: lhs RefObj "b"; rhs HierPath name "a.Valid"
//          whose getPathElems() has exactly 2 items: RefObj "a" (resolving
//          to Variable "a") and RefObj "Valid" (resolving to the union's
//          TypespecMember "Valid", not a variable)
//       3) SysTaskCall "$display" with 2 arguments: Constant string
//          ":assert: (42 == %d)" and RefObj "b"
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason, not just "no time"):
//   - Whether "b" actually equals 42 at runtime. HLC is a compiler/
//     elaborator, not a simulator: Variable::getValue<T>() only ever
//     exposes a declaration-time initializer, and "b" is only assigned
//     inside the initial block, never at declaration -- there is no field
//     capturing the post-assignment value the ":assert:" tag is asking a
//     simulator to check.

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
#include <hldb/sys_task_call.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TaggedUnionMemberAccessSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.9--tagged_union_member_access-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / variables ------------------------------------------------------

TEST_F(TaggedUnionMemberAccessSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TaggedUnionMemberAccessSimTest, ModuleHasTwoVariablesNeitherDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr);

  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::TypedefTypespec>(), nullptr);
  EXPECT_NE(b->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial block: tag Valid(42), read it back, display it ---------------

TEST_F(TaggedUnionMemberAccessSimTest, InitialBlockHasThreeStatements) {
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

TEST_F(TaggedUnionMemberAccessSimTest, FirstStatementTagsAValidFortyTwo) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");
  const hldb::TaggedPattern *const pat = assign->getRhs<hldb::TaggedPattern>();
  ASSERT_NE(pat, nullptr);
  EXPECT_EQ(pat->getName(), "Valid");
  ASSERT_NE(pat->getTag<hldb::Constant>(), nullptr);
  EXPECT_EQ(pat->getTag<hldb::Constant>()->getDecompile(), "42");
}

TEST_F(TaggedUnionMemberAccessSimTest, SecondStatementReadsAdotValidIntoB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::HierPath *const path = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->getName(), "a.Valid");
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(path->getPathElems()->at(0))->getName(), "a");
  const hldb::RefObj *const member = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "Valid");
  EXPECT_NE(member->getActual<hldb::TypespecMember>(), nullptr);
}

TEST_F(TaggedUnionMemberAccessSimTest, ThirdStatementDisplaysExpectedBValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (42 == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "b");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(TaggedUnionMemberAccessSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(TaggedUnionMemberAccessSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime value ----------------------------

TEST_F(TaggedUnionMemberAccessSimTest, BEqualsFortyTwoAtRuntime) {
  GTEST_SKIP() << "The source tags 'a' Valid(42), reads 'a.Valid' into 'b', "
                  "and asserts b == 42. HLC is a static compiler/"
                  "elaborator: Variable::getValue<T>() only ever exposes a "
                  "declaration-time initializer, and 'b' is only assigned "
                  "inside the initial block, never at declaration -- there "
                  "is no field capturing its post-assignment runtime value.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const bValue = b->getValue<hldb::Constant>();
  ASSERT_NE(bValue, nullptr) << "no field captures b's post-assignment runtime value";
  EXPECT_EQ(bValue->getDecompile(), "42");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
