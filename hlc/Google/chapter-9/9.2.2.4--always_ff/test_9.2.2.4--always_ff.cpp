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

// Tests for 9.2.2.4--always_ff.sv (tags: 9.2.2.4)
//   module always_tb ();
//     wire a = 0;
//     wire b = 0;
//     reg q = 0;
//     always_ff @(posedge a)
//       q <= b;
//   endmodule
//
// IEEE 1800-2017 Sec 9.2.2.4 "always_ff procedure": another refined always
// form (see 9.2.2.2--always_comb.sv / 9.2.2.3--always_latch.sv) -- its
// Always node's AlwaysType must be exactly vpiAlwaysFF.
//
// "wire a = 0" / "wire b = 0" / "reg q = 0" -- identical declarations and
// reasoning to 9.2.2.3--always_latch.sv: "a"/"b" are Nets (1-bit unsigned
// LogicTypespec, no declared Ranges, net_decl_assignment value via
// getValue() -> Constant "0", vpiUIntConst); "q" is a Variable (same
// typespec shape, plain-value "0" initializer -> vpiUIntConst).
// getNetDeclAssign() is asserted true per Sec 6.7.1 but is a CONFIRMED
// compiler bug (never set, across every net-declaration-with-initializer
// shape checked so far: plain scalar, vector with sized/based initializer,
// multi-net declarations, and multiple net types) -- see
// 9.2.2.2--always_comb.cpp for the original cross-checked evidence;
// asserted anyway, intentionally failing.
//
// "@(posedge a) q <= b;" -- Sec 9.4.2 "Event control": an event_control
// statement. The EventControl node holds the event expression directly as
// getCondition() and the guarded statement as getStmt(), both without an
// enclosing Begin since there is no begin/end in source. "posedge a" (Sec
// 9.4.2.1/Table 9.1's edge_identifier) is modeled as a unary
// Operation(vpiPosedgeOp) with 1 operand, a RefObj resolving to Net "a" --
// the same unary-operator-as-Operation shape already established for "~a"
// (vpiBitNegOp) in 9.2.2.1/9.2.2.2.
//
// "q <= b" uses the non-blocking assignment operator "<=", which Sec
// 9.2.2.4 requires be used exclusively within always_ff (unlike
// always_latch/always_comb, where blocking is only a style
// recommendation) -- getBlocking() must be false.
//
// Checked:
//   - design has module "always_tb" with exactly 2 nets ("a", "b") and 1
//     variable ("q") -- same shape checks as 9.2.2.3--always_latch.cpp
//   - module has exactly 1 process, and it is an Always whose AlwaysType
//     is exactly vpiAlwaysFF
//   - the Always' body is directly an EventControl (no enclosing Begin)
//   - the EventControl's condition is an Operation(vpiPosedgeOp) with 1
//     operand, a RefObj resolving to Net "a"; its guarded stmt is directly
//     an Assignment (no enclosing Begin)
//   - that Assignment is non-blocking ("<="); lhs is a RefObj resolving to
//     Variable "q"; rhs is a RefObj resolving to Net "b"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AlwaysFfTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.2.2.4--always_ff.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("always_tb", m_design->getAllModules());
  }

  static const hldb::Net *getNet(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>(name, mod->getNets());
  }

  static const hldb::LogicTypespec *getNetTypespec(std::string_view name) {
    const hldb::Net *const n = getNet(name);
    if (n == nullptr || n->getTypespec() == nullptr) return nullptr;
    return n->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Variable *getVariableQ() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("q", mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableQTypespec() {
    const hldb::Variable *const q = getVariableQ();
    if (q == nullptr || q->getTypespec() == nullptr) return nullptr;
    return q->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::EventControl *getEventControl() {
    const hldb::Always *const alw = getAlwaysProcess();
    if (alw == nullptr) return nullptr;
    return alw->getStmt<hldb::EventControl>();
  }

  static const hldb::Operation *getPosedgeOperation() {
    const hldb::EventControl *const ec = getEventControl();
    if (ec == nullptr) return nullptr;
    return ec->getCondition<hldb::Operation>();
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::EventControl *const ec = getEventControl();
    if (ec == nullptr) return nullptr;
    return ec->getStmt<hldb::Assignment>();
  }
};

// --- nets "a" and "b" (wire) --------------------------------------------------

TEST_F(AlwaysFfTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(AlwaysFfTest, ModuleHasTwoNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 2u);
}

TEST_F(AlwaysFfTest, NetAExists) { EXPECT_NE(getNet("a"), nullptr); }

TEST_F(AlwaysFfTest, NetBExists) { EXPECT_NE(getNet("b"), nullptr); }

// "wire a"/"wire b" have a net-type keyword, so per IEEE 1800-2023 Sec
// 6.7/6.8 neither must also appear in the module's variable collection.
TEST_F(AlwaysFfTest, NetsAreNotDuplicatedAsVariables) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getVariables() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>("a", mod->getVariables()), nullptr)
        << "'wire a' has a net-type keyword and must not also appear as a Variable";
    EXPECT_EQ(hldb::findByName<hldb::Variable>("b", mod->getVariables()), nullptr)
        << "'wire b' has a net-type keyword and must not also appear as a Variable";
  }
}

TEST_F(AlwaysFfTest, NetANetTypeIsWire) {
  const hldb::Net *const a = getNet("a");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire) << "'wire a' must have net type vpiWire";
}

TEST_F(AlwaysFfTest, NetBNetTypeIsWire) {
  const hldb::Net *const b = getNet("b");
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getNetType(), vpiWire) << "'wire b' must have net type vpiWire";
}

TEST_F(AlwaysFfTest, NetATypespecIsLogicTypespec) { EXPECT_NE(getNetTypespec("a"), nullptr); }

