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

// Spec-based validation of IEEE 1800-2017 Sec 6.20.2.1 unbounded parameter '$'.
// SV: tests/Google/chapter-6/6.20.2.1--parameter_unbounded.sv
//
//   parameter p = $;
//   wire clk = 0;
//   wire [31:0] a;
//   always @(posedge clk) a[0:p] = 23;
//
// -- Sec 6.20.2.1 construct under test ----
//
// The '$' token used as a parameter value represents an unbounded integer.
// It is commonly used as an upper bound in parameterised width expressions
// (e.g. queue sizes, part-select ranges). Surelog represents the unbounded
// value as a Constant with vpiConstType = vpiUnbounded (11) and
// vpiDecompile = "$".
//
// -- UHDM tree (from log) ----
//
//   Module name:top
//   |-- vpiParameter (1 item)
//   |   `-- Parameter name:p
//   |       `-- vpiTypespec  RefTypespec -> LogicTypespec
//   |-- vpiParamAssign (1 item)
//   |   `-- ParamAssign
//   |       |-- vpiLhs  RefObj name:p -> actual: Parameter name:p
//   |       `-- vpiRhs  Constant
//   |           |-- vpiTypespec  RefTypespec -> StringTypespec
//   |           |-- vpiConstType: unbounded (11)
//   |           `-- vpiDecompile: "$"
//   |-- vpiNet (2 items)
//   |   |-- Net name:clk  vpiNetType:wire(1)
//   |   |   `-- vpiValue  Constant("0", uIntConst=9)
//   |   `-- Net name:a    vpiNetType:wire(1)
//   |       `-- vpiTypespec  LogicTypespec  vpiRange [31:0]
//   `-- vpiProcess (1 item)
//       `-- Always  vpiAlwaysType:always(1)
//           `-- vpiStmt  EventControl
//               |-- vpiCondition  Operation { posedgeOp(39) }
//               |   `-- operands[0]  RefObj("clk")
//               `-- vpiStmt  Assignment (blocking)
//                   |-- vpiLhs  PartSelect name:a[0:p]
//                   |   |-- vpiPrefix  RefObj name:a -> Net name:a
//                   |   `-- vpiRange  Range
//                   |       |-- vpiLeftRange   Constant("0", uIntConst=9)
//                   |       `-- vpiRightRange  RefObj name:p -> Parameter name:p
//                   `-- vpiRhs  Constant("23", uIntConst=9)
//
// -- VPI constants ----
//   vpiPosedgeOp  = 39
//   vpiUIntConst  =  9
//   vpiUnbounded  = 11   (the '$' unbounded value)
//   vpiWire       =  1   (net type)
//   vpiAlways     =  1   (always type)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

#include <string>

namespace hlc {

class ParameterUnboundedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.2.1--parameter_unbounded.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Helpers
// ----

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Net *getNet(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>(name, m->getNets());
}

static const hldb::Parameter *getParam(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::Parameter>("p", m->getParameters());
}

static const hldb::ParamAssign *getParamAssign(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParamAssigns() || m->getParamAssigns()->empty()) return nullptr;
  return (*m->getParamAssigns())[0];
}

static const hldb::Always *getAlways(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  return any_cast<const hldb::Always *>((*m->getProcesses())[0]);
}

static const hldb::EventControl *getEventControl(const hldb::Design *d) {
  const auto *al = getAlways(d);
  if (!al) return nullptr;
  return al->getStmt<hldb::EventControl>();
}

static const hldb::Assignment *getAssignment(const hldb::Design *d) {
  const auto *ec = getEventControl(d);
  if (!ec) return nullptr;
  return ec->getStmt<hldb::Assignment>();
}

static const hldb::PartSelect *getPartSelect(const hldb::Design *d) {
  const auto *asgn = getAssignment(d);
  if (!asgn) return nullptr;
  return asgn->getLhs<hldb::PartSelect>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterUnboundedTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// Parameter -- 'parameter p = $'
// ===========================================================================

TEST_F(ParameterUnboundedTest, Param_Collection_HasOneEntry) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "one parameter declaration ('p') expected";
}

TEST_F(ParameterUnboundedTest, Param_p_Exists) {
  EXPECT_NE(getParam(m_design), nullptr) << "Parameter named 'p' not found";
}

TEST_F(ParameterUnboundedTest, Param_p_HasLogicTypespec) {
  const auto *p = getParam(m_design);
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  EXPECT_NE(p->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'parameter p' with no explicit type must use LogicTypespec";
}

// ===========================================================================
// ParamAssign -- value assignment 'p = $'
// ===========================================================================

TEST_F(ParameterUnboundedTest, ParamAssign_Collection_HasOneEntry) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_EQ(m->getParamAssigns()->size(), 1u) << "one ParamAssign expected (for 'p = $')";
}

TEST_F(ParameterUnboundedTest, ParamAssign_Lhs_IsRefObj) {
  const auto *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getLhs<hldb::RefObj>(), nullptr) << "ParamAssign lhs must be RefObj referencing 'p'";
}

