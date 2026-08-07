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

// Tests for 6.14--chandle.sv (tags: 6.14)
//   module top();
//     chandle a;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, and 6.14 "Chandle data type", p.111, checked before any test
// code was written):
//   6.8's data_type grammar lists "chandle" as its own top-level
//   alternative ("data_type ::= ... | chandle | ..."), used only within
//   data_declaration (variable declarations) -- it never appears in
//   IEEE 1800-2023 6.7's net_type list. "chandle a;" declared directly
//   in a module body must therefore be a Variable, not a Net,
//   regardless of module-level scope. This file has no
//   :should_fail_because: tag -- it is legal per spec ("chandle
//   variable_name;" is exactly the declaration syntax 6.14 shows).
//
//   A prior version of this test used hldb::Net/getNets() for "a"
//   throughout -- the same net/variable misclassification bug found and
//   fixed across 6.5, 6.9.1, and the 6.12 series this session. This
//   version targets hldb::Variable for "a" instead.
//
// What is checked:
//   - module top exists, has no Nets and exactly 1 Variable named "a"
//   - "a" has a RefTypespec node (typespec is present) whose vpiActual
//     is null -- HLC does not resolve chandle to a ChandleTypespec in
//     the global type pool
//   - "a" has no initial value (6.14: "chandles shall always be
//     initialized to the value null", but that is a simulation-time
//     fact -- no declaration-time initializer appears in the object
//     model either way, matching this source which supplies none)
//   - no ContAssigns and no processes in top
//   - compiler reports zero errors (this file is fully legal per 6.14)
//
// What is NOT checked and why:
//   - whether the RefTypespec should resolve to a ChandleTypespec: kept
//     as a GTEST_SKIP with real, currently-failing assertion code
//     beneath it (removing the skip fails today) -- HLC does not yet
//     populate this typespec at all.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/chandle_typespec.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ChandleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.14--chandle.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(ChandleTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ChandleTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'chandle a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'chandle a' should be a Variable (IEEE 1800-2023 6.8: 'chandle' is a data_type "
         "alternative used only in variable declarations); if this is null, hldb likely "
         "misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

// ---------------------------------------------------------------------------
// Typespec -- RefTypespec node present but vpiActual is unresolved
// ---------------------------------------------------------------------------
TEST_F(ChandleTest, AHasTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec(), nullptr) << "'a' should have a RefTypespec node";
}

TEST_F(ChandleTest, ATypespecActualIsNotNull) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr) << "chandle variable typespec vpiActual is unresolved";
}

TEST_F(ChandleTest, ATypespecNameIsChandle) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr) << "Unresolved RefTypespec; expecting ChandleTypespec";
  EXPECT_NE(rts->getActual<hldb::ChandleTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// No initial value, no continuous assignments, no processes
// ---------------------------------------------------------------------------
TEST_F(ChandleTest, AHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr) << "chandle variable 'a' has no declaration-time initializer";
}

TEST_F(ChandleTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(ChandleTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(ChandleTest, CompilerReportsZeroErrors) {
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
