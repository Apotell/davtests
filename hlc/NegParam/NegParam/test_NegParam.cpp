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

// Tests for tests/NegParam/dut.sv:
//
//   module top();
//    localparam dram_base_addr_gp         = 40'h00_8000_0000;
//    localparam bp_proc_param_s bp_default_cfg_p =
//       '{
//         boot_pc       : dram_base_addr_gp
//       };
//   endmodule
//
//   module prim_packer #(
//     parameter int InW  = 32,
//     parameter int OutW = 32,
//     parameter int HintByteData = 0
//   ) ();
//     localparam int Width = InW + OutW;
//     localparam int ConcatW = Width + InW;
//     localparam int PtrW = $clog2(ConcatW+1);
//     localparam int IdxW = $clog2(InW) + ~|$clog2(InW);
//     logic [PtrW-1:0]          pos, pos_next;
//     logic [IdxW-1:0]          lod_idx;
//     if (IdxW != 5) begin
//       BAD bad();
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023):
//   - Sec 23.3.1: neither module is instantiated anywhere, so both are
//     top-level modules, "implicitly instantiated once, and its instance
//     name is the same as the module name".
//   - Sec 6.20.2: "A parameter declaration with no type or range
//     specification shall default to the type and range of the final value
//     ... If the expression is integral, the parameter is a logic vector of
//     the same size with range [size-1:0]." dram_base_addr_gp therefore is
//     a 40-bit logic vector [39:0]. Sec 5.7.1: 40'h00_8000_0000 is an
//     unsigned based hex literal of size 40 whose value is 0x80000000 --
//     a positive value (2147483648) that must NOT be sign-wrapped into a
//     negative 32-bit integer (the point of this regression test).
//   - Sec 6.5: "Data shall be declared before they are used" --
//     bp_proc_param_s is never declared, so using it as the type of
//     bp_default_cfg_p must be diagnosed.
//   - Sec 10.9.2: the initializer '{boot_pc : dram_base_addr_gp} is a
//     structure assignment pattern with a member-name key.
//   - Sec 6.20.2/6.20.1: InW/OutW/HintByteData are (overridable) int
//     parameters, Width/ConcatW/PtrW/IdxW are localparams.
//   - Sec 11.4.9 + 20.8.1: IdxW = $clog2(InW) + ~|$clog2(InW) is an add of
//     a $clog2 call and a reduction-NOR (vpiUnaryNorOp) of a $clog2 call.
//     With InW = 32, $clog2(32) = 5 and ~|5 = 0, so IdxW = 5.
//   - Sec 6.8: "logic [..] pos, pos_next;" has no net-type keyword and is
//     therefore a variable declaration, not a net.
//   - Sec 27.3/27.5: the if-generate condition IdxW != 5 is false for the
//     default parameter values, so the generate block is not instantiated
//     and "BAD bad();" is never brought into existence. Sec 23.3.1 also
//     notes an instantiation "in a generate block that is not itself
//     instantiated" does not count. No diagnostic may therefore be reported
//     for the undefined module BAD.
//
// What is NOT checked and why:
//   - Elaborated values of Width/ConcatW/PtrW/IdxW: the .hlc does not
//     request elaboration, so only the declared expressions are available.
//   - Timescale warnings: tool-specific, not mandated by the standard.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/tagged_pattern.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>