TEST_F(ParameterUnboundedTest, ParamAssign_Lhs_NameIsP) {
  const auto *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const auto *ref = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "p");
}

TEST_F(ParameterUnboundedTest, ParamAssign_Lhs_ActualIsParameter) {
  const auto *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const auto *ref = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Parameter>(), nullptr) << "ParamAssign lhs RefObj must resolve to Parameter node";
}

TEST_F(ParameterUnboundedTest, ParamAssign_Rhs_IsConstant) {
  const auto *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "ParamAssign rhs ('$') must be a Constant node";
}

TEST_F(ParameterUnboundedTest, ParamAssign_Rhs_IsUnbounded) {
  // Sec 6.20.2.1: '$' is the unbounded value; Surelog represents it as a
  // Constant with vpiConstType = vpiUnbounded (11).
  const auto *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const auto *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 11) /* vpiUnbounded */
      << "Sec 6.20.2.1: '$' must have vpiConstType = vpiUnbounded (11)";
}

TEST_F(ParameterUnboundedTest, ParamAssign_Rhs_ValueIsDollar) {
  // The decompile/string representation of the unbounded value is "$".
  const auto *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const auto *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "$") << "Sec 6.20.2.1: unbounded '$' constant must have value \"$\"";
}

// ===========================================================================
// Nets -- wire clk, wire [31:0] a
// ===========================================================================

TEST_F(ParameterUnboundedTest, Net_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 2u) << "two nets expected: 'clk' and 'a'";
}

TEST_F(ParameterUnboundedTest, Net_clk_Exists) { EXPECT_NE(getNet(m_design, "clk"), nullptr) << "net 'clk' not found"; }

TEST_F(ParameterUnboundedTest, Net_clk_IsWire) {
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire) << "'wire clk' must have vpiNetType = vpiWire (1)";
}

TEST_F(ParameterUnboundedTest, Net_clk_HasLogicTypespec) {
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'wire clk' must have a LogicTypespec";
}

TEST_F(ParameterUnboundedTest, Net_clk_InitialValueIsZero) {
  // 'wire clk = 0' -- Surelog stores the initializer as vpiValue Constant.
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr);
  const auto *c = net->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'wire clk = 0' must have a Constant value";
  EXPECT_EQ(std::string(c->getValue()), "0") << "'wire clk = 0' initial value must be 0";
}

TEST_F(ParameterUnboundedTest, Net_clk_InitialValue_IsUnsignedInt) {
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr);
  const auto *c = net->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst) << "literal '0' in 'wire clk = 0' must be vpiUIntConst (9)";
}

// IEEE 1800-2023 Sec 6.7/6.8: 'clk' has the net-type keyword `wire`, so it
// must not also appear in the module's Variable collection.
TEST_F(ParameterUnboundedTest, Net_clk_IsNotInVariables) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getVariables() == nullptr || hldb::findByName<hldb::Variable>("clk", m->getVariables()) == nullptr)
      << "'clk' is declared with net-type 'wire'; it must not appear in the module's Variable collection";
}

TEST_F(ParameterUnboundedTest, Net_a_Exists) { EXPECT_NE(getNet(m_design, "a"), nullptr) << "net 'a' not found"; }

TEST_F(ParameterUnboundedTest, Net_a_IsWire) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire) << "'wire [31:0] a' must have vpiNetType = vpiWire (1)";
}

TEST_F(ParameterUnboundedTest, Net_a_HasLogicTypespec) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'wire [31:0] a' must have a LogicTypespec";
}

TEST_F(ParameterUnboundedTest, Net_a_Typespec_HasPackedRange) {
  // 'wire [31:0] a' -- the LogicTypespec must have a Range [31:0].
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *lts = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  EXPECT_EQ(lts->getRanges()->size(), 1u) << "'wire [31:0] a' typespec must have exactly one packed range";
}

TEST_F(ParameterUnboundedTest, Net_a_Typespec_Range_LeftIs31) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  const auto *lts = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  const auto *range = (*lts->getRanges())[0];
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getValue()), "31") << "'wire [31:0] a' left bound must be 31";
}

// IEEE 1800-2023 Sec 6.7/6.8: 'a' has the net-type keyword `wire`, so it must
// not also appear in the module's Variable collection.
TEST_F(ParameterUnboundedTest, Net_a_IsNotInVariables) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getVariables() == nullptr || hldb::findByName<hldb::Variable>("a", m->getVariables()) == nullptr)
      << "'a' is declared with net-type 'wire'; it must not appear in the module's Variable collection";
}

TEST_F(ParameterUnboundedTest, Net_a_Typespec_Range_RightIsZero) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  const auto *lts = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  const auto *range = (*lts->getRanges())[0];
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getValue()), "0") << "'wire [31:0] a' right bound must be 0";
}

// ===========================================================================
// Always process -- 'always @(posedge clk) a[0:p] = 23'
// ===========================================================================

TEST_F(ParameterUnboundedTest, Process_Collection_HasOneEntry) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  EXPECT_EQ(m->getProcesses()->size(), 1u) << "one process (the always block) expected";
}

