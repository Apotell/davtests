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
// Three modules, each under its own "`timescale" directive. UHDM encodes
// time values as powers-of-10 exponents (SI notation):
//   top:  1 ms / 1 us -> vpiTimeUnit -3, vpiTimePrecision -6
//   mod0: 1 us / 1 ns -> vpiTimeUnit -6, vpiTimePrecision -9
//   mod1: 1 ns / 1 ps -> vpiTimeUnit -9, vpiTimePrecision -12
// The SourceFile itself reflects whichever "`timescale" directive was last
// active when the file finished parsing (mod1's: -9 / -12), NOT the first
// directive in the file -- unlike 20.4--printtimescale, where a single
// directive meant the SourceFile and the one Module agreed.
//
// $printtimescale (tags: 20.4) is called here WITH an explicit argument,
// "mod0.m" -- a hierarchical path naming instance "m" (an instantiation of
// mod1) inside module mod0. Because it is called with parentheses and an
// argument, it is NOT flagged as a property (vpiIsProperty) in the HLDB,
// unlike the parenthesis-less "$printtimescale;" in 20.4--printtimescale.
//
// There is no "begin ... end" in top's or mod1's source, so each Initial
// process's statement is its SysTaskCall directly, with no Begin wrapper
// in between.
//
// Checked:
//   - design has modules "top", "mod0", "mod1"
//   - each module's time unit / time precision matches its own
//     "`timescale" directive; the SourceFile's matches mod1's (the last
//     directive active at end of file)
//   - top has exactly 1 process, an Initial whose statement is a
//     SysTaskCall named "$printtimescale" directly (no Begin wrapper),
//     with exactly 1 argument: a RefObj named "mod0.m"
//   - the "mod0.m" RefObj has exactly 2 path elements ("mod0" resolving to
//     Module "mod0", "m" resolving to a RefInstance) and itself resolves
//     (getActual) to that same RefInstance
//   - the "$printtimescale(mod0.m)" SysTaskCall is NOT flagged as a
//     property (vpiIsProperty), since it has an explicit argument list
//   - mod0 has exactly 1 RefInstance, named "m", whose typespec resolves
//     to Module "mod1" (the "mod1 m();" instantiation)
//   - mod1 has exactly 1 process, an Initial whose statement is a
//     SysTaskCall named "$display" directly (no Begin wrapper), with
//     exactly 1 argument: a Constant string "mod1"
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects (the actual timescale banner text that
// $printtimescale would print for the "mod0.m" instance) cannot be
// observed -- HLC is a compiler/elaborator with no simulation capability,
// so no execution ever happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class PrintTimescaleHierTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20.4--printtimescale-hier.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Module *getMod0() { return hldb::findByName<hldb::Module>("mod0", m_design->getAllModules()); }
  static const hldb::Module *getMod1() { return hldb::findByName<hldb::Module>("mod1", m_design->getAllModules()); }

  static const hldb::SourceFile *getSourceFile() {
    if (m_design->getSourceFiles() == nullptr || m_design->getSourceFiles()->empty()) {
      return nullptr;
    }
    return m_design->getSourceFiles()->at(0);
  }

  static const hldb::SysTaskCall *getPrintTimescaleCall() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) {
      return nullptr;
    }
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::SysTaskCall>();
  }

  static const hldb::RefObj *getHierPathArg() {
    const hldb::SysTaskCall *const call = getPrintTimescaleCall();
    if (call == nullptr || call->getArguments() == nullptr || call->getArguments()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::RefObj>(call->getArguments()->at(0));
  }

  static const hldb::RefInstance *getMod0RefInstance() {
    const hldb::Module *const mod0 = getMod0();
    if (mod0 == nullptr || mod0->getRefInstances() == nullptr || mod0->getRefInstances()->empty()) {
      return nullptr;
    }
    return mod0->getRefInstances()->at(0);
  }

  static const hldb::SysTaskCall *getMod1DisplayCall() {
    const hldb::Module *const mod1 = getMod1();
    if (mod1 == nullptr || mod1->getProcesses() == nullptr || mod1->getProcesses()->empty()) {
      return nullptr;
    }
    const hldb::Initial *const init = any_cast<hldb::Initial>(mod1->getProcesses()->at(0));
    if (init == nullptr) {
      return nullptr;
    }
    return init->getStmt<hldb::SysTaskCall>();
  }
};

// --- modules -------------------------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, AllThreeModulesExist) {
  EXPECT_NE(getTop(), nullptr);
  EXPECT_NE(getMod0(), nullptr);
  EXPECT_NE(getMod1(), nullptr);
}

