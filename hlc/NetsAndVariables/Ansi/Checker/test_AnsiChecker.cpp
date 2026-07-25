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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Checker.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so the file-scope
// checker testing point stands on its own.
//
// Checked:
//   - work@nets_and_variables_checker exists with two input ports (clk, a)
//   - its logic variables (ck_logic, ck_vec) exist
//   - it has a single process, modeled as an Always process

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/checker_decl.h>
#include <hldb/checker_port.h>
#include <hldb/design.h>
#include <hldb/process_stmt.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiCheckerTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AnsiChecker.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AnsiCheckerTest, CheckerExists) {
  ASSERT_NE(m_design->getCheckerDecls(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls()),
            nullptr);
}

TEST_F(AnsiCheckerTest, CheckerHasTwoInputPorts) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getPorts(), nullptr);
  ASSERT_EQ(checker->getPorts()->size(), 2u);

  const hldb::CheckerPort *const clk = hldb::findByName<hldb::CheckerPort>("clk", checker->getPorts());
  const hldb::CheckerPort *const a = hldb::findByName<hldb::CheckerPort>("a", checker->getPorts());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(a->getDirection(), vpiInput);
}

TEST_F(AnsiCheckerTest, CheckerHasLogicVariables) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_logic", checker->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ck_vec", checker->getVariables()), nullptr);
}

TEST_F(AnsiCheckerTest, CheckerHasOneProcess) {
  const hldb::CheckerDecl *const checker =
      hldb::findByName<hldb::CheckerDecl>("work@nets_and_variables_checker", m_design->getCheckerDecls());
  ASSERT_NE(checker, nullptr);
  ASSERT_NE(checker->getProcesses(), nullptr);
  ASSERT_EQ(checker->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Always>(checker->getProcesses()->at(0)), nullptr)
      << "'always_ff @(posedge clk)' should be modeled as an Always process";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