namespace hlc {

class NegParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NegParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Locate by definition name so that the lookup does not depend on how the
  // instance name is spelled (that is checked separately).
  static const hldb::Module *getModuleByDef(std::string_view defName) {
    if (m_design == nullptr || m_design->getAllModules() == nullptr) return nullptr;
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      if (m->getDefName() == defName) return m;
    }
    return nullptr;
  }

  static const hldb::Parameter *getParam(std::string_view mod, std::string_view name) {
    const hldb::Module *const m = getModuleByDef(mod);
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, m->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(std::string_view mod, std::string_view name) {
    const hldb::Module *const m = getModuleByDef(mod);
    if (m == nullptr || m->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *m->getParamAssigns()) {
      const hldb::Any *const lhs = pa->getLhs();
      if (lhs == nullptr) continue;
      if (const hldb::Parameter *const p = any_cast<hldb::Parameter>(lhs)) {
        if (p->getName() == name) return pa;
      } else if (const hldb::RefObj *const r = any_cast<hldb::RefObj>(lhs)) {
        if (r->getName() == name) return pa;
      }
    }
    return nullptr;
  }

  static const hldb::Variable *getVar(std::string_view mod, std::string_view name) {
    const hldb::Module *const m = getModuleByDef(mod);
    if (m == nullptr || m->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, m->getVariables());
  }

  // Parse an integral Constant's value; returns false when not parseable.
  static bool constValue(const hldb::Any *any, uint64_t &out) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(any);
    if (c == nullptr) return false;
    std::string v(c->getValue());
    const std::string::size_type colon = v.find(':');
    if (colon != std::string::npos) v = v.substr(colon + 1);
    int base = 10;
    if (c->getConstType() == vpiHexConst) base = 16;
    if (c->getConstType() == vpiBinaryConst) base = 2;
    if (c->getConstType() == vpiOctConst) base = 8;
    try {
      out = std::stoull(v, nullptr, base);
    } catch (...) {
      return false;
    }
    return true;
  }
};

// ---------------------------------------------------------------------------
// Sec 23.3.1: both modules are top-level, instance name == module name
// ---------------------------------------------------------------------------

TEST_F(NegParamTest, ModuleTopExists) {
  const hldb::Module *const top = getModuleByDef("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_EQ(top->getName(), std::string_view("top"));
}

TEST_F(NegParamTest, ModulePrimPackerExistsWithModuleName) {
  const hldb::Module *const pp = getModuleByDef("prim_packer");
  ASSERT_NE(pp, nullptr) << "module 'prim_packer' not found";
  EXPECT_EQ(pp->getName(), std::string_view("prim_packer"))
      << "Sec 23.3.1: a top-level instance's name is the same as the module name";
}

// ---------------------------------------------------------------------------
// localparam dram_base_addr_gp = 40'h00_8000_0000;
// ---------------------------------------------------------------------------

TEST_F(NegParamTest, DramBaseAddrIsLocalParam) {
  const hldb::Parameter *const p = getParam("top", "dram_base_addr_gp");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam());
}

TEST_F(NegParamTest, DramBaseAddrValueIs40BitHexPositive) {
  const hldb::ParamAssign *const pa = getParamAssign("top", "dram_base_addr_gp");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs(), nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(c, nullptr) << "a literal initializer must be a Constant";
  EXPECT_EQ(c->getConstType(), vpiHexConst);
  EXPECT_EQ(c->getSize(), 40) << "Sec 5.7.1: 40'h... is a 40-bit literal";
  uint64_t v = 0;
  ASSERT_TRUE(constValue(c, v)) << "value: " << c->getValue();
  EXPECT_EQ(v, 0x80000000ULL) << "Sec 5.7.1: unsigned 40-bit value 0x80000000 must not be sign-wrapped";
}

TEST_F(NegParamTest, DramBaseAddrTypeIsLogic39To0) {
  const hldb::Parameter *const p = getParam("top", "dram_base_addr_gp");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "Sec 6.20.2: integral final value -> logic vector";
  EXPECT_FALSE(lt->getSigned()) << "Sec 5.7.1: a based literal without 's' is unsigned";
  ASSERT_NE(lt->getRanges(), nullptr) << "Sec 6.20.2: range [size-1:0] = [39:0]";
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  uint64_t left = 0;
  uint64_t right = 1;
  ASSERT_TRUE(constValue(r->getLeftExpr(), left));
  ASSERT_TRUE(constValue(r->getRightExpr(), right));
  EXPECT_EQ(left, 39u);
  EXPECT_EQ(right, 0u);
}

// ---------------------------------------------------------------------------
// localparam bp_proc_param_s bp_default_cfg_p = '{boot_pc : ...};
// ---------------------------------------------------------------------------

TEST_F(NegParamTest, BpDefaultCfgIsLocalParamWithNamedType) {
  const hldb::Parameter *const p = getParam("top", "bp_default_cfg_p");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam());
  ASSERT_NE(p->getTypespec(), nullptr);
  EXPECT_EQ(p->getTypespec()->getName(), std::string_view("bp_proc_param_s"));
}

