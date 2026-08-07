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

// Tests for 11.9--tagged_union.sv (tags: 11.9)
//   typedef union tagged {
//     void Invalid;
//     int Valid;
//   } u_int;
//
//   u_int a, b;
//
//   initial begin
//     a = tagged Invalid;
//     b = tagged Valid(42);
//   end
//
// Per IEEE 1800-2017 11.9, a "tagged union" pairs each member with an
// explicit tag, and a "tagged Member(expr)" / "tagged Member" assignment
// constructs a value carrying exactly one active tag. HLC represents:
//   - the type itself as a UnionTypespec with vpiTagged == true and a
//     TypespecMemberCollection (one TypespecMember per alternative)
//   - the "u_int" typedef as a TypedefTypespec whose vpiTypedefAlias
//     resolves back to that UnionTypespec
//   - each "tagged X" / "tagged X(expr)" construct as a TaggedPattern node
//     (not an Operation or a plain Constant): the void member carries no
//     vpiTag at all, while the int member's vpiTag is the argument
//     expression
//
// Checked:
//   - module top has module-level typespecs (2): a UnionTypespec
//     (vpiTagged == true) with exactly 2 TypespecMembers -- "Invalid"
//     (whose getTypespec() is a RefTypespec named "void" that resolves via
//     getActual<VoidTypespec>(), a real, distinct typespec kind -- not
//     "no backing typespec") and "Valid" (whose
//     getTypespec()->getActual<IntTypespec>() is non-null) -- and a
//     TypedefTypespec "u_int" whose
//     getTypedefAlias()->getActual<UnionTypespec>() resolves back to that
//     same union
//   - "u_int a, b;" has no explicit net-type keyword, so per IEEE 1800-2023
//     6.7/6.8 "a" and "b" are hldb::Variable, not hldb::Net
//   - module has exactly 2 variables: "a" and "b", both typed via a
//     RefTypespec named "u_int" whose getActual<TypedefTypespec>() resolves
//     to the "u_int" TypedefTypespec above, neither decl-assigned
//   - module has exactly 1 process: an Initial whose Begin has exactly 2
//     statements:
//       1) blocking Assignment: lhs RefObj "a"; rhs TaggedPattern name
//          "Invalid" whose getTag() is null (void members carry no tag
//          expression at all)
//       2) blocking Assignment: lhs RefObj "b"; rhs TaggedPattern name
//          "Valid" whose getTag<Constant>() decompiles to "42"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Not checked:
//   - this file carries no $display, so there is no runtime value to check
//     even in principle (see the "-sim" member-access sibling for that).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/union_typespec.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TaggedUnionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.9--tagged_union.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::UnionTypespec *getUnion() {
    const hldb::Module *const top = getTop();
    if ((top == nullptr) || (top->getTypespecs() == nullptr)) return nullptr;
    for (const hldb::Typespec *const ts : *top->getTypespecs()) {
      const hldb::UnionTypespec *const u = any_cast<hldb::UnionTypespec>(ts);
      if (u != nullptr) return u;
    }
    return nullptr;
  }
};

// --- module-level typespecs: the tagged union type itself -------------------

TEST_F(TaggedUnionTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TaggedUnionTest, UnionTypespecIsTaggedWithInvalidAndValidMembers) {
  const hldb::UnionTypespec *const u = getUnion();
  ASSERT_NE(u, nullptr);
  EXPECT_TRUE(u->getTagged());
  ASSERT_NE(u->getMembers(), nullptr);
  ASSERT_EQ(u->getMembers()->size(), 2u);

  const hldb::TypespecMember *const invalid = u->getMembers()->at(0);
  const hldb::TypespecMember *const valid = u->getMembers()->at(1);
  ASSERT_NE(invalid, nullptr);
  ASSERT_NE(valid, nullptr);
  EXPECT_EQ(invalid->getName(), "Invalid");
  EXPECT_EQ(valid->getName(), "Valid");

  ASSERT_NE(invalid->getTypespec(), nullptr);
  EXPECT_NE(invalid->getTypespec()->getActual<hldb::VoidTypespec>(), nullptr)
      << "the void member's typespec ref should  resolve to VoidTypespec";

  ASSERT_NE(valid->getTypespec(), nullptr);
  EXPECT_NE(valid->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(TaggedUnionTest, TypedefUIntAliasesTheUnion) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *typedefTs = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    typedefTs = any_cast<hldb::TypedefTypespec>(ts);
    if (typedefTs != nullptr) break;
  }
  ASSERT_NE(typedefTs, nullptr);
  EXPECT_EQ(typedefTs->getName(), "u_int");
  ASSERT_NE(typedefTs->getTypedefAlias(), nullptr);
  EXPECT_EQ(typedefTs->getTypedefAlias()->getActual<hldb::UnionTypespec>(), getUnion());
}

// --- variables ---------------------------------------------------------------

TEST_F(TaggedUnionTest, ModuleHasTwoUIntVariablesNeitherDeclAssigned) {
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

  ASSERT_NE(a->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_EQ(a->getTypespec<hldb::RefTypespec>()->getName(), "u_int");
  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::TypedefTypespec>(), nullptr);
  ASSERT_NE(b->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(b->getTypespec<hldb::RefTypespec>()->getActual<hldb::TypedefTypespec>(), nullptr);
}

// --- initial block: tagged Invalid / tagged Valid(42) -----------------------

TEST_F(TaggedUnionTest, InitialBlockHasTwoBlockingAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(TaggedUnionTest, FirstStatementAssignsTaggedInvalidWithNoTagValue) {
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
  EXPECT_EQ(pat->getName(), "Invalid");
  EXPECT_EQ(pat->getTag(), nullptr) << "the void member carries no tag expression";
}

TEST_F(TaggedUnionTest, SecondStatementAssignsTaggedValidWithTagFortyTwo) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::TaggedPattern *const pat = assign->getRhs<hldb::TaggedPattern>();
  ASSERT_NE(pat, nullptr);
  EXPECT_EQ(pat->getName(), "Valid");
  ASSERT_NE(pat->getTag<hldb::Constant>(), nullptr);
  EXPECT_EQ(pat->getTag<hldb::Constant>()->getDecompile(), "42");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(TaggedUnionTest, DesignHasThreTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(TaggedUnionTest, CompilerReportsZeroErrors) {
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
