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

// Tests for 20.15--random.sv (tags: 20.15)
//   module top();
//   initial begin
//   	$display("%d", $random);
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.15, "Probabilistic distribution functions":
// "function integer $random (inout integer seed);" -- when called without
// a seed argument and without parentheses, $random is used as a property
// (vpiIsProperty) rather than a function call with an argument list, and
// simply returns a pseudo-random number using the default seed.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     declaring no variables and wrapping exactly 1 statement
//   - the statement is a SysTaskCall named "$display" with exactly 2
//     arguments: a Constant string "%d" and a SysFuncCall named "$random"
//   - the "$random" SysFuncCall has no argument list (called without
//     parentheses or a seed) and is flagged as a property (vpiIsProperty)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the actual pseudo-random value returned by
// $random) cannot be observed -- HLC is a compiler/elaborator with no
// simulation capability, so no execution ever happens for this test to
// check, matching the .hlc file's own note that this is a parsing, not
// simulation, test.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class RandomFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.15--random.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }

  static const hldb::SysFuncCall *getRandomCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(RandomFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(RandomFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(RandomFunctionTest, InitialBodyIsBeginWithNoVariablesAndOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  EXPECT_EQ(body->getVariables(), nullptr) << "no variable is declared in this body";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- $display("%d", $random); -----------------------------------------------------

TEST_F(RandomFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(RandomFunctionTest, DisplayCallHasFormatAndRandomArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr) << "'\"%d\"' should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getDecompile(), "\"%d\"");

  EXPECT_NE(getRandomCall(), nullptr) << "'$random' should be a SysFuncCall";
}

TEST_F(RandomFunctionTest, RandomCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getRandomCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$random");
}

TEST_F(RandomFunctionTest, RandomCallHasNoArgumentsAndIsProperty) {
  const hldb::SysFuncCall *const call = getRandomCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getArguments(), nullptr) << "'$random' called without parentheses or a seed argument";
  EXPECT_TRUE(call->getIsProperty()) << "20.15: a parenthesis-less '$random' is modeled as a property";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(RandomFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