TEST_F(AlwaysFfTest, NetBTypespecIsLogicTypespec) { EXPECT_NE(getNetTypespec("b"), nullptr); }

TEST_F(AlwaysFfTest, NetATypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getNetTypespec("a");
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.7.2: an undecorated 'wire' defaults to unsigned";
}

TEST_F(AlwaysFfTest, NetBTypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getNetTypespec("b");
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.7.2: an undecorated 'wire' defaults to unsigned";
}

TEST_F(AlwaysFfTest, NetATypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getNetTypespec("a");
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'wire a' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(AlwaysFfTest, NetBTypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getNetTypespec("b");
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'wire b' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(AlwaysFfTest, NetsAreMarkedAsNetDeclAssign) {
  const hldb::Net *const a = getNet("a");
  const hldb::Net *const b = getNet("b");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(a->getNetDeclAssign()) << "6.7.1: 'wire a = 0' is a net_decl_assignment";
  EXPECT_TRUE(b->getNetDeclAssign()) << "6.7.1: 'wire b = 0' is a net_decl_assignment";
}

TEST_F(AlwaysFfTest, NetAValueIsConstantZero) {
  const hldb::Net *const a = getNet("a");
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const val = a->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'wire a = 0' must carry its net_decl_assignment value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

TEST_F(AlwaysFfTest, NetBValueIsConstantZero) {
  const hldb::Net *const b = getNet("b");
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const val = b->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'wire b = 0' must carry its net_decl_assignment value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- variable "q" (reg) --------------------------------------------------------

TEST_F(AlwaysFfTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
}

TEST_F(AlwaysFfTest, VariableQExists) { EXPECT_NE(getVariableQ(), nullptr); }

// "reg q" has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// must not also appear in the module's net collection.
TEST_F(AlwaysFfTest, VariableQIsNotDuplicatedAsNet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("q", mod->getNets()), nullptr)
        << "'reg q' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(AlwaysFfTest, VariableQTypespecIsLogicTypespec) { EXPECT_NE(getVariableQTypespec(), nullptr); }

TEST_F(AlwaysFfTest, VariableQTypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getVariableQTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg' with no 'signed' keyword defaults to unsigned";
}

TEST_F(AlwaysFfTest, VariableQTypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getVariableQTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'reg q' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(AlwaysFfTest, VariableQInitialValueIsConstantZero) {
  const hldb::Variable *const q = getVariableQ();
  ASSERT_NE(q, nullptr);
  const hldb::Constant *const val = q->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'reg q = 0' must carry an initial value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- always_ff process structure -----------------------------------------------

TEST_F(AlwaysFfTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(AlwaysFfTest, TheOneProcessIsAlways) { EXPECT_NE(getAlwaysProcess(), nullptr); }

TEST_F(AlwaysFfTest, AlwaysTypeIsAlwaysFF) {
  const hldb::Always *const alw = getAlwaysProcess();
  ASSERT_NE(alw, nullptr);
  EXPECT_EQ(alw->getAlwaysType(), vpiAlwaysFF) << "9.2.2.4: 'always_ff' must have AlwaysType vpiAlwaysFF";
}

// 9.2.2.4: "always_ff" with no begin/end wraps a single statement_or_null
// -- the process' stmt must be the EventControl itself, not a Begin block.
TEST_F(AlwaysFfTest, AlwaysStmtIsDirectlyAnEventControl) {
  EXPECT_NE(getEventControl(), nullptr)
      << "9.4.2: a single-statement 'always_ff' body (no begin/end) must not be wrapped in a Begin block";
}

// --- @(posedge a) q <= b; -----------------------------------------------------

TEST_F(AlwaysFfTest, EventConditionIsPosedgeOperation) {
  const hldb::Operation *const cond = getPosedgeOperation();
  ASSERT_NE(cond, nullptr) << "'posedge a' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp) << "9.4.2: 'posedge' is vpiPosedgeOp";
}

TEST_F(AlwaysFfTest, PosedgeOperationHasOneOperand) {
  const hldb::Operation *const cond = getPosedgeOperation();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  EXPECT_EQ(cond->getOperands()->size(), 1u) << "'posedge a' is a unary edge qualifier with 1 operand";
}

TEST_F(AlwaysFfTest, PosedgeOperandResolvesToNetA) {
  const hldb::Operation *const cond = getPosedgeOperation();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_GE(cond->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "operand[0] of 'posedge a' should be a RefObj";
  EXPECT_EQ(operand->getName(), "a");
  EXPECT_EQ(operand->getActual<hldb::Net>(), getNet("a"));
}

// 9.4.2: with a single guarded statement (no begin/end), it is held
// directly on the EventControl, not wrapped in a Begin.
TEST_F(AlwaysFfTest, EventControlGuardedStmtIsDirectlyAnAssignment) {
  EXPECT_NE(getAssignment(), nullptr)
      << "9.4.2: a single guarded statement (no begin/end) must not be wrapped in a Begin block";
}

TEST_F(AlwaysFfTest, AssignmentIsNonBlocking) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking()) << "9.2.2.4: always_ff requires non-blocking assignment; 'q <= b' uses '<='";
}

TEST_F(AlwaysFfTest, AssignmentLhsResolvesToVariableQ) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableQ());
}

TEST_F(AlwaysFfTest, AssignmentRhsResolvesToNetB) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "assignment rhs should be a RefObj";
  EXPECT_EQ(rhs->getName(), "b");
  EXPECT_EQ(rhs->getActual<hldb::Net>(), getNet("b"));
}

// --- compiler diagnostics ----------------------------------------------------

// The construct with the most real risk of a binding failure is any of
// "a"/"b"/"q" resolving back to their declarations.
TEST_F(AlwaysFfTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a', 'b', and 'q' in '@(posedge a) q <= b;' must bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
