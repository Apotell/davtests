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

// Tests for tests/ElabCParam/dut.sv -- elaboration-time propagation of a
// module parameter through an instance chain, and its effect on a
// generate-if condition and a dependent localparam:
//
//   module prim_fifo_sync #(parameter int unsigned Depth = 4) ();
//     if (Depth == 0) begin : gen_passthru
//       assign depth = 1'b0;
//     end else begin : gen_normal
//       localparam int unsigned PTRV_W = $clog2(Depth) + ~|$clog2(Depth);
//     end
//   endmodule
//
//   module fifo_sync #(parameter int unsigned ReqDepth = 6) ();
//     prim_fifo_sync #(.Depth(ReqDepth)) reqfifo ();
//   endmodule
//
//   module socket_1n #(parameter int unsigned N = 2,
//                       parameter bit [7:0] DReqDepth = {N{4'h2}}) ();
//     parameter bit [7:0] AA = DReqDepth * 2;
//     parameter bit [7:0] BB = DReqDepth[1*4+:4];
//     for (genvar i = 0; i < N; i++) begin : gen_dfifo
//       fifo_sync #(.ReqDepth(DReqDepth[i*4+:4])) fifo_d ();
//     end
//   endmodule
//
//   module all_zero #(parameter int unsigned N = 2,
//                      parameter bit [7:0] DReqDepth = {2{4'h0}}) (); ...
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 6.20.2/23.3: a parameter override on an instance
// ('.Depth(ReqDepth)') is a constant-expression connection; the override is
// recorded via a by-name, overriding ParamAssign on the instance.
// IEEE 1800-2023 27.5 "Generate-if constructs": the condition of a
// generate-if is a constant expression evaluated at elaboration; here it is
// preserved as an Operation(vpiEqOp) over the parameter and a Constant.
// IEEE 1800-2023 11.4.12.1 "Repetition operator": '{N{4'h2}}' is a multiple
// concatenation, encoded as Operation(vpiMultiConcatOp) with the repeat
// count as its first operand.
// IEEE 1800-2023 11.5.1 "Vector bit-select and part-select addressing":
// 'DReqDepth[1*4+:4]' is an indexed part-select (IndexedPartSelect).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/indexed_part_select.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ElabCParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ElabCParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  template <typename ScopeT>
  static const hldb::Parameter *findParam(const ScopeT *scope, std::string_view name) {
    if (scope == nullptr || scope->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *scope->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  template <typename ScopeT>
  static const hldb::ParamAssign *findParamAssign(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(scope));
  }

  static const hldb::RefInstance *findRefInst(std::string_view instName, const hldb::Module *parent) {
    if (parent == nullptr || parent->getRefInstances() == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(instName, parent->getRefInstances());
  }

  static const hldb::GenIfElse *findGenIfElse(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIfElse *const gie = any_cast<hldb::GenIfElse>(stmt)) return gie;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

TEST_F(ElabCParamTest, ModulesExist) {
  EXPECT_NE(getModule("prim_fifo_sync"), nullptr) << "module 'prim_fifo_sync' not found";
  EXPECT_NE(getModule("fifo_sync"), nullptr) << "module 'fifo_sync' not found";
  EXPECT_NE(getModule("socket_1n"), nullptr) << "module 'socket_1n' not found";
  EXPECT_NE(getModule("all_zero"), nullptr) << "module 'all_zero' not found";
}

// ---------------------------------------------------------------------------
// prim_fifo_sync: parameter Depth, default 4
// ---------------------------------------------------------------------------

TEST_F(ElabCParamTest, PrimFifoSync_DepthParamNotLocalWithDefaultFour) {
  const hldb::Module *const m = getModule("prim_fifo_sync");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const depth = findParam(m, "Depth");
  ASSERT_NE(depth, nullptr) << "'parameter int unsigned Depth' not found";
  EXPECT_FALSE(depth->getLocalParam()) << "'parameter int unsigned Depth' must not be a localparam";

  const hldb::ParamAssign *const pa = findParamAssign(m, "Depth");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'Depth' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'Depth = 4': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "4");
}

// ---------------------------------------------------------------------------
// prim_fifo_sync: 'if (Depth == 0) begin : gen_passthru ... end
//                  else begin : gen_normal ... end' (IEEE 1800-2023 27.5)
// ---------------------------------------------------------------------------

TEST_F(ElabCParamTest, PrimFifoSync_GenerateIfElseExists) {
  const hldb::Module *const m = getModule("prim_fifo_sync");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr) << "prim_fifo_sync has no generate statements";
  EXPECT_NE(findGenIfElse(m), nullptr) << "no GenIfElse found among prim_fifo_sync's generate statements";
}

