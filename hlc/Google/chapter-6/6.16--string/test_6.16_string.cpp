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

// Tests for 6.16--string.sv (tags: 6.16)
//   module top();
//     string a;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, checked before any test code was written):
//   6.8's data_type grammar lists "string" as its own top-level
//   alternative ("data_type ::= ... | string | ..."), used only within
//   data_declaration (variable declarations) -- it never appears in
//   IEEE 1800-2023 6.7's net_type list. "string a;" declared directly
//   in a module body must therefore be a Variable, not a Net,
//   regardless of module-level scope. This file has no
//   :should_fail_because: tag -- it is legal per spec.
//
//   A prior version of this test used hldb::Net/getNets() for "a" --
//   the same net/variable misclassification bug found and fixed across
//   6.5, 6.9.1, 6.12, and 6.14 this session. This version targets
//   hldb::Variable for "a" instead.
//
// What is checked:
//   - module top exists, has no Nets and exactly 1 Variable named "a"
//   - "a" has a StringTypespec (via RefTypespec)
//   - "a" has no initial value (plain declaration, no initializer)
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
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class StringTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16--string.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(StringTypeTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(StringTypeTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'string a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'string a' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

TEST_F(StringTypeTest, ATypespecIsString) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::StringTypespec>(), nullptr) << "'a' should have StringTypespec";
}

TEST_F(StringTypeTest, AHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr) << "string a is declared without an initializer";
}

TEST_F(StringTypeTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty()) << "module has no initial/always blocks";
}

TEST_F(StringTypeTest, CompilerReportsZeroErrors) {
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
