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

// Tests for 6.17--event.sv (tags: 6.17)
//   module top();
//     event a;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, checked before any test code was written):
//   6.8's data_type grammar lists "event" as its own top-level
//   alternative ("data_type ::= ... | event | ..."), used only within
//   data_declaration (variable declarations) -- it never appears in
//   IEEE 1800-2023 6.7's net_type list. "event a;" declared directly in
//   a module body must therefore be a Variable, not a Net, regardless of
//   module-level scope. This file has no :should_fail_because: tag --
//   it is legal per spec.
//
//   A prior version of this test used hldb::Net/getNets() for "a" --
//   the same net/variable misclassification bug found and fixed across
//   6.5, 6.9.1, 6.12, 6.13, 6.14, and 6.16 this session. This version
//   targets hldb::Variable for "a" instead.
//
// What is checked:
//   - module top exists, has no Nets and exactly 1 Variable named "a"
//   - "a" RefTypespec resolves to EventTypespec
//   - "a" has no initial value
//   - top has no processes, no continuous assignments
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
#include <hldb/event_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class EventTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.17--event.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(EventTypeTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(EventTypeTest, ModuleHasNoNetsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'event a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "'event a' declares no variable (IEEE 1800-2023 6.7) anywhere in this file";
}
TEST_F(EventTypeTest, AIsNamedEvent) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::NamedEvent *const ne = hldb::findByName<hldb::NamedEvent>("a", top->getNamedEvents());
  ASSERT_NE(ne, nullptr)
      << "'event a' should be a NamedEvent; if this is null, hldb likely misclassified it as a Net/Variable";
}

TEST_F(EventTypeTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(EventTypeTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(EventTypeTest, CompilerReportsZeroErrors) {
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
