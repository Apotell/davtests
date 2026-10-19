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

// Tests for 20.4--printtimescale-hier.sv (tags: 20.4)
//   `timescale 1 ms / 1 us
//   module top();
//   initial
//   	$printtimescale(mod0.m);
//   endmodule
//
//   `timescale 1 us / 1 ns
//   module mod0();
//   	mod1 m();
//   endmodule
//
//   `timescale 1 ns / 1 ps
//   module mod1();
//   initial
//   	$display("mod1");
//   endmodule
//
// IEEE 1800-2023 Sec 20.4.1, "$printtimescale": "$printtimescale [ (
// hierarchical_identifier ) ] ;" -- with an argument, the time unit and
// precision of the named module instance are displayed.
//
// Sec 23.3.1: modules that are never instantiated are top-level modules.
// "top" and "mod0" are not instantiated anywhere, so both are top-level;
// "mod1" is instantiated by "mod0" ("mod1 m();"), so it is not. Sec 23.6:
// a hierarchical name may start at a top-level module, so "mod0.m" names
// the instance "m" (of "mod1") inside the top-level "mod0".
//
// Sec 22.7: each "`timescale" directive applies to the modules that follow
// it. Sec 37 (VPI) encodes vpiTimeUnit / vpiTimePrecision as the
// power-of-10 exponent of the value in seconds:
//   top:  1 ms / 1 us -> -3 / -6
//   mod0: 1 us / 1 ns -> -6 / -9
//   mod1: 1 ns / 1 ps -> -9 / -12
//
// Checked:
//   - design has modules "top", "mod0", "mod1"
//   - "top" and "mod0" are top-level modules; "mod1" is not
//   - each module's time unit / precision matches its own "`timescale"
//   - top has exactly 1 process, an Initial whose statement is a
//     SysTaskCall named "$printtimescale" directly (no Begin wrapper), with
//     exactly 1 argument: a RefObj named "mod0.m"
//   - "mod0.m" has 2 path elements, "mod0" binding to Module "mod0" and "m"
//     binding to mod0's instance "m"; "mod0.m" itself binds to that same
//     instance
//   - mod0 has exactly 1 instance, named "m", of module "mod1"
//   - mod1 has exactly 1 process, an Initial whose statement is a
//     SysTaskCall named "$display" directly (no Begin wrapper), with
//     exactly 1 argument: a string Constant "mod1"
//   - compiler reports zero fatal, syntax and error diagnostics; this also
//     covers the three "endmodule"s, which have no HLDB node of their own
//
// Call arguments are read through NamedArgument::getHighConn(): this HLDB
// wraps every tf_call argument, positional ones included, in a NamedArgument.
//
// NOT CHECKED:
//   - runtime effects (the timescale text that $printtimescale prints for
//     "mod0.m") -- HLC is a compiler/elaborator with no simulation
//     capability; the .sv header tags this test as "simulation parsing".
//   - the SourceFile's time unit / precision -- Sec 22.7 applies
//     "`timescale" to design elements; a per-file timescale is an HLDB
//     detail, not something the LRM defines.
//   - vpiIsProperty on "$printtimescale(mod0.m)" -- that is an HLDB
//     encoding, not something Sec 20.4 dictates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class PrintTimescaleHierTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.4--printtimescale-hier.hlc"}); }
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

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Module *getMod0() { return hldb::findByName<hldb::Module>("mod0", m_design->getAllModules()); }
  static const hldb::Module *getMod1() { return hldb::findByName<hldb::Module>("mod1", m_design->getAllModules()); }

  static const hldb::SysTaskCall *getInitialSysTaskCall(const hldb::Module *mod) {
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    const hldb::Initial *const init = any_cast<hldb::Initial>(mod->getProcesses()->at(0));
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::SysTaskCall>();
  }

  static const hldb::SysTaskCall *getPrintTimescaleCall() { return getInitialSysTaskCall(getTop()); }

  static const hldb::SysTaskCall *getMod1DisplayCall() { return getInitialSysTaskCall(getMod1()); }

  static const hldb::RefObj *getHierPathArg() {
    const hldb::SysTaskCall *const call = getPrintTimescaleCall();
    if (call == nullptr) {
      return nullptr;
    }
    return getArgument<hldb::RefObj>(call->getArguments(), 0);
  }

  static const hldb::RefInstance *getMod0InstanceM() {
    const hldb::Module *const mod0 = getMod0();
    if (mod0 == nullptr || mod0->getRefInstances() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::RefInstance>("m", mod0->getRefInstances());
  }
};

