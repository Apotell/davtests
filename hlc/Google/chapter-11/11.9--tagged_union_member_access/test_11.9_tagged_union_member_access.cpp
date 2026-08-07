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

// Tests for 11.9--tagged_union_member_access.sv (tags: 11.9)
//   typedef union tagged { void Invalid; int Valid; } u_int;
//   u_int a, b;
//   int c;
//
//   initial begin
//     a = tagged Invalid;
//     b = tagged Valid(42);
//     c = b.Valid;
//   end
//
// This is the direct extension of tagged_union.sv: it adds a third
// statement, "c = b.Valid", reading the "Valid" member back out of "b"
// (which was legitimately assigned "tagged Valid(42)" the line before --
// this is the *valid* access; see the "_inv" sibling for the invalid one).
// Per IEEE 1800-2017 11.9, member access on a tagged union compiles to a
// HierPath, exactly the same node kind used for any other "x.y" hierarchical
// reference -- the trailing path element resolves not to a declared Net or
// Variable but to the union type's own TypespecMember.
//
// Neither "u_int a, b;" nor "int c;" carries an explicit net-type keyword,
// so per IEEE 1800-2023 6.7/6.8 all three are hldb::Variable, not hldb::Net.
//
// Checked:
//   - everything tagged_union.sv checks for the UnionTypespec/TypedefTypespec
//     shape and the first two statements holds identically here
//   - module has exactly 3 variables: "a", "b" (both "u_int") and "c" (plain
//     IntTypespec), none decl-assigned
//   - the initial block's Begin has exactly 3 statements; the third is a
//     blocking Assignment: lhs RefObj "c"; rhs HierPath name "b.Valid"
//     whose getPathElems() has exactly 2 items: RefObj "b" (resolving via
//     getActual<Variable>() to Variable "b") and RefObj "Valid" (resolving
//     via getActual<TypespecMember>() to the union's own "Valid" member --
//     not a Net/Variable/data object -- since "Valid" is a member name of
//     the type, not a declared object)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Not checked:
//   - this file carries no $display, so there is no runtime value to check
//     even in principle (see the "-sim" sibling for that).

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
#include <hldb/union_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TaggedUnionMemberAccessTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.9--tagged_union_member_access.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / variables ------------------------------------------------------

TEST_F(TaggedUnionMemberAccessTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TaggedUnionMemberAccessTest, ModuleHasThreeVariablesNoneDeclAssigned) {
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

  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::TypedefTypespec>(), nullptr);
  EXPECT_NE(b->getTypespec<hldb::RefTypespec>()->getActual<hldb::TypedefTypespec>(), nullptr);
  EXPECT_NE(c->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial block: assign both members, then read one back ---------------

TEST_F(TaggedUnionMemberAccessTest, InitialBlockHasThreeStatements) {
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

TEST_F(TaggedUnionMemberAccessTest, FirstTwoStatementsAssignInvalidAndValidTags) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const aAssign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(aAssign, nullptr);
  const hldb::TaggedPattern *const aPat = aAssign->getRhs<hldb::TaggedPattern>();
  ASSERT_NE(aPat, nullptr);
  EXPECT_EQ(aPat->getName(), "Invalid");

  const hldb::Assignment *const bAssign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(bAssign, nullptr);
  const hldb::TaggedPattern *const bPat = bAssign->getRhs<hldb::TaggedPattern>();
  ASSERT_NE(bPat, nullptr);
  EXPECT_EQ(bPat->getName(), "Valid");
  ASSERT_NE(bPat->getTag<hldb::Constant>(), nullptr);
  EXPECT_EQ(bPat->getTag<hldb::Constant>()->getDecompile(), "42");
}

TEST_F(TaggedUnionMemberAccessTest, ThirdStatementReadsBDotValidAsHierPathToTypespecMember) {
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
  EXPECT_EQ(path->getName(), "b.Valid");
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const base = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "b");
  EXPECT_NE(base->getActual<hldb::Variable>(), nullptr);

  const hldb::RefObj *const member = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "Valid");
  EXPECT_NE(member->getActual<hldb::TypespecMember>(), nullptr)
      << "the trailing path element should resolve to the union's TypespecMember, not a variable";
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(TaggedUnionMemberAccessTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(TaggedUnionMemberAccessTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