TEST_F(ElabCParamTest, PrimFifoSync_ConditionIsDepthEqualsZero) {
  const hldb::Module *const m = getModule("prim_fifo_sync");
  ASSERT_NE(m, nullptr);
  const hldb::GenIfElse *const gie = findGenIfElse(m);
  ASSERT_NE(gie, nullptr);

  const hldb::Operation *const cond = gie->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'Depth == 0' condition must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiEqOp) << "'Depth == 0' must produce vpiEqOp";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u) << "binary equality must have exactly two operands";

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(lhs, nullptr) << "'Depth == 0': left operand must be RefObj (reference to Depth)";
  EXPECT_EQ(lhs->getName(), "Depth");

  const hldb::Constant *const rhs = any_cast<hldb::Constant>((*cond->getOperands())[1]);
  ASSERT_NE(rhs, nullptr) << "'Depth == 0': right operand must be Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "0");
}

TEST_F(ElabCParamTest, PrimFifoSync_ThenAndElseBranchesAreNamedBlocks) {
  const hldb::Module *const m = getModule("prim_fifo_sync");
  ASSERT_NE(m, nullptr);
  const hldb::GenIfElse *const gie = findGenIfElse(m);
  ASSERT_NE(gie, nullptr);

  const hldb::Begin *const thenBlk = gie->getStmt<hldb::Begin>();
  ASSERT_NE(thenBlk, nullptr) << "'begin : gen_passthru' body must be a Begin";
  EXPECT_EQ(thenBlk->getName(), "gen_passthru");

  const hldb::Begin *const elseBlk = gie->getElseStmt<hldb::Begin>();
  ASSERT_NE(elseBlk, nullptr) << "'begin : gen_normal' body must be a Begin";
  EXPECT_EQ(elseBlk->getName(), "gen_normal");
}

// ---------------------------------------------------------------------------
// gen_normal: 'localparam int unsigned PTRV_W = $clog2(Depth) +
// ~|$clog2(Depth);' -- a localparam whose default is a constant expression
// referencing the enclosing module's Depth parameter (IEEE 1800-2023
// 6.20.4 "Local parameters").
// ---------------------------------------------------------------------------

TEST_F(ElabCParamTest, GenNormal_PtrvWIsLocalParamWithAddOperation) {
  const hldb::Module *const m = getModule("prim_fifo_sync");
  ASSERT_NE(m, nullptr);
  const hldb::GenIfElse *const gie = findGenIfElse(m);
  ASSERT_NE(gie, nullptr);
  const hldb::Begin *const elseBlk = gie->getElseStmt<hldb::Begin>();
  ASSERT_NE(elseBlk, nullptr);

  const hldb::Parameter *const ptrvW = findParam(elseBlk, "PTRV_W");
  ASSERT_NE(ptrvW, nullptr) << "'localparam int unsigned PTRV_W' not found inside 'gen_normal'";
  EXPECT_TRUE(ptrvW->getLocalParam()) << "'localparam PTRV_W' must be marked as a localparam";

  const hldb::ParamAssign *const pa = findParamAssign(elseBlk, "PTRV_W");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'PTRV_W' not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'$clog2(Depth) + ~|$clog2(Depth)': RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::SysFuncCall *const clog2 = any_cast<hldb::SysFuncCall>((*rhs->getOperands())[0]);
  ASSERT_NE(clog2, nullptr) << "'$clog2(Depth)' left operand must be a SysFuncCall";
  EXPECT_EQ(clog2->getName(), "$clog2");
}