// --- modules -------------------------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, AllThreeModulesExist) {
  EXPECT_NE(getTop(), nullptr);
  EXPECT_NE(getMod0(), nullptr);
  EXPECT_NE(getMod1(), nullptr);
}

TEST_F(PrintTimescaleHierTaskTest, TopAndMod0AreTopLevelButMod1IsNot) {
  const hldb::Module *const top = getTop();
  const hldb::Module *const mod0 = getMod0();
  const hldb::Module *const mod1 = getMod1();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(mod0, nullptr);
  ASSERT_NE(mod1, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_TRUE(top->getTopModule()) << "23.3.1: 'top' is never instantiated, so it is a top-level module";
    EXPECT_TRUE(mod0->getTopModule()) << "23.3.1: 'mod0' is never instantiated, so it is a top-level module";
    EXPECT_FALSE(mod1->getTopModule()) << "23.3.1: 'mod1' is instantiated by 'mod0', so it is not top-level";
  } else {
    EXPECT_FALSE(top->getTopModule()) << "all modules are non-top modules in non-elaborated model";
    EXPECT_FALSE(mod0->getTopModule()) << "all modules are non-top modules in non-elaborated model";
    EXPECT_FALSE(mod1->getTopModule()) << "all modules are non-top modules in non-elaborated model";
  }
}

// --- `timescale directives ------------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, TopTimescaleIsMillisecondOverMicrosecond) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getTimeUnit(), -3) << "22.7: '`timescale 1 ms / 1 us' -> time unit -3";
  EXPECT_EQ(top->getTimePrecision(), -6) << "22.7: '`timescale 1 ms / 1 us' -> time precision -6";
}

TEST_F(PrintTimescaleHierTaskTest, Mod0TimescaleIsMicrosecondOverNanosecond) {
  const hldb::Module *const mod0 = getMod0();
  ASSERT_NE(mod0, nullptr);
  EXPECT_EQ(mod0->getTimeUnit(), -6) << "22.7: '`timescale 1 us / 1 ns' -> time unit -6";
  EXPECT_EQ(mod0->getTimePrecision(), -9) << "22.7: '`timescale 1 us / 1 ns' -> time precision -9";
}

TEST_F(PrintTimescaleHierTaskTest, Mod1TimescaleIsNanosecondOverPicosecond) {
  const hldb::Module *const mod1 = getMod1();
  ASSERT_NE(mod1, nullptr);
  EXPECT_EQ(mod1->getTimeUnit(), -9) << "22.7: '`timescale 1 ns / 1 ps' -> time unit -9";
  EXPECT_EQ(mod1->getTimePrecision(), -12) << "22.7: '`timescale 1 ns / 1 ps' -> time precision -12";
}

// --- top: $printtimescale(mod0.m); ----------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, TopHasOneInitialProcessWithPrintTimescaleCall) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);

  const hldb::SysTaskCall *const call = getPrintTimescaleCall();
  ASSERT_NE(call, nullptr) << "'$printtimescale(...);' should be the Initial's statement directly, with no Begin "
                              "wrapper";
  EXPECT_EQ(call->getName(), "$printtimescale");
}

TEST_F(PrintTimescaleHierTaskTest, PrintTimescaleCallHasOneHierPathArgument) {
  const hldb::SysTaskCall *const call = getPrintTimescaleCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "20.4.1: one hierarchical_identifier argument, 'mod0.m'";

  const hldb::RefObj *const arg = getHierPathArg();
  ASSERT_NE(arg, nullptr) << "'mod0.m' should be a RefObj hierarchical reference";
  EXPECT_EQ(arg->getName(), "mod0.m");
}

