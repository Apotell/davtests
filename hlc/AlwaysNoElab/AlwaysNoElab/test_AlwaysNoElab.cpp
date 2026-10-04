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

// Tests for dut.sv (tags: AlwaysNoElab), compiled with "-PNrDevices=3"
//   module dut #(
//     parameter int NrDevices = 1
//     )();
//     localparam int unsigned NumBitsDeviceSel = NrDevices > 1 ? $clog2(NrDevices) : 1;
//     logic [NumBitsDeviceSel-1:0] device_sel_req;
//     int device = 2;
//     always_comb begin
//       device_sel_req = NumBitsDeviceSel'(device);
//     end
//   endmodule // dut
//
// HLC compiles this module without elaborating it (no top-level
// instantiation drives a parameter override here). That has 2
// consequences this test exists to pin down:
//   - the "-PNrDevices=3" command-line override has no effect: NrDevices
//     keeps its source default of 1, and NumBitsDeviceSel is computed from
//     that default, not the override
//   - the sized cast "NumBitsDeviceSel'(device)" needs NumBitsDeviceSel to
//     be resolved as a type, but without elaboration HLC cannot turn a
//     local parameter into a cast target type: the cast's typespec binds
//     to an UnsupportedTypespec named "NumBitsDeviceSel" and the compiler
//     reports an HLDB_UNSUPPORTED_TYPESPEC error for it
//
// Checked:
//   - module "dut" (matched by its bare vpiDefName, since its decorated
//     vpiName embeds the unelaborated parameter value) has 2 parameters:
//     NrDevices (not a localparam, ParamAssign rhs constant 1 -- the
//     source default, confirming the "-P" override was not applied) and
//     NumBitsDeviceSel (a localparam, ParamAssign rhs the ternary
//     "NrDevices > 1 ? $clog2(NrDevices) : 1")
//   - module has 2 variables: "device_sel_req" (a vector logic, ranged
//     "[NumBitsDeviceSel-1:0]") and "device" (an int, whose initializer
//     "= 2" is modeled as the Variable's own value, not a separate
//     assignment)
//   - module has exactly 1 process, an Always whose vpiAlwaysType is
//     "comb", and whose statement is a Begin (the source has an explicit
//     "begin ... end") containing a single blocking Assignment
//   - that Assignment's rhs is a cast operation (vpiCastOp) targeting an
//     UnsupportedTypespec named "NumBitsDeviceSel", over operand "device"
//   - compiler reports exactly the HLDB_UNSUPPORTED_TYPESPEC error for
//     symbol "NumBitsDeviceSel"
//
// NOT CHECKED: runtime effects, and whether elaborating this module (e.g.
// as a real top-level with "-PNrDevices=3" actually driving an instance)
// would resolve the cast cleanly -- HLC is a compiler/elaborator with no
// simulation capability, and this particular test input is never
// elaborated, so neither can be observed here.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/unsupported_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AlwaysNoElabTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AlwaysNoElab.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(name, mod->getParamAssigns());
  }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::Assignment *getCombAssignment() {
    const hldb::Always *const always = getAlwaysProcess();
    if (always == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const begin = always->getStmt<hldb::Begin>();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->empty()) {
      return nullptr;
    }
    return any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  }
};

// --- module / parameters -----------------------------------------------------

TEST_F(AlwaysNoElabTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(AlwaysNoElabTest, NrDevicesKeepsSourceDefaultDespiteCommandLineOverride) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParameters(), nullptr);
  ASSERT_EQ(mod->getParameters()->size(), 2u);

  const hldb::Parameter *const nrDevices = hldb::findByName<hldb::Parameter>("NrDevices", mod->getParameters());
  ASSERT_NE(nrDevices, nullptr);
  EXPECT_FALSE(nrDevices->getLocalParam());

  const hldb::ParamAssign *const assign = getParamAssign("NrDevices");
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1") << "'-PNrDevices=3' must not take effect without elaboration";
}

TEST_F(AlwaysNoElabTest, NumBitsDeviceSelIsClog2Ternary) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);

  const hldb::Parameter *const numBits = hldb::findByName<hldb::Parameter>("NumBitsDeviceSel", mod->getParameters());
  ASSERT_NE(numBits, nullptr);
  EXPECT_TRUE(numBits->getLocalParam());

  const hldb::ParamAssign *const assign = getParamAssign("NumBitsDeviceSel");
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const condition = assign->getRhs<hldb::Operation>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiConditionOp);
  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 3u);

  const hldb::Operation *const greaterThan = any_cast<hldb::Operation>(condition->getOperands()->at(0));
  ASSERT_NE(greaterThan, nullptr);
  EXPECT_EQ(greaterThan->getOpType(), vpiGtOp);

  const hldb::SysFuncCall *const clog2 = any_cast<hldb::SysFuncCall>(condition->getOperands()->at(1));
  ASSERT_NE(clog2, nullptr);
  EXPECT_EQ(clog2->getName(), "$clog2");
  ASSERT_NE(clog2->getArguments(), nullptr);
  ASSERT_EQ(clog2->getArguments()->size(), 1u);
  const hldb::RefObj *const clog2Arg = any_cast<hldb::RefObj>(clog2->getArguments()->at(0));
  ASSERT_NE(clog2Arg, nullptr);
  EXPECT_EQ(clog2Arg->getName(), "NrDevices");

  const hldb::Constant *const elseConst = any_cast<hldb::Constant>(condition->getOperands()->at(2));
  ASSERT_NE(elseConst, nullptr);
  EXPECT_EQ(elseConst->getDecompile(), "1");
}

// --- variables ---------------------------------------------------------------

TEST_F(AlwaysNoElabTest, DeviceSelReqIsAVectorLogic) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  ASSERT_EQ(mod->getVariables()->size(), 2u);

  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("device_sel_req", mod->getVariables());
  ASSERT_NE(var, nullptr);
  EXPECT_TRUE(var->getVector());
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(AlwaysNoElabTest, DeviceVariableHasInlineInitializerValue) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);

  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("device", mod->getVariables());
  ASSERT_NE(var, nullptr);
  const hldb::Constant *const value = var->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "'int device = 2' should store its initializer on the Variable itself";
  EXPECT_EQ(value->getDecompile(), "2");
}

// --- always_comb / unresolved sized cast -------------------------------------

TEST_F(AlwaysNoElabTest, ModuleHasOneCombAlwaysProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
  const hldb::Always *const always = getAlwaysProcess();
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysComb);
}

TEST_F(AlwaysNoElabTest, AssignmentCastsToUnsupportedParameterTypespec) {
  const hldb::Assignment *const assign = getCombAssignment();
  ASSERT_NE(assign, nullptr) << "always_comb's statement should be a Begin with a single Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "device_sel_req");

  const hldb::Operation *const cast = assign->getRhs<hldb::Operation>();
  ASSERT_NE(cast, nullptr);
  EXPECT_EQ(cast->getOpType(), vpiCastOp);
  ASSERT_NE(cast->getTypespec(), nullptr);
  const hldb::UnsupportedTypespec *const castType = cast->getTypespec()->getActual<hldb::UnsupportedTypespec>();
  ASSERT_NE(castType, nullptr) << "a localparam-named cast target cannot be resolved without elaboration";
  EXPECT_EQ(castType->getName(), "NumBitsDeviceSel");

  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(cast->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), "device");
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(AlwaysNoElabTest, ReportsUnsupportedTypespecForNumBitsDeviceSel) {
  EXPECT_NE(findError(ErrorDefinition::HLDB_UNSUPPORTED_TYPESPEC, "NumBitsDeviceSel"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
