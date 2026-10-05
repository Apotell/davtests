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

// Tests for tests/LoopBits/dut.sv (tags: LoopBits)
//    1 module Module;
//    2     parameter S = 0;
//    3     parameter T = 0;
//    4     wire [T-1:0] x;
//    5     generate
//    6         if (S) begin : a
//    7             if (S) begin : b
//    8                 assign Module.x = 1'sb1;
//    9                 wire [T:0] y = 1'sbz;
//   10             end
//   11         end
//   12     endgenerate
//   13     initial $display("Module %0d: %b %0d %b %0d", S, Module.x, T, Module.a.b.y, T + 1);
//   14 endmodule
//   16 module top;
//   17     parameter ONE = 1;
//   18     wire [ONE*7:ONE*0] x;
//   19     if (1) begin : blk
//   20         localparam W = ONE * 3;
//   21         wire [W-1:$bits(x)] x;
//   22     end
//   23     Module #(1, 8) m1();
//   24     Module #(2, 7) m2();
//   25     Module #(3, 3) m3();
//   26 endmodule
//
// The construct under test is a net whose packed range depends on $bits()
// of a same-named net ("wire [W-1:$bits(x)] x;" inside generate block blk
// shadows top.x), plus nets declared inside nested named generate blocks
// that are referenced hierarchically from outside them.
//
// What is checked (IEEE 1800-2023):
//   - 27.3 generate region: Module's "generate ... endgenerate" wraps the
//     outer if-generate; 27.5 conditional generate: nested if-generates with
//     named generate blocks "a" and "b"; top has an if-generate with named
//     block "blk".
//   - 27.2 "Parameters declared in generate blocks shall be treated as
//     localparams": W is a localparam in blk.
//   - 23.9 "The following elements define a new scope ... Generate blocks":
//     y is declared in (and owned by) block b; blk's inner x is declared in
//     (and owned by) block blk. Neither is a module-level net.
//   - 6.7 net declaration assignment: "wire [T:0] y = 1'sbz;" is a net
//     declaration assignment (Net::getNetDeclAssign() true, value 1'sbz);
//     "wire [T-1:0] x;" and "wire [ONE*7:ONE*0] x;" have no "= expr" and are
//     not.
//   - 6.5 "Data shall be declared before they are used": inside blk's
//     declaration "wire [W-1:$bits(x)] x;" the packed dimension is part of
//     the declaration of the inner x, so the "x" in "$bits(x)" cannot refer
//     to the inner x (not yet declared) and resolves upward (23.9) to
//     top.x. Range left is "W-1" (vpiSubOp), right is a $bits() call.
//   - 23.8 upwards name referencing / 23.6 hierarchical names:
//     "Module.x" (line 8, line 13) binds to Module's net x; "Module.a.b.y"
//     (line 13) names y through generate blocks a and b and must bind (every
//     instance m1/m2/m3 has S != 0, so both blocks exist).
//   - 23.10.2.1 parameter value assignment by ordered list:
//     "Module #(1, 8)" assigns 1 to S and 8 to T (declaration order), so
//     each override's LHS must name S and T respectively.
//   - HLDB integrity: a legal design must not produce an
//     HLDB_ILLEGAL_PROPERTY_VALUE diagnostic on blk (line 19).
//
// What is NOT checked and why:
//   - the elaborated widths of m1/m2/m3.x, y and blk.x (and the $display
//     output): this .hlc requests no elaboration, so per-instance parameter
//     values are not available in the folded model.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/gen_region.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LoopBitsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LoopBits.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // Module: generate region -> if (S) begin : a
  static const hldb::GenIf *getOuterGenIf() {
    const hldb::Module *const m = getModule("Module");
    if (m == nullptr || m->getGenStmts() == nullptr || m->getGenStmts()->size() != 1) return nullptr;
    const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(m->getGenStmts()->at(0));
    if (region == nullptr) return nullptr;
    return region->getStmt<hldb::GenIf>();
  }
  static const hldb::Begin *getBlockA() {
    const hldb::GenIf *const gi = getOuterGenIf();
    return (gi == nullptr) ? nullptr : gi->getStmt<hldb::Begin>();
  }
  static const hldb::GenIf *getInnerGenIf() {
    const hldb::Begin *const a = getBlockA();
    if (a == nullptr || a->getStmts() == nullptr || a->getStmts()->size() != 1) return nullptr;
    return any_cast<hldb::GenIf>(a->getStmts()->at(0));
  }
  static const hldb::Begin *getBlockB() {
    const hldb::GenIf *const gi = getInnerGenIf();
    return (gi == nullptr) ? nullptr : gi->getStmt<hldb::Begin>();
  }
  // top: if (1) begin : blk
  static const hldb::Begin *getBlockBlk() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr || top->getGenStmts() == nullptr || top->getGenStmts()->size() != 1) return nullptr;
    const hldb::GenIf *const gi = any_cast<hldb::GenIf>(top->getGenStmts()->at(0));
    return (gi == nullptr) ? nullptr : gi->getStmt<hldb::Begin>();
  }
  static const hldb::SysTaskCall *getDisplay() {
    const hldb::Module *const m = getModule("Module");
    if (m == nullptr || m->getProcesses() == nullptr || m->getProcesses()->size() != 1) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(m->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::SysTaskCall>();
  }
};