TEST_F(ParameterUnboundedTest, Process_IsAlways) {
  EXPECT_NE(getAlways(m_design), nullptr) << "process must be an Always node";
}

TEST_F(ParameterUnboundedTest, Always_Type_IsAlways) {
  const auto *al = getAlways(m_design);
  ASSERT_NE(al, nullptr);
  EXPECT_EQ(al->getAlwaysType(), vpiAlways) << "'always' keyword must produce vpiAlwaysType = vpiAlways (1)";
}

TEST_F(ParameterUnboundedTest, Always_Stmt_IsEventControl) {
  EXPECT_NE(getEventControl(m_design), nullptr) << "always stmt must be an EventControl for '@(posedge clk)'";
}

// ===========================================================================
// EventControl -- '@(posedge clk)'
// ===========================================================================

TEST_F(ParameterUnboundedTest, EventControl_Condition_IsPosedge) {
  const auto *ec = getEventControl(m_design);
  ASSERT_NE(ec, nullptr);
  const auto *op = ec->getCondition<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp) << "'@(posedge clk)' must use vpiPosedgeOp (39)";
}

TEST_F(ParameterUnboundedTest, EventControl_Condition_OperandIsClk) {
  const auto *ec = getEventControl(m_design);
  ASSERT_NE(ec, nullptr);
  const auto *op = ec->getCondition<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

TEST_F(ParameterUnboundedTest, EventControl_Stmt_IsAssignment) {
  EXPECT_NE(getAssignment(m_design), nullptr) << "EventControl stmt must be an Assignment";
}

// ===========================================================================
// Assignment -- 'a[0:p] = 23' (blocking)
// ===========================================================================

TEST_F(ParameterUnboundedTest, Assignment_IsBlocking) {
  const auto *asgn = getAssignment(m_design);
  ASSERT_NE(asgn, nullptr);
  EXPECT_TRUE(asgn->getBlocking()) << "'a[0:p] = 23' is a blocking assignment (no '<=' operator)";
}

TEST_F(ParameterUnboundedTest, Assignment_Lhs_IsPartSelect) {
  EXPECT_NE(getPartSelect(m_design), nullptr) << "assignment lhs must be a PartSelect for 'a[0:p]'";
}

TEST_F(ParameterUnboundedTest, Assignment_Lhs_NameIsASlice) {
  const auto *ps = getPartSelect(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(ps->getName(), "a[0:p]") << "PartSelect name must be 'a[0:p]'";
}

// ===========================================================================
// PartSelect -- 'a[0:p]'
// ===========================================================================

TEST_F(ParameterUnboundedTest, PartSelect_Prefix_IsRefObjA) {
  const auto *ps = getPartSelect(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *ref = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(ref, nullptr) << "PartSelect prefix must be RefObj('a')";
  EXPECT_EQ(ref->getName(), "a");
}

TEST_F(ParameterUnboundedTest, PartSelect_Prefix_ActualIsNet) {
  const auto *ps = getPartSelect(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *ref = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Net>(), nullptr) << "PartSelect prefix RefObj must resolve to Net 'a'";
}

TEST_F(ParameterUnboundedTest, PartSelect_Range_LeftIsZero) {
  // 'a[0:p]' -- left bound is the constant 0.
  const auto *ps = getPartSelect(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *range = ps->getRange();
  ASSERT_NE(range, nullptr) << "PartSelect must have a Range for '[0:p]'";
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getValue()), "0") << "'a[0:p]' left bound must be Constant 0";
}

TEST_F(ParameterUnboundedTest, PartSelect_Range_RightIsRefObjP) {
  // 'a[0:p]' -- right bound is the parameter 'p' (a RefObj, not a Constant).
  const auto *ps = getPartSelect(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *range = ps->getRange();
  ASSERT_NE(range, nullptr);
  const auto *ref = range->getRightExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr) << "'a[0:p]' right bound must be RefObj('p'), not a Constant";
  EXPECT_EQ(ref->getName(), "p");
}

TEST_F(ParameterUnboundedTest, PartSelect_Range_RightRefObj_ActualIsParameter) {
  // The RefObj 'p' in the range must resolve to the Parameter node.
  const auto *ps = getPartSelect(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *range = ps->getRange();
  ASSERT_NE(range, nullptr);
  const auto *ref = range->getRightExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Parameter>(), nullptr)
      << "RefObj 'p' in part-select range must resolve to Parameter 'p'";
}

// ===========================================================================
// Assignment RHS -- '23'
// ===========================================================================

TEST_F(ParameterUnboundedTest, Assignment_Rhs_IsConstant23) {
  const auto *asgn = getAssignment(m_design);
  ASSERT_NE(asgn, nullptr);
  const auto *c = asgn->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "assignment rhs must be a Constant";
  EXPECT_EQ(std::string(c->getValue()), "23") << "'a[0:p] = 23' rhs must be Constant 23";
}

TEST_F(ParameterUnboundedTest, Assignment_Rhs_IsUnsignedInt) {
  const auto *asgn = getAssignment(m_design);
  ASSERT_NE(asgn, nullptr);
  const auto *c = asgn->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst) << "literal '23' must be vpiUIntConst (9)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
