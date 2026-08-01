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

// Tests for other.sv (tags: 7.8.1)
//   module top ();
//     typedef struct { byte B; int I[*]; } Unpkt;
//     int arr[Unpkt];
//   endmodule
//
// HLC emits EL0535 ("Illegal implicit variable Unpkt") ? typedef unresolved
// as associative-array index type; ArrayTypespec falls back to static(1).
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: 'arr' (ArrayTypespec static=1 ? error recovery)
//   - ArrayTypespec elem type is IntTypespec
//   - module has TypedefTypespec "Unpkt" (from typedef struct definition)
//   - top has no processes
//   - top has no continuous assignments
//
// Also checked:
//   - StructTypespec internals of Unpkt: member "B" resolves to ByteTypespec,
//     member "I" resolves to a wildcard-indexed ArrayTypespec
//   - IndexTypespec is absent (null) in the error-recovery ArrayTypespec

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/byte_typespec.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>

namespace hlc {

class Other : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "other.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(Other, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- variable arr (error-recovery: static array, not associative) ----

TEST_F(Other, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(Other, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

TEST_F(Other, VariableHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(Other, ArrayTypespecIsStaticDueToErrorRecovery) {
  // int arr[Unpkt] ? HLC could not resolve Unpkt as an index type (EL0535),
  // so the ArrayTypespec falls back to static(1) instead of associative(3)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1 (error recovery)
}

TEST_F(Other, ArrayTypespecElemTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Other, ArrayTypespecIndexTypespecIsNull) {
  // int arr[Unpkt] -- Unpkt could not be resolved as an index type, so the
  // error-recovery ArrayTypespec has no index typespec at all.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getIndexTypespec(), nullptr);
}

TEST_F(Other, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

// --- typedef Unpkt ----

TEST_F(Other, ModuleHasTypedefUnpkt) {
  // typedef struct { ... } Unpkt creates a TypedefTypespec named "Unpkt"
  // accessible via module typespecs (not through the variable)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("Unpkt", top->getTypespecs());
  EXPECT_NE(td, nullptr);
}

TEST_F(Other, UnpktStructHasTwoMembers) {
  // typedef struct { byte B; int I[*]; } Unpkt -- the underlying StructTypespec
  // should have exactly 2 members: B and I.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("Unpkt", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getTypedefAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getTypedefAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u);
}

TEST_F(Other, UnpktMemberBIsByteTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("Unpkt", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::StructTypespec *const st = td->getTypedefAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  const hldb::TypespecMember *const b = st->getMembers()->at(0);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "B");
  ASSERT_NE(b->getTypespec(), nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::ByteTypespec>(), nullptr);
}

TEST_F(Other, UnpktMemberIIsWildcardArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("Unpkt", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::StructTypespec *const st = td->getTypedefAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 2u);
  const hldb::TypespecMember *const i = st->getMembers()->at(1);
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), "I");
  ASSERT_NE(i->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = i->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3 (I[*] is wildcard-indexed)
}

// --- structural completeness ----

TEST_F(Other, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