// ===========================================================================
// module Module
// ===========================================================================

TEST_F(LoopBitsTest, ModulesExist) {
  EXPECT_NE(getModule("Module"), nullptr);
  EXPECT_NE(getModule("top"), nullptr);
}

TEST_F(LoopBitsTest, ModuleParametersSAndTAreNonlocal) {
  const hldb::Module *const m = getModule("Module");
  ASSERT_NE(m, nullptr);
  for (std::string_view name : {"S", "T"}) {
    const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>(name, m->getParameters());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_FALSE(p->getLocalParam()) << name << ": 6.20.4 module body parameter without a parameter_port_list";
  }
}

// 6.7: "wire [T-1:0] x;" has no net declaration assignment.
TEST_F(LoopBitsTest, ModuleNetXIsPlainWireVector) {
  const hldb::Module *const m = getModule("Module");
  ASSERT_NE(m, nullptr);
  const hldb::Net *const x = hldb::findByName<hldb::Net>("x", m->getNets());
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getNetType(), vpiWire);
  EXPECT_TRUE(x->getVector());
  EXPECT_FALSE(x->getNetDeclAssign()) << "6.7: 'wire [T-1:0] x;' has no '= expr'";
  EXPECT_EQ(x->getValue(), nullptr);
  ASSERT_NE(x->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = x->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "left bound 'T-1'";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
}

// 27.3 / 27.5: generate region -> if (S) begin : a -> if (S) begin : b
TEST_F(LoopBitsTest, NestedNamedGenerateBlocks) {
  const hldb::GenIf *const outer = getOuterGenIf();
  ASSERT_NE(outer, nullptr) << "generate region should contain the outer if-generate";
  const hldb::RefObj *const c1 = outer->getCondition<hldb::RefObj>();
  ASSERT_NE(c1, nullptr);
  EXPECT_EQ(c1->getName(), "S");
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");

  const hldb::GenIf *const inner = getInnerGenIf();
  ASSERT_NE(inner, nullptr) << "block a should contain exactly the inner if-generate";
  const hldb::RefObj *const c2 = inner->getCondition<hldb::RefObj>();
  ASSERT_NE(c2, nullptr);
  EXPECT_EQ(c2->getName(), "S");
  const hldb::Begin *const b = getBlockB();
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "b");
}

// 23.9: y is declared in generate block b, so b owns it.
TEST_F(LoopBitsTest, NetYIsDeclaredInBlockB) {
  const hldb::Begin *const b = getBlockB();
  ASSERT_NE(b, nullptr);
  const hldb::Net *const y = hldb::findByName<hldb::Net>("y", b->getNets());
  ASSERT_NE(y, nullptr) << "23.9: generate block 'b' is a scope; 'wire [T:0] y' belongs to it";
}

TEST_F(LoopBitsTest, NetYIsNotAModuleLevelNet) {
  const hldb::Module *const m = getModule("Module");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("y", m->getNets()), nullptr)
      << "23.9: 'y' is declared in generate block a.b, not directly in module 'Module'";
}

