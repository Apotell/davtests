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

// Tests for 20.15--dist_exponential.sv (tags: 20.15)
//   module top();
//   initial begin
//   	integer seed = 1234;
//   	$display("%d", $dist_exponential(seed, 100));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.15, "Probabilistic distribution functions":
// "function integer $dist_exponential (inout integer seed, input integer
// mean);" -- returns a pseudo-random number with an exponential
// distribution, updating seed by reference and taking the mean as its
// second argument.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 variable and 1 statement
//   - the variable is "seed": IntegerTypespec (signed), initial value
//     Constant unsigned int "1234"
//   - the statement is a SysTaskCall named "$display" with exactly 2
//     arguments: a Constant string "%d" and a SysFuncCall named
//     "$dist_exponential"
//   - the "$dist_exponential" SysFuncCall has exactly 2 arguments: a RefObj
//     "seed" resolving to the Variable "seed", and a Constant unsigned int
//     "100" (the mean)
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the actual pseudo-random value returned by
// $dist_exponential) cannot be observed -- HLC is a compiler/elaborator
// with no simulation capability, so no execution ever happens for this
// test to check, matching the .hlc file's own note that this is a parsing,
// not simulation, test.

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
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DistExponentialFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.15--dist_exponential.hlc"}); }
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

  static const hldb::Variable *getSeedVariable() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("seed", body->getVariables());
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }

  static const hldb::SysFuncCall *getDistExponentialCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr || display->getArguments() == nullptr || display->getArguments()->size() < 2u) {
      return nullptr;
    }
    return any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(DistExponentialFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(DistExponentialFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(DistExponentialFunctionTest, InitialBodyIsBeginWithOneVariableAndOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  EXPECT_EQ(body->getVariables()->size(), 1u) << "'integer seed = 1234;' is the only variable declaration";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- integer seed = 1234; ------------------------------------------------------

TEST_F(DistExponentialFunctionTest, SeedIsSignedIntegerVariable) {
  const hldb::Variable *const seed = getSeedVariable();
  ASSERT_NE(seed, nullptr) << "'seed' should be a Variable";
  EXPECT_EQ(seed->getName(), "seed");
  const hldb::RefTypespec *const ref = seed->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  const hldb::IntegerTypespec *const ts = ref->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(ts, nullptr) << "'integer seed' should have an IntegerTypespec";
  EXPECT_TRUE(ts->getSigned());
}

TEST_F(DistExponentialFunctionTest, SeedHasInitialValue1234) {
  const hldb::Variable *const seed = getSeedVariable();
  ASSERT_NE(seed, nullptr);
  const hldb::Constant *const init = seed->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'= 1234' should be a Constant";
  EXPECT_EQ(init->getConstType(), vpiUIntConst);
  EXPECT_EQ(init->getDecompile(), "1234");
}

// --- $display("%d", $dist_exponential(seed, 100)); ------------------------------

TEST_F(DistExponentialFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(DistExponentialFunctionTest, DisplayCallHasFormatAndDistExponentialArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr) << "'\"%d\"' should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getDecompile(), "\"%d\"");

  EXPECT_NE(getDistExponentialCall(), nullptr) << "'$dist_exponential(...)' should be a SysFuncCall";
}

TEST_F(DistExponentialFunctionTest, DistExponentialCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getDistExponentialCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$dist_exponential");
}

TEST_F(DistExponentialFunctionTest, DistExponentialCallHasSeedAndMeanArguments) {
  const hldb::SysFuncCall *const call = getDistExponentialCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u)
      << "20.15: '$dist_exponential' takes an inout seed and an input mean argument";

  const hldb::RefObj *const seedArg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(seedArg, nullptr) << "'seed' should be a RefObj";
  EXPECT_EQ(seedArg->getName(), "seed");
  EXPECT_EQ(seedArg->getActual<hldb::Variable>(), getSeedVariable());

  const hldb::Constant *const mean = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(mean, nullptr) << "'100' should be a Constant";
  EXPECT_EQ(mean->getConstType(), vpiUIntConst);
  EXPECT_EQ(mean->getDecompile(), "100");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(DistExponentialFunctionTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
