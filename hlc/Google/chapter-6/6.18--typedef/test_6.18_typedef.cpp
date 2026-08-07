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

// Tests for 6.18--typedef.sv (tags: 6.18)
//   module top();
//     typedef logic logic_t;
//     logic_t a;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, checked before any test code was written):
//   6.8's data_type grammar lists "[class_scope | package_scope]
//   type_identifier {packed_dimension}" as its own alternative, alongside
//   integer_vector_type ("logic" itself). A user-defined type_identifier
//   like "logic_t" used to declare "a" is exactly this alternative --
//   still part of data_declaration (variable declarations), never a net
//   declaration. "logic_t" is ultimately an alias for "logic" (itself a
//   variable-type keyword, never in IEEE 1800-2023 6.7's net_type list),
//   so "logic_t a;" must be a Variable, not a Net, regardless of
//   module-level scope. This file has no :should_fail_because: tag --
//   it is legal per spec.
//
//   A prior version of this test used hldb::Net/getNets() for "a" --
//   the same net/variable misclassification bug found and fixed across
//   6.5, 6.9.1, 6.12, 6.13, 6.14, 6.16, and 6.17 this session, here
//   extended through a typedef indirection. This version targets
//   hldb::Variable for "a" instead.
//
// What is checked:
//   - module top exists, has no Nets and exactly 1 Variable named "a"
//   - "a"'s RefTypespec name is "logic_t" and resolves to TypedefTypespec
//   - module owns a TypedefTypespec named "logic_t" whose alias resolves
//     to LogicTypespec
//   - "a" has no initial value
//   - top has no processes
//   - compiler reports zero errors (this file is fully legal per 6.8)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class TypedefTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.18--typedef.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(TypedefTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TypedefTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'logic_t a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'logic_t a' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

// ---------------------------------------------------------------------------
// Variable 'a' -- typespec is a RefTypespec named "logic_t" -> TypedefTypespec
// ---------------------------------------------------------------------------
TEST_F(TypedefTest, ATypespecNameIsLogicT) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getName(), "logic_t");
}

TEST_F(TypedefTest, ATypespecActualIsTypedef) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr) << "'a' resolves to TypedefTypespec";
}

// ---------------------------------------------------------------------------
// Module typespec collection -- contains TypedefTypespec named "logic_t"
// ---------------------------------------------------------------------------
TEST_F(TypedefTest, ModuleHasTypedefTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("logic_t", top->getTypespecs());
  ASSERT_NE(td, nullptr) << "module should own a TypedefTypespec named 'logic_t'";
  EXPECT_EQ(td->getName(), "logic_t");
}

TEST_F(TypedefTest, TypedefAliasResolvesToLogic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("logic_t", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<hldb::LogicTypespec>(), nullptr)
      << "typedef logic_t alias should resolve to LogicTypespec";
}

TEST_F(TypedefTest, AHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr);
}

TEST_F(TypedefTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(TypedefTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