// 6.7: "wire [T:0] y = 1'sbz;" is a net declaration assignment.
TEST_F(LoopBitsTest, NetYHasDeclAssignOfSignedZ) {
  const hldb::Begin *const b = getBlockB();
  ASSERT_NE(b, nullptr);
  const hldb::Net *const y = hldb::findByName<hldb::Net>("y", b->getNets());
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(y->getNetType(), vpiWire);
  EXPECT_TRUE(y->getNetDeclAssign());
  const hldb::Constant *const v = y->getValue<hldb::Constant>();
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getDecompile(), "1'sbz");
  EXPECT_EQ(v->getConstType(), vpiBinaryConst);
  EXPECT_EQ(v->getSize(), 1);
}

// 23.8: "assign Module.x = 1'sb1;" inside block b targets Module's x.
TEST_F(LoopBitsTest, HierarchicalContAssignInBlockBBindsToModuleX) {
  const hldb::Begin *const b = getBlockB();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  const hldb::ContAssign *ca = nullptr;
  for (const hldb::Any *const s : *b->getStmts()) {
    if ((ca = any_cast<hldb::ContAssign>(s)) != nullptr) break;
  }
  ASSERT_NE(ca, nullptr) << "block b should contain 'assign Module.x = 1'sb1;'";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "Module.x");
  const hldb::Module *const m = getModule("Module");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Net>("x", m->getNets()));
  const hldb::Constant *const rhs = ca->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1'sb1");
}

// 23.6 / 23.8: "Module.a.b.y" in the $display binds through the blocks.
TEST_F(LoopBitsTest, DisplayHierarchicalRefModuleABYBinds) {
  const hldb::SysTaskCall *const d = getDisplay();
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->getName(), "$display");
  ASSERT_NE(d->getArguments(), nullptr);
  ASSERT_EQ(d->getArguments()->size(), 6u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(d->getArguments()->at(4));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "Module.a.b.y");
  ASSERT_NE(ref->getActual(), nullptr) << "23.6: 'Module.a.b.y' names net y in generate block a.b";
  EXPECT_EQ(ref->getActual()->getAnyType(), hldb::AnyType::Net);
  EXPECT_EQ(ref->getActual()->getName(), "y");
}

TEST_F(LoopBitsTest, DisplayHierarchicalRefModuleXBinds) {
  const hldb::SysTaskCall *const d = getDisplay();
  ASSERT_NE(d, nullptr);
  ASSERT_NE(d->getArguments(), nullptr);
  ASSERT_EQ(d->getArguments()->size(), 6u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(d->getArguments()->at(2));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "Module.x");
  const hldb::Module *const m = getModule("Module");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(ref->getActual(), nullptr);
  EXPECT_EQ(ref->getActual(), hldb::findByName<hldb::Net>("x", m->getNets()));
}

TEST_F(LoopBitsTest, NoBindingFailuresForGenerateBlockPath) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "a", 13), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "b", 13), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "y", 13), nullptr);
}

// ===========================================================================
// module top
// ===========================================================================

TEST_F(LoopBitsTest, TopNetXIsPlainWireVector) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Net *const x = hldb::findByName<hldb::Net>("x", top->getNets());
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getNetType(), vpiWire);
  EXPECT_TRUE(x->getVector());
  EXPECT_FALSE(x->getNetDeclAssign()) << "6.7: 'wire [ONE*7:ONE*0] x;' has no '= expr'";
  ASSERT_NE(x->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = x->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  const hldb::Operation *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getOpType(), vpiMultOp);
  EXPECT_EQ(right->getOpType(), vpiMultOp);
}

// 27.2: parameters in generate blocks are localparams.
TEST_F(LoopBitsTest, BlkHasLocalparamW) {
  const hldb::Begin *const blk = getBlockBlk();
  ASSERT_NE(blk, nullptr);
  EXPECT_EQ(blk->getName(), "blk");
  const hldb::Parameter *const w = hldb::findByName<hldb::Parameter>("W", blk->getParameters());
  ASSERT_NE(w, nullptr);
  EXPECT_TRUE(w->getLocalParam());
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("W", blk->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiMultOp);
}

// 23.9: blk's inner x is a net of block blk.
TEST_F(LoopBitsTest, BlkDeclaresInnerNetX) {
  const hldb::Begin *const blk = getBlockBlk();
  ASSERT_NE(blk, nullptr);
  const hldb::Net *const x = hldb::findByName<hldb::Net>("x", blk->getNets());
  ASSERT_NE(x, nullptr) << "'wire [W-1:$bits(x)] x;' must be modeled as a net of generate block blk";
  EXPECT_EQ(x->getNetType(), vpiWire);
  EXPECT_FALSE(x->getNetDeclAssign());
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  EXPECT_NE(x, hldb::findByName<hldb::Net>("x", top->getNets())) << "inner x shadows, it is not top.x";
}