// ---------------------------------------------------------------------------
// fifo_sync: 'prim_fifo_sync #(.Depth(ReqDepth)) reqfifo ();'
// -- parameter propagation from fifo_sync's own ReqDepth into
// prim_fifo_sync's Depth (IEEE 1800-2023 23.3, 23.10 "Overriding
// parameters").
// ---------------------------------------------------------------------------

TEST_F(ElabCParamTest, FifoSync_ReqDepthParamDefaultIsSix) {
  const hldb::Module *const m = getModule("fifo_sync");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const reqDepth = findParam(m, "ReqDepth");
  ASSERT_NE(reqDepth, nullptr) << "'parameter int unsigned ReqDepth' not found";
  EXPECT_FALSE(reqDepth->getLocalParam());

  const hldb::ParamAssign *const pa = findParamAssign(m, "ReqDepth");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'ReqDepth = 6': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "6");
}

TEST_F(ElabCParamTest, FifoSync_InstantiatesPrimFifoSyncWithDepthOverride) {
  const hldb::Module *const m = getModule("fifo_sync");
  ASSERT_NE(m, nullptr);
  const hldb::RefInstance *const reqfifo = findRefInst("reqfifo", m);
  ASSERT_NE(reqfifo, nullptr) << "'prim_fifo_sync #(...) reqfifo ()' RefInstance not found";
  ASSERT_NE(reqfifo->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = reqfifo->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "reqfifo's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("prim_fifo_sync"));

  const hldb::ParamAssign *const pa = findParamAssign(reqfifo, "Depth");
  ASSERT_NE(pa, nullptr) << "'.Depth(ReqDepth)' override not found on 'reqfifo'";
  EXPECT_TRUE(pa->getConnByName()) << "'.Depth(...)' is a by-name parameter connection (Sec 23.3)";
  EXPECT_TRUE(pa->getOverridden()) << "an explicit instance-level override must be marked as overriding the default";

  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'.Depth(ReqDepth)': RHS must be a RefObj (reference to ReqDepth)";
  EXPECT_EQ(rhs->getName(), "ReqDepth");
}

// ---------------------------------------------------------------------------
// socket_1n: parameters N (default 2), DReqDepth (default {N{4'h2}}),
// dependent parameters AA and BB, and a generate-for propagating a
// per-iteration part-select of DReqDepth into fifo_sync's ReqDepth.
// ---------------------------------------------------------------------------

TEST_F(ElabCParamTest, Socket1n_NParamDefaultIsTwo) {
  const hldb::Module *const m = getModule("socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const n = findParam(m, "N");
  ASSERT_NE(n, nullptr) << "'parameter int unsigned N' not found";
  EXPECT_FALSE(n->getLocalParam());

  const hldb::ParamAssign *const pa = findParamAssign(m, "N");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'N = 2': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "2");
}

// IEEE 1800-2023 11.4.12.1: '{N{4'h2}}' is a repetition (multiple
// concatenation): Operation(vpiMultiConcatOp) whose first operand is the
// repeat count.
TEST_F(ElabCParamTest, Socket1n_DReqDepthDefaultIsMultiConcatWithNCount) {
  const hldb::Module *const m = getModule("socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(m, "DReqDepth");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'DReqDepth' not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'{N{4'h2}}': default RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiMultiConcatOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_GE(rhs->getOperands()->size(), 1u);

  const hldb::RefObj *const count = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(count, nullptr) << "'{N{...}}': repeat-count operand must be a RefObj (reference to N)";
  EXPECT_EQ(count->getName(), "N");
}

// 'parameter bit [7:0] AA = DReqDepth * 2;'
TEST_F(ElabCParamTest, Socket1n_AAIsDReqDepthTimesTwo) {
  const hldb::Module *const m = getModule("socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const aa = findParam(m, "AA");
  ASSERT_NE(aa, nullptr) << "'parameter bit [7:0] AA' not found";
  EXPECT_FALSE(aa->getLocalParam());

  const hldb::ParamAssign *const pa = findParamAssign(m, "AA");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'DReqDepth * 2': RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiMultOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(lhs, nullptr) << "'DReqDepth * 2': left operand must be RefObj";
  EXPECT_EQ(lhs->getName(), "DReqDepth");
}

// 'parameter bit [7:0] BB = DReqDepth[1*4+:4];' -- indexed part-select
// (IEEE 1800-2023 11.5.1).
TEST_F(ElabCParamTest, Socket1n_BBIsIndexedPartSelectOfDReqDepth) {
  const hldb::Module *const m = getModule("socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const bb = findParam(m, "BB");
  ASSERT_NE(bb, nullptr) << "'parameter bit [7:0] BB' not found";
  EXPECT_FALSE(bb->getLocalParam());

  const hldb::ParamAssign *const pa = findParamAssign(m, "BB");
  ASSERT_NE(pa, nullptr);
  const hldb::IndexedPartSelect *const rhs = pa->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(rhs, nullptr) << "'DReqDepth[1*4+:4]': RHS must be an IndexedPartSelect";
  EXPECT_EQ(rhs->getName(), "DReqDepth");
  EXPECT_EQ(rhs->getIndexedPartSelectType(), vpiPosIndexed) << "'+:' must produce vpiPosIndexed";
}

// Generate-for propagating fifo_d's ReqDepth from a per-iteration
// part-select of DReqDepth (IEEE 1800-2023 27.4 "Generate-loop
// constructs").
TEST_F(ElabCParamTest, Socket1n_GenerateForInstantiatesFifoSync) {
  const hldb::Module *const m = getModule("socket_1n");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr) << "socket_1n has no generate statements";

  const hldb::RefInstance *fifoD = nullptr;
  for (const hldb::Any *const stmt : *m->getGenStmts()) {
    const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt);
    if (region == nullptr) continue;
    const hldb::GenFor *const genFor = region->getStmt<hldb::GenFor>();
    if (genFor == nullptr) continue;
    const hldb::Begin *const body = genFor->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr) continue;
    EXPECT_EQ(body->getName(), "gen_dfifo");
    for (const hldb::Any *const s : *body->getStmts()) {
      const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(s);
      if (ri != nullptr && ri->getName() == "fifo_d") {
        fifoD = ri;
        break;
      }
    }
  }
  ASSERT_NE(fifoD, nullptr) << "'fifo_sync #(...) fifo_d ()' not found inside socket_1n's generate-for body";
  ASSERT_NE(fifoD->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = fifoD->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "fifo_d's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("fifo_sync"));

  const hldb::ParamAssign *const pa = findParamAssign(fifoD, "ReqDepth");
  ASSERT_NE(pa, nullptr) << "'.ReqDepth(DReqDepth[i*4+:4])' override not found on 'fifo_d'";
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());
  const hldb::IndexedPartSelect *const rhs = pa->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(rhs, nullptr) << "'.ReqDepth(DReqDepth[i*4+:4])': RHS must be an IndexedPartSelect";
  EXPECT_EQ(rhs->getName(), "DReqDepth");
}

// ---------------------------------------------------------------------------
// all_zero: same shape as socket_1n but with DReqDepth defaulting to
// {2{4'h0}} -- confirms the replication default is per-module, not shared.
// ---------------------------------------------------------------------------

TEST_F(ElabCParamTest, AllZero_DReqDepthDefaultIsMultiConcatWithConstantCount) {
  const hldb::Module *const m = getModule("all_zero");
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(m, "DReqDepth");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'DReqDepth' not found on 'all_zero'";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'{2{4'h0}}': default RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiMultiConcatOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_GE(rhs->getOperands()->size(), 1u);

  const hldb::Constant *const count = any_cast<hldb::Constant>((*rhs->getOperands())[0]);
  ASSERT_NE(count, nullptr) << "'{2{...}}': repeat-count operand must be a Constant";
  EXPECT_EQ(std::string(count->getDecompile()), "2");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