TEST_F(NegParamTest, UndeclaredTypeBpProcParamSIsDiagnosed) {
  // Sec 6.5: data shall be declared before they are used.
  const bool found = (findError(ErrorDefinition::COMP_FAILED_TO_BIND, "bp_proc_param_s") != nullptr) ||
                     (findError(ErrorDefinition::COMP_UNDEFINED_TYPE, "bp_proc_param_s") != nullptr);
  EXPECT_TRUE(found) << "use of undeclared type 'bp_proc_param_s' must be diagnosed";
}

TEST_F(NegParamTest, BpDefaultCfgInitializerIsKeyedAssignmentPattern) {
  const hldb::ParamAssign *const pa = getParamAssign("top", "bp_default_cfg_p");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs(), nullptr);
  const hldb::Operation *const op = any_cast<hldb::Operation>(pa->getRhs());
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp) << "Sec 10.9: '{...} is an assignment pattern";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(op->getOperands()->at(0));
  ASSERT_NE(tp, nullptr) << "member_identifier : expression must be a keyed (tagged) pattern";
  const hldb::RefObj *const tag = any_cast<hldb::RefObj>(tp->getTag());
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->getName(), std::string_view("boot_pc"));
  const hldb::RefObj *const val = any_cast<hldb::RefObj>(tp->getPattern());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), std::string_view("dram_base_addr_gp"));
  ASSERT_NE(val->getActual(), nullptr);
  EXPECT_EQ(val->getActual()->getAnyType(), hldb::AnyType::Parameter);
}

// ---------------------------------------------------------------------------
// prim_packer parameters
// ---------------------------------------------------------------------------

TEST_F(NegParamTest, PrimPackerPortParamsAreOverridableInts) {
  for (const char *const name : {"InW", "OutW", "HintByteData"}) {
    const hldb::Parameter *const p = getParam("prim_packer", name);
    ASSERT_NE(p, nullptr) << name;
    EXPECT_FALSE(p->getLocalParam()) << name;
    ASSERT_NE(p->getTypespec(), nullptr) << name;
    const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << name << ": 'parameter int' must have int type";
  }
}

TEST_F(NegParamTest, PrimPackerPortParamDefaults) {
  struct ParamDefault {
    const char *m_name;
    uint64_t m_value;
  };
  const ParamDefault expected[] = {{"InW", 32u}, {"OutW", 32u}, {"HintByteData", 0u}};
  for (const ParamDefault &e : expected) {
    const hldb::ParamAssign *const pa = getParamAssign("prim_packer", e.m_name);
    ASSERT_NE(pa, nullptr) << e.m_name;
    uint64_t v = 99;
    ASSERT_TRUE(constValue(pa->getRhs(), v)) << e.m_name;
    EXPECT_EQ(v, e.m_value) << e.m_name;
  }
}

TEST_F(NegParamTest, PrimPackerBodyParamsAreLocalParams) {
  for (const char *const name : {"Width", "ConcatW", "PtrW", "IdxW"}) {
    const hldb::Parameter *const p = getParam("prim_packer", name);
    ASSERT_NE(p, nullptr) << name;
    EXPECT_TRUE(p->getLocalParam()) << name;
  }
}

TEST_F(NegParamTest, IdxWIsClog2PlusReductionNorOfClog2) {
  const hldb::ParamAssign *const pa = getParamAssign("prim_packer", "IdxW");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const add = any_cast<hldb::Operation>(pa->getRhs());
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);

  const hldb::SysFuncCall *const lhs = any_cast<hldb::SysFuncCall>(add->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("$clog2"));

  const hldb::Operation *const nor = any_cast<hldb::Operation>(add->getOperands()->at(1));
  ASSERT_NE(nor, nullptr);
  EXPECT_EQ(nor->getOpType(), vpiUnaryNorOp) << "Sec 11.4.9: '~|' is the reduction NOR operator";
  ASSERT_NE(nor->getOperands(), nullptr);
  ASSERT_EQ(nor->getOperands()->size(), 1u);
  const hldb::SysFuncCall *const inner = any_cast<hldb::SysFuncCall>(nor->getOperands()->at(0));
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getName(), std::string_view("$clog2"));
  ASSERT_NE(inner->getArguments(), nullptr);
  ASSERT_EQ(inner->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(inner->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view("InW"));
}