// 6.5 / 23.9: range [W-1:$bits(x)], "x" inside $bits refers to top.x.
TEST_F(LoopBitsTest, BlkInnerXRangeUsesBitsOfOuterX) {
  const hldb::Begin *const blk = getBlockBlk();
  ASSERT_NE(blk, nullptr);
  const hldb::Net *const x = hldb::findByName<hldb::Net>("x", blk->getNets());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(x->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = x->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);

  const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "left bound 'W-1'";
  EXPECT_EQ(left->getOpType(), vpiSubOp);

  const hldb::SysFuncCall *const bits = r->getRightExpr<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr) << "right bound '$bits(x)'";
  EXPECT_EQ(bits->getName(), "$bits");
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "x");
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(arg->getActual(), nullptr);
  EXPECT_EQ(arg->getActual(), hldb::findByName<hldb::Net>("x", top->getNets()))
      << "6.5: the inner x is not yet declared inside its own packed dimension; 'x' resolves to top.x";
}

// 23.10.2.1: ordered-list overrides map to S then T.
TEST_F(LoopBitsTest, OrderedOverridesMapToSAndT) {
  const hldb::Module *const top = getModule("top");
  const hldb::Module *const m = getModule("Module");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const s = hldb::findByName<hldb::Parameter>("S", m->getParameters());
  const hldb::Parameter *const t = hldb::findByName<hldb::Parameter>("T", m->getParameters());
  ASSERT_NE(s, nullptr);
  ASSERT_NE(t, nullptr);

  struct Expected {
    std::string_view m_inst;
    std::string_view m_s;
    std::string_view m_t;
  };
  const Expected expected[] = {{"m1", "1", "8"}, {"m2", "2", "7"}, {"m3", "3", "3"}};
  for (const Expected &e : expected) {
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(e.m_inst, top->getRefInstances());
    ASSERT_NE(inst, nullptr) << e.m_inst;
    ASSERT_NE(inst->getTypespec(), nullptr) << e.m_inst;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mt, nullptr) << e.m_inst;
    EXPECT_EQ(mt->getModule(), m) << e.m_inst;
    ASSERT_NE(mt->getParamAssigns(), nullptr) << e.m_inst;
    ASSERT_EQ(mt->getParamAssigns()->size(), 2u) << e.m_inst;

    const hldb::ParamAssign *const pa0 = mt->getParamAssigns()->at(0);
    const hldb::ParamAssign *const pa1 = mt->getParamAssigns()->at(1);
    ASSERT_NE(pa0, nullptr);
    ASSERT_NE(pa1, nullptr);
    EXPECT_FALSE(pa0->getConnByName()) << e.m_inst;
    EXPECT_TRUE(pa0->getOverridden()) << e.m_inst;

    const hldb::Constant *const v0 = pa0->getRhs<hldb::Constant>();
    const hldb::Constant *const v1 = pa1->getRhs<hldb::Constant>();
    ASSERT_NE(v0, nullptr);
    ASSERT_NE(v1, nullptr);
    EXPECT_EQ(v0->getDecompile(), e.m_s) << e.m_inst;
    EXPECT_EQ(v1->getDecompile(), e.m_t) << e.m_inst;

    const hldb::RefObj *const l0 = pa0->getLhs<hldb::RefObj>();
    const hldb::RefObj *const l1 = pa1->getLhs<hldb::RefObj>();
    ASSERT_NE(l0, nullptr) << e.m_inst << ": 23.10.2.1 first ordered value is assigned to S";
    ASSERT_NE(l1, nullptr) << e.m_inst << ": 23.10.2.1 second ordered value is assigned to T";
    EXPECT_EQ(l0->getName(), "S");
    EXPECT_EQ(l1->getName(), "T");
    EXPECT_EQ(l0->getActual(), s);
    EXPECT_EQ(l1->getActual(), t);
  }
}

TEST_F(LoopBitsTest, NoIllegalHldbPropertyOnBlk) {
  EXPECT_EQ(findError(ErrorDefinition::HLDB_ILLEGAL_PROPERTY_VALUE, 19), nullptr)
      << "legal input must not produce an invalid HLDB model for generate block blk";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