// --- `timescale directives ------------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, TopTimeUnitAndPrecisionAreMillisecondAndMicrosecond) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getTimeUnit(), -3) << "time unit should be -3 (1 ms = 10^-3 s)";
  EXPECT_EQ(top->getTimePrecision(), -6) << "time precision should be -6 (1 us = 10^-6 s)";
}

TEST_F(PrintTimescaleHierTaskTest, Mod0TimeUnitAndPrecisionAreMicrosecondAndNanosecond) {
  const hldb::Module *const mod0 = getMod0();
  ASSERT_NE(mod0, nullptr);
  EXPECT_EQ(mod0->getTimeUnit(), -6) << "time unit should be -6 (1 us = 10^-6 s)";
  EXPECT_EQ(mod0->getTimePrecision(), -9) << "time precision should be -9 (1 ns = 10^-9 s)";
}

TEST_F(PrintTimescaleHierTaskTest, Mod1TimeUnitAndPrecisionAreNanosecondAndPicosecond) {
  const hldb::Module *const mod1 = getMod1();
  ASSERT_NE(mod1, nullptr);
  EXPECT_EQ(mod1->getTimeUnit(), -9) << "time unit should be -9 (1 ns = 10^-9 s)";
  EXPECT_EQ(mod1->getTimePrecision(), -12) << "time precision should be -12 (1 ps = 10^-12 s)";
}

TEST_F(PrintTimescaleHierTaskTest, SourceFileTimeUnitAndPrecisionMatchMod1sLastDirective) {
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimeUnit(), -9) << "source file time unit should match mod1's directive, the last in the file";
  EXPECT_EQ(sf->getTimePrecision(), -12)
      << "source file time precision should match mod1's directive, the last in the file";
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

TEST_F(PrintTimescaleHierTaskTest, PrintTimescaleCallHasOneHierPathArgumentAndIsNotProperty) {
  const hldb::SysTaskCall *const call = getPrintTimescaleCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  EXPECT_FALSE(call->getIsProperty()) << "20.4: '$printtimescale(mod0.m)' has an explicit argument list, so it is "
                                         "not modeled as a property, unlike a parenthesis-less '$printtimescale'";
}

TEST_F(PrintTimescaleHierTaskTest, HierPathArgIsNamedModZeroDotM) {
  const hldb::RefObj *const arg = getHierPathArg();
  ASSERT_NE(arg, nullptr) << "'mod0.m' should be a RefObj hierarchical path";
  EXPECT_EQ(arg->getName(), "mod0.m");
}

TEST_F(PrintTimescaleHierTaskTest, HierPathArgHasTwoPathElemsModZeroThenM) {
  const hldb::RefObj *const arg = getHierPathArg();
  ASSERT_NE(arg, nullptr);
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 2u) << "expected 'mod0' -> 'm'";

  const hldb::RefObj *const mod0Elem = any_cast<hldb::RefObj>(arg->getPathElems()->at(0));
  ASSERT_NE(mod0Elem, nullptr) << "'mod0' should be a RefObj path element";
  EXPECT_EQ(mod0Elem->getName(), "mod0");
  EXPECT_EQ(mod0Elem->getActual<hldb::Module>(), getMod0());

  const hldb::RefObj *const mElem = any_cast<hldb::RefObj>(arg->getPathElems()->at(1));
  ASSERT_NE(mElem, nullptr) << "'m' should be a RefObj path element";
  EXPECT_EQ(mElem->getName(), "m");
  EXPECT_EQ(mElem->getActual<hldb::RefInstance>(), getMod0RefInstance());
}

TEST_F(PrintTimescaleHierTaskTest, HierPathArgResolvesToModZeroRefInstanceM) {
  const hldb::RefObj *const arg = getHierPathArg();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getActual<hldb::RefInstance>(), getMod0RefInstance());
}

// --- mod0: mod1 m(); -------------------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, Mod0HasOneRefInstanceNamedM) {
  const hldb::Module *const mod0 = getMod0();
  ASSERT_NE(mod0, nullptr);
  ASSERT_NE(mod0->getRefInstances(), nullptr);
  ASSERT_EQ(mod0->getRefInstances()->size(), 1u);

  const hldb::RefInstance *const m = getMod0RefInstance();
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "m");
}

TEST_F(PrintTimescaleHierTaskTest, Mod0RefInstanceMIsTypedAsMod1) {
  const hldb::RefInstance *const m = getMod0RefInstance();
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

  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "'\"mod1\"' should be a Constant";
  EXPECT_EQ(arg->getConstType(), vpiStringConst);
  EXPECT_EQ(arg->getDecompile(), "\"mod1\"");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(PrintTimescaleHierTaskTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
