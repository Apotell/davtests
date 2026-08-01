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

// Spec-based validation of illegal real literal constant forms per IEEE 1800-2017 Sec 5.7.2.
//
// Sec 5.7.2 rule under test:
//   "Real numbers expressed with a decimal point shall have at least one digit
//    on each side of the decimal point."
//
// The following forms are explicitly listed as INVALID in Sec 5.7.2:
//   .12   -- missing digit BEFORE decimal point
//   9.    -- missing digit AFTER decimal point
//   4.E3  -- missing digit AFTER decimal point (before exponent)
//   .2e-7 -- missing digit BEFORE decimal point
//
// All four appear inside the initial block of module top. The first illegal
// form (.12) triggers a syntax error that collapses the parse: the initial
// block is never completed, and the module body is never closed.
//
// Surelog's UHDM output after the broken parse:
//   - 2 nameless stub module fragments (no name, no nets, no processes)
//   - No module named "top"
//   - No initial block and no assignments -- confirming the illegal literal
//     forms prevented any of the assignments from reaching UHDM

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class RealConstantsIllegal : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.2-real-constants-illegal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Sec 5.7.2: the four illegal forms must be rejected. No valid named module
// should reach UHDM -- if 'top' exists, Surelog silently accepted input
// that the spec forbids.
// ----
TEST_F(RealConstantsIllegal, NoModuleNamedTop) {
  EXPECT_EQ(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr)
      << "Sec 5.7.2: forms without a digit on both sides of the decimal point "
         "must be rejected -- 'top' must not exist";
}

// ----
// The broken parse leaves 2 nameless stub fragments. Exactly 2 confirms the
// parser attempted recovery but could not produce a valid module.
// ----
TEST_F(RealConstantsIllegal, TwoStubModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u) << "broken parse should produce exactly 2 nameless stub modules";
}

TEST_F(RealConstantsIllegal, StubModulesHaveNoName) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(m->getName().empty()) << "stub module should have an empty name, got: " << m->getName();
  }
}

// ----
// Sec 5.7.2: the initial block contained all four illegal literals. If no
// processes exist in any module, the initial block was never parsed --
// confirming the illegal forms stopped the parse before any assignment
// (.12, 9., 4.E3, .2e-7) could be recorded in UHDM.
// ----
TEST_F(RealConstantsIllegal, NoProcessesInStubModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty())
        << "Sec 5.7.2: initial block with illegal literals must not appear in "
           "UHDM -- stub module should have no processes";
  }
}

// ----
// No nets in any stub module -- 'logic [31:0] a' leaked to global scope
// because the module body was never completed.
// ----
TEST_F(RealConstantsIllegal, NoNetsInStubModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "stub module should have no nets";
  }
}

// ----
// Per IEEE 1800-2023 Sec 5.7.2, real literals without a digit on each side of
// the decimal point (.12, 9., 4.E3, .2e-7) are illegal and must be reported
// as syntax errors.
// ----
TEST_F(RealConstantsIllegal, Compiler_ReportsSyntaxErrors) {
  const ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_GT(stats.nbSyntax, 0) << "illegal real literals (IEEE 1800-2023 Sec 5.7.2) must be syntax errors";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
