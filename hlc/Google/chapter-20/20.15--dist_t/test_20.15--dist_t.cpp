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

// Tests for 20.15--dist_t.sv (tags: 20.15)
//   module top();
//   initial begin
//   	integer seed = 1234;
//   	$display("%d", $dist_t(seed, 3));
//   end
//   endmodule
//
// IEEE 1800-2023 Sec 20.15, "Probabilistic distribution functions":
// "function integer $dist_t (inout integer seed, input integer df);" --
// returns a pseudo-random number with a t distribution, updating seed by
// reference and taking the degree of freedom (df) as its second argument.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from the explicit "begin ... end")
//     wrapping exactly 1 variable and 1 statement
//   - the variable is "seed": IntegerTypespec (signed, Sec 6.11), initial
//     value Constant "1234"
//   - the statement is a SysTaskCall named "$display" with exactly 2
//     arguments: a string Constant "%d" and a SysFuncCall named "$dist_t"
//   - the "$dist_t" SysFuncCall has exactly 2 arguments: a RefObj "seed"
//     resolving to the Variable "seed" (the inout seed), and a Constant "3"
//     (the degree of freedom)
//   - compiler reports zero fatal, syntax and error diagnostics
//
// Call arguments are read through NamedArgument::getHighConn(): this HLDB
// wraps every tf_call argument, positional ones included, in a NamedArgument.
//
// NOT CHECKED:
//   - runtime effects (the pseudo-random value returned by $dist_t and the
//     updated seed) -- HLC is a compiler/elaborator with no simulation
//     capability; the .sv header tags this test as "simulation parsing".
//   - the vpiConstType of the unsized literals 1234 and 3 -- that is a
//     compiler encoding, not something Sec 20.15 dictates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DistTFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.15--dist_t.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every tf_call argument is wrapped in a NamedArgument; the actual
  // argument expression is its high conn.
  template <typename T>
  static const T *getArgument(const hldb::NamedArgumentCollection *args, size_t index) {
    if (args == nullptr || args->size() <= index) {
      return nullptr;
    }
    const hldb::NamedArgument *const arg = args->at(index);
    if (arg == nullptr) {
      return nullptr;
    }
    return arg->getHighConn<T>();
  }

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

  static const hldb::SysFuncCall *getDistTCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr) {
      return nullptr;
    }
    return getArgument<hldb::SysFuncCall>(display->getArguments(), 1);
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(DistTFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(DistTFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

TEST_F(DistTFunctionTest, InitialBodyIsBeginWithOneVariableAndOneStmt) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' should wrap the body in a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  EXPECT_EQ(body->getVariables()->size(), 1u) << "'integer seed = 1234;' is the only variable declaration";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "'$display(...);' is the only statement";
}

// --- integer seed = 1234; ------------------------------------------------------

TEST_F(DistTFunctionTest, SeedIsSignedIntegerVariable) {
  const hldb::Variable *const seed = getSeedVariable();
  ASSERT_NE(seed, nullptr) << "'seed' should be a Variable";
  EXPECT_EQ(seed->getName(), "seed");
  const hldb::RefTypespec *const ref = seed->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr);
  const hldb::IntegerTypespec *const ts = ref->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(ts, nullptr) << "'integer seed' should have an IntegerTypespec";
  EXPECT_TRUE(ts->getSigned()) << "6.11: 'integer' is a signed type";
}

TEST_F(DistTFunctionTest, SeedHasInitialValue1234) {
  const hldb::Variable *const seed = getSeedVariable();
  ASSERT_NE(seed, nullptr);
  const hldb::Constant *const init = seed->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'= 1234' should be a Constant";
  EXPECT_EQ(init->getDecompile(), "1234");
}

// --- $display("%d", $dist_t(seed, 3)); -----------------------------------------

TEST_F(DistTFunctionTest, StmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(DistTFunctionTest, DisplayCallHasFormatAndDistTArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = getArgument<hldb::Constant>(call->getArguments(), 0);
  ASSERT_NE(fmt, nullptr) << "'\"%d\"' should be a Constant";
  EXPECT_EQ(fmt->getConstType(), vpiStringConst) << "'\"%d\"' is a string literal";
  EXPECT_EQ(fmt->getDecompile(), "\"%d\"");

  EXPECT_NE(getDistTCall(), nullptr) << "'$dist_t(...)' should be a SysFuncCall";
}

TEST_F(DistTFunctionTest, DistTCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getDistTCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$dist_t");
}

TEST_F(DistTFunctionTest, DistTCallHasSeedAndDfArguments) {
  const hldb::SysFuncCall *const call = getDistTCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "20.15: '$dist_t' takes an inout seed and an input df argument";

  const hldb::RefObj *const seedArg = getArgument<hldb::RefObj>(call->getArguments(), 0);
  ASSERT_NE(seedArg, nullptr) << "'seed' should be a RefObj";
  EXPECT_EQ(seedArg->getName(), "seed");
  EXPECT_EQ(seedArg->getActual<hldb::Variable>(), getSeedVariable())
      << "'seed' argument should bind to the 'integer seed' declared in the begin block";

  const hldb::Constant *const df = getArgument<hldb::Constant>(call->getArguments(), 1);
  ASSERT_NE(df, nullptr) << "'3' should be a Constant";
  EXPECT_EQ(df->getDecompile(), "3");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(DistTFunctionTest, CompilesWithNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