TEST_F(NegParamTest, PtrWIsClog2OfConcatWPlusOne) {
  const hldb::ParamAssign *const pa = getParamAssign("prim_packer", "PtrW");
  ASSERT_NE(pa, nullptr);
  const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(pa->getRhs());
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), std::string_view("$clog2"));
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Operation *const add = any_cast<hldb::Operation>(call->getArguments()->at(0));
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp);
}

// ---------------------------------------------------------------------------
// Sec 6.8: logic declarations without net keyword are variables
// ---------------------------------------------------------------------------

TEST_F(NegParamTest, PosPosNextLodIdxAreVariablesNotNets) {
  const hldb::Module *const pp = getModuleByDef("prim_packer");
  ASSERT_NE(pp, nullptr);
  for (const char *const name : {"pos", "pos_next", "lod_idx"}) {
    EXPECT_NE(getVar("prim_packer", name), nullptr) << name << " must be a Variable";
    if (pp->getNets() != nullptr) {
      EXPECT_EQ(hldb::findByName<hldb::Net>(name, pp->getNets()), nullptr) << name << " must not be a Net";
    }
  }
}

TEST_F(NegParamTest, PosRangeIsPtrWMinusOneDownToZero) {
  const hldb::Variable *const v = getVar("prim_packer", "pos");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  const hldb::Operation *const left = any_cast<hldb::Operation>(r->getLeftExpr());
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const ptrw = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(ptrw, nullptr);
  EXPECT_EQ(ptrw->getName(), std::string_view("PtrW"));
  uint64_t right = 1;
  ASSERT_TRUE(constValue(r->getRightExpr(), right));
  EXPECT_EQ(right, 0u);
}

// ---------------------------------------------------------------------------
// if (IdxW != 5) begin BAD bad(); end
// ---------------------------------------------------------------------------

TEST_F(NegParamTest, GenIfConditionIsIdxWNotEqual5) {
  const hldb::Module *const pp = getModuleByDef("prim_packer");
  ASSERT_NE(pp, nullptr);
  ASSERT_NE(pp->getGenStmts(), nullptr);
  ASSERT_EQ(pp->getGenStmts()->size(), 1u);
  const hldb::GenIf *const gi = any_cast<hldb::GenIf>(pp->getGenStmts()->at(0));
  ASSERT_NE(gi, nullptr);
  const hldb::Operation *const cond = any_cast<hldb::Operation>(gi->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const idxw = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(idxw, nullptr);
  EXPECT_EQ(idxw->getName(), std::string_view("IdxW"));
  uint64_t five = 0;
  ASSERT_TRUE(constValue(cond->getOperands()->at(1), five));
  EXPECT_EQ(five, 5u);
}

TEST_F(NegParamTest, GenIfBlockHoldsInstanceBad) {
  const hldb::Module *const pp = getModuleByDef("prim_packer");
  ASSERT_NE(pp, nullptr);
  ASSERT_NE(pp->getGenStmts(), nullptr);
  ASSERT_EQ(pp->getGenStmts()->size(), 1u);
  const hldb::GenIf *const gi = any_cast<hldb::GenIf>(pp->getGenStmts()->at(0));
  ASSERT_NE(gi, nullptr);
  const hldb::Begin *const blk = any_cast<hldb::Begin>(gi->getStmt());
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(blk->getStmts()->at(0));
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), std::string_view("bad"));
}

TEST_F(NegParamTest, NoDiagnosticForBadInUninstantiatedGenerateBlock) {
  // Sec 27.3/27.5: IdxW == 5 for the defaults, so the block is never
  // instantiated and 'BAD bad();' is never brought into existence.
  EXPECT_EQ(findError(ErrorDefinition::HLDB_UNSUPPORTED_TYPESPEC, "BAD"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION, "BAD"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "BAD"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
