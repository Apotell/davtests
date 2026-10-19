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

// Tests for 20.3--time.sv (tags: 20.3)
//   module top();
//   initial
//   	$display($time);
//   endmodule
//
// IEEE 1800-2023 Sec 20.3, "Simulation time system functions", Syntax 20-2:
// "time_function ::= $time | $stime | $realtime" -- $time takes no
// arguments. Sec 20.3.1: it returns the current simulation time as a 64-bit
// integer, scaled to the time unit of the calling module. Since there is no
// "begin ... end" in the source, the initial procedure's statement (Sec
// 9.2.1) is the "$display" call itself.
//
// Checked:
//   - design has module "top" with exactly 1 process, and it is an Initial
//   - the Initial's statement is a SysTaskCall named "$display" directly
//     (no Begin wrapper, since there is no "begin ... end" in the source)
//     with exactly 1 argument: a SysFuncCall named "$time"
//   - the "$time" SysFuncCall has no arguments
//   - compiler reports zero fatal, syntax and error diagnostics; this also
//     covers "endmodule", which has no HLDB node of its own
//
// Call arguments are read through NamedArgument::getHighConn(): this HLDB
// wraps every tf_call argument, positional ones included, in a NamedArgument.
//
// NOT CHECKED:
//   - runtime effects (the simulation time value returned by $time) -- HLC
//     is a compiler/elaborator with no simulation capability.
//   - vpiIsProperty on "$time" -- that is an HLDB encoding, not something
//     Sec 20.3 dictates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TimeFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.3--time.hlc"}); }
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

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::SysTaskCall>();
  }

  static const hldb::SysFuncCall *getTimeCall() {
    const hldb::SysTaskCall *const display = getDisplayCall();
    if (display == nullptr) {
      return nullptr;
    }
    return getArgument<hldb::SysFuncCall>(display->getArguments(), 0);
  }
};

// --- module / initial process ------------------------------------------------

TEST_F(TimeFunctionTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(TimeFunctionTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  EXPECT_NE(getInitialProcess(), nullptr);
}

// --- $display($time); ----------------------------------------------------------

TEST_F(TimeFunctionTest, InitialStmtIsDisplaySysTaskCall) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...);' should be the Initial's statement directly, with no Begin wrapper";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(TimeFunctionTest, DisplayCallHasOneTimeArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  EXPECT_NE(getTimeCall(), nullptr) << "'$time' should be a SysFuncCall";
}

TEST_F(TimeFunctionTest, TimeCallIsNamedCorrectly) {
  const hldb::SysFuncCall *const call = getTimeCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$time");
}

TEST_F(TimeFunctionTest, TimeCallHasNoArguments) {
  const hldb::SysFuncCall *const call = getTimeCall();
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "20.3: '$time' takes no arguments";
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(TimeFunctionTest, CompilesWithNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "'module top(); ... endmodule' should parse as one complete module";
  EXPECT_EQ(stats.nbError, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
