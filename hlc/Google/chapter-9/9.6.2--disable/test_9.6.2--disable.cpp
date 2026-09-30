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

// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-9/9.6.2--disable.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: disable
// :description: disable block
// :tags: 9.6.2
// */
// module fork_tb ();
// 	reg a = 0;
// 	reg b = 0;
// 	initial begin: block
// 		a = 1;
// 		disable block;
// 		b = 1;
// 	end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.6.2, "Disabling named blocks"):
// "disable block_identifier;" immediately terminates the named block (or
// task) it refers to. Here "disable block;" appears directly inside the
// named block it targets ("initial begin: block ... end"), so it is a
// self-referential disable -- the statement following it ("b = 1;") is
// never reached, but that is a simulation-time effect HLC (a compiler /
// elaborator) does not execute; only the static binding is checked here.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - design has module "fork_tb" with exactly 2 variables: "a", "b"
//   - each variable: LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Initial
//   - "initial begin: block ... end" binds the named Begin "block" directly
//     as the Initial's statement (no extra wrapping Begin is introduced)
//   - the Begin's own name (Scope::getName()) is "block" -- the
//     block-header identifier from "begin: block"
//   - the Begin's getEndLabel() is empty -- the source closes with a bare
//     "end", not "end: block"
//   - the Begin contains exactly 3 sequential statements: "a = 1;",
//     "disable block;", "b = 1;"
//   - the first and third statements are plain blocking Assignments to "a"
//     and "b" respectively, each with a Constant "1" (vpiUIntConst) rhs
//   - the second statement is a Disable with getVpiType() == vpiDisable;
//     its expr resolves to a RefObj named "block" whose actual binds back
//     to the enclosing Begin itself (the self-referential disable target)
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Runtime effect of "disable block;" (that "b = 1;" never executes):
//     HLC is a compiler/elaborator with no simulation, so no execution ever
//     happens for this test to observe.
// ============================================================================

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/disable.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DisableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.6.2--disable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  }

  static const hldb::Variable *getVariable(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr)
      return nullptr;
    return hldb::findByName<hldb::Variable>(name, mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableTypespec(std::string_view name) {
    const hldb::Variable *const v = getVariable(name);
    if (v == nullptr || v->getTypespec() == nullptr)
      return nullptr;
    return v->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty())
      return nullptr;
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getBlockBegin() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr)
      return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Any *getBlockStmt(size_t index) {
    const hldb::Begin *const begin = getBlockBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() <= index)
      return nullptr;
    return begin->getStmts()->at(index);
  }

  static const hldb::Assignment *getBlockAssignment(size_t index) {
    return any_cast<hldb::Assignment>(getBlockStmt(index));
  }

  static const hldb::Disable *getDisableStmt() { return any_cast<hldb::Disable>(getBlockStmt(1)); }
};

// --- module / variables "a", "b" ------------------------------------------

TEST_F(DisableTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(DisableTest, ModuleHasTwoVariables) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 2u);
}

TEST_F(DisableTest, VariableAExists) { EXPECT_NE(getVariable("a"), nullptr); }
TEST_F(DisableTest, VariableBExists) { EXPECT_NE(getVariable("b"), nullptr); }

// "reg a/b" have no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// neither must also appear in the module's net collection.
TEST_F(DisableTest, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", mod->getNets()), nullptr);
  }
}

TEST_F(DisableTest, VariableTypespecsAreLogicUnsignedWithNoRanges) {
  for (std::string_view name : {"a", "b"}) {
    const hldb::LogicTypespec *const ts = getVariableTypespec(name);
    ASSERT_NE(ts, nullptr) << "'reg " << name << "' should resolve to LogicTypespec";
    EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg " << name << "' with no 'signed' keyword defaults to unsigned";
    EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
        << "'reg " << name << "' declares no '[msb:lsb]' -- it is an implicit scalar bit";
  }
}

TEST_F(DisableTest, AllVariablesInitialValueIsConstantZero) {
  for (std::string_view name : {"a", "b"}) {
    const hldb::Variable *const v = getVariable(name);
    ASSERT_NE(v, nullptr);
    const hldb::Constant *const val = v->getValue<hldb::Constant>();
    ASSERT_NE(val, nullptr) << "'reg " << name << " = 0' must carry an initial value";
    EXPECT_EQ(val->getDecompile(), "0");
    EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
  }
}

// --- initial process / named block "block" ----------------------------------

TEST_F(DisableTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(DisableTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

// 9.6.2: "initial begin: block ... end" binds the named Begin directly as
// the Initial's statement -- no extra wrapping Begin is introduced.
TEST_F(DisableTest, InitialBindsNamedBeginDirectly) {
  EXPECT_NE(getBlockBegin(), nullptr) << "'initial begin: block ... end' must bind a Begin directly";
}

TEST_F(DisableTest, BlockBeginHasNameBlockAndNoEndLabel) {
  const hldb::Begin *const begin = getBlockBegin();
  ASSERT_NE(begin, nullptr);
  EXPECT_EQ(begin->getName(), "block") << "'begin: block' should set the Begin's own name to 'block'";
  EXPECT_TRUE(begin->getEndLabel().empty()) << "the source closes with a bare 'end', not 'end: block'";
}

TEST_F(DisableTest, BlockBeginHasThreeStmts) {
  const hldb::Begin *const begin = getBlockBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u) << "'a = 1;', 'disable block;', 'b = 1;' are exactly 3 statements";
}

// --- a = 1; -----------------------------------------------------------------

TEST_F(DisableTest, FirstStmtAssignsOneToA) {
  const hldb::Assignment *const assign = getBlockAssignment(0);
  ASSERT_NE(assign, nullptr) << "'a = 1;' should be a plain blocking Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'a = ...' uses the blocking assignment operator '='";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("a"));

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

// --- disable block; -----------------------------------------------------------

TEST_F(DisableTest, SecondStmtIsDisable) {
  EXPECT_NE(getDisableStmt(), nullptr) << "9.6.2: the second statement must be a Disable";
}

TEST_F(DisableTest, DisableHasVpiDisableType) {
  const hldb::Disable *const disable = getDisableStmt();
  ASSERT_NE(disable, nullptr);
  EXPECT_EQ(disable->getVpiType(), vpiDisable);
}

TEST_F(DisableTest, DisableTargetsTheEnclosingNamedBlock) {
  const hldb::Disable *const disable = getDisableStmt();
  ASSERT_NE(disable, nullptr);

  const hldb::RefObj *const expr = disable->getExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr) << "'disable block;' should carry a RefObj naming its target";
  EXPECT_EQ(expr->getName(), "block");

  const hldb::Begin *const target = expr->getActual<hldb::Begin>();
  EXPECT_EQ(target, getBlockBegin()) << "'disable block;' is self-referential -- it targets the "
                                         "'begin: block' it is nested in";
}

// --- b = 1; -----------------------------------------------------------------

TEST_F(DisableTest, ThirdStmtAssignsOneToB) {
  const hldb::Assignment *const assign = getBlockAssignment(2);
  ASSERT_NE(assign, nullptr) << "'b = 1;' should be a plain blocking Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'b = ...' uses the blocking assignment operator '='";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("b"));

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

// --- compiler diagnostics ----------------------------------------------------

TEST_F(DisableTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a', 'b' and 'block' must all bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