TEST_F(PrintTimescaleHierTaskTest, HierPathArgHasPathElemsModZeroThenM) {
  const hldb::RefObj *const arg = getHierPathArg();
  ASSERT_NE(arg, nullptr);
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 2u) << "expected 'mod0' -> 'm'";

  const hldb::RefObj *const mod0Elem = any_cast<hldb::RefObj>(arg->getPathElems()->at(0));
  ASSERT_NE(mod0Elem, nullptr) << "'mod0' should be a RefObj path element";
  EXPECT_EQ(mod0Elem->getName(), "mod0");
  EXPECT_EQ(mod0Elem->getActual<hldb::Module>(), getMod0()) << "23.6: 'mod0' names the top-level module 'mod0'";

  const hldb::RefObj *const mElem = any_cast<hldb::RefObj>(arg->getPathElems()->at(1));
  ASSERT_NE(mElem, nullptr) << "'m' should be a RefObj path element";
  EXPECT_EQ(mElem->getName(), "m");
  EXPECT_EQ(mElem->getActual<hldb::RefInstance>(), getMod0InstanceM()) << "'m' names mod0's instance 'm'";
}

TEST_F(PrintTimescaleHierTaskTest, HierPathArgBindsToMod0InstanceM) {
  const hldb::RefObj *const arg = getHierPathArg();
  ASSERT_NE(arg, nullptr);
  ASSERT_NE(getMod0InstanceM(), nullptr);
  EXPECT_EQ(arg->getActual<hldb::RefInstance>(), getMod0InstanceM())
      << "20.4.1: 'mod0.m' should bind to the instance 'm' inside 'mod0'";
}

// --- mod0: mod1 m(); -------------------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, Mod0HasOneInstanceNamedM) {
  const hldb::Module *const mod0 = getMod0();
  ASSERT_NE(mod0, nullptr);
  ASSERT_NE(mod0->getRefInstances(), nullptr);
  EXPECT_EQ(mod0->getRefInstances()->size(), 1u) << "'mod1 m();' is mod0's only instantiation";
  EXPECT_NE(getMod0InstanceM(), nullptr) << "mod0 should have an instance named 'm'";
}

TEST_F(PrintTimescaleHierTaskTest, Mod0InstanceMIsOfModuleMod1) {
  const hldb::RefInstance *const m = getMod0InstanceM();
  ASSERT_NE(m, nullptr);
  const hldb::RefTypespec *const ref = m->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(ref, nullptr) << "'m()' should have a RefTypespec";
  const hldb::ModuleTypespec *const ts = ref->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(ts, nullptr) << "'mod1 m();' should have a ModuleTypespec";
  EXPECT_EQ(ts->getModule(), getMod1()) << "'mod1 m();' should instantiate module 'mod1'";
}

// --- mod1: $display("mod1"); ------------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, Mod1HasOneInitialProcessWithDisplayCall) {
  const hldb::Module *const mod1 = getMod1();
  ASSERT_NE(mod1, nullptr);
  ASSERT_NE(mod1->getProcesses(), nullptr);
  EXPECT_EQ(mod1->getProcesses()->size(), 1u);

  const hldb::SysTaskCall *const call = getMod1DisplayCall();
  ASSERT_NE(call, nullptr) << "'$display(...);' should be mod1's Initial's statement directly, with no Begin "
                              "wrapper";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(PrintTimescaleHierTaskTest, Mod1DisplayCallHasOneStringConstantArgument) {
  const hldb::SysTaskCall *const call = getMod1DisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  const hldb::Constant *const arg = getArgument<hldb::Constant>(call->getArguments(), 0);
  ASSERT_NE(arg, nullptr) << "'\"mod1\"' should be a Constant";
  EXPECT_EQ(arg->getConstType(), vpiStringConst) << "'\"mod1\"' is a string literal";
  EXPECT_EQ(arg->getDecompile(), "\"mod1\"");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, CompilesWithNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "each 'module ... endmodule' should parse as one complete module";
  EXPECT_EQ(stats.nbError, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
