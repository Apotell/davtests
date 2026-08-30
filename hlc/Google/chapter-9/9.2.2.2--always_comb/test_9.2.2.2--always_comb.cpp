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

// Tests for 9.2.2.2--always_comb.sv (tags: 9.2.2.2)
//   module always_tb ();
//     wire a = 0;
//     reg b = 0;
//     always_comb
//       b = ~a;
//   endmodule
//
// IEEE 1800-2017 Sec 9.2.2.2 "always_comb procedure": a refined always
// form, unlike the general "always" (9.2.2.1) -- its Always node's
// AlwaysType must be exactly vpiAlwaysComb.
//
// "wire a = 0" -- unlike the "reg"/"logic" declarations in
// 9.2.1--initial.sv / 9.2.2.1--always.sv, "wire" IS a net-type keyword
// (IEEE 1800-2023 Sec 6.7), so "a" must be a Net, never a Variable. Per
// Sec 6.7.1 "Net declarations with built-in net types", a net declared
// with "= <expr>" is a net_decl_assignment: semantically equivalent to a
// separate continuous assignment. The Net class carries this directly on
// itself via getValue() (the assigned expression, correctly populated)
// and getNetDeclAssign() (a flag meant to mark exactly this form -- but
// CONFIRMED NEVER SET in this build; see the NetAIsMarkedAsNetDeclAssign
// test below for the cross-checked evidence). With no explicit data-type
// keyword before "wire", "a" defaults to a 1-bit, unsigned LogicTypespec
// (Sec 6.7.2), same as an undecorated "logic"/"reg" scalar.
//
// "reg b = 0" is an ordinary variable, same reasoning as
// 9.2.1--initial.sv / 9.2.2.1--always.sv: single-bit, unsigned
// LogicTypespec, no declared Ranges, in the module's variable collection
// only. Its "0" initializer is a plain-value literal, so per this
// codebase's established plain-value classification it is vpiUIntConst
// (see 9.2.1--initial.sv; tracked open question about this vs. vpiIntConst
// in delay/count contexts, see project memory).
//
// "~a" is IEEE 1800-2017 Sec 11.4.1's unary bitwise negation operator
// (vpiBitNegOp), same as 9.2.2.1--always.sv.
//
// Checked:
//   - design has module "always_tb" with exactly 1 net ("a") and 1
//     variable ("b")
//   - net "a": LogicTypespec, unsigned, no declared ranges; net type is
//     vpiWire; getValue() resolves to Constant "0" (vpiUIntConst); not
//     duplicated in the variable collection. getNetDeclAssign() SHOULD be
//     true per 6.7.1 but is a confirmed compiler bug (never set) --
//     asserted anyway and intentionally left failing
//   - variable "b": LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Always whose AlwaysType
//     is exactly vpiAlwaysComb
//   - the Always' body is directly an Assignment (no enclosing Begin,
//     since the source has no begin/end)
//   - the Assignment is blocking ("="); lhs is a RefObj resolving to
//     Variable "b"; rhs is an Operation(vpiBitNegOp) with 1 operand, a
//     RefObj resolving to Net "a"

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

class AlwaysCombTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.2.2.2--always_comb.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("always_tb", m_design->getAllModules());
  }

  static const hldb::Net *getNetA() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("a", mod->getNets());
  }

  static const hldb::LogicTypespec *getNetATypespec() {
    const hldb::Net *const a = getNetA();
    if (a == nullptr || a->getTypespec() == nullptr) return nullptr;
    return a->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Variable *getVariableB() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("b", mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableBTypespec() {
    const hldb::Variable *const b = getVariableB();
    if (b == nullptr || b->getTypespec() == nullptr) return nullptr;
    return b->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::Always *const alw = getAlwaysProcess();
    if (alw == nullptr) return nullptr;
    return alw->getStmt<hldb::Assignment>();
  }
};

// --- net "a" (wire) -----------------------------------------------------------

TEST_F(AlwaysCombTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(AlwaysCombTest, ModuleHasOneNet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 1u);
}

TEST_F(AlwaysCombTest, NetAExists) { EXPECT_NE(getNetA(), nullptr); }

// "wire a" has a net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// must not also appear in the module's variable collection.
TEST_F(AlwaysCombTest, NetAIsNotDuplicatedAsVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getVariables() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>("a", mod->getVariables()), nullptr)
        << "'wire a' has a net-type keyword and must not also appear as a Variable";
  }
}

TEST_F(AlwaysCombTest, NetANetTypeIsWire) {
  const hldb::Net *const a = getNetA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire) << "'wire a' must have net type vpiWire";
}

TEST_F(AlwaysCombTest, NetATypespecIsLogicTypespec) { EXPECT_NE(getNetATypespec(), nullptr); }

TEST_F(AlwaysCombTest, NetATypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getNetATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.7.2: an undecorated 'wire' defaults to unsigned";
}

TEST_F(AlwaysCombTest, NetATypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getNetATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'wire a' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

// CONFIRMED COMPILER BUG (not a defect in always_comb.sv):
//   6.7.1: "wire a = 0" is a net_decl_assignment, distinct from a
//   Variable's plain initial value. getNetDeclAssign() exists specifically
//   to flag this, but this build never sets it to true -- confirmed by
//   cross-checking chapter-6/6.9.2--vector_scalared.sv's identical shape
//   ("tri1 scalared [15:0] a = 0"): its net's own value is likewise
//   correctly captured (see 6.9.2--vector_scalared.log's "vpiValue
//   Constant ... vpiDecompile: '0'"), yet the dumper -- which does print
//   other true boolean flags on that same net, e.g. "vpiExplicitScalared:
//   true" -- never prints "vpiNetDeclAssign: true" either. The value
//   itself is captured correctly in both cases (see NetAValueIsConstantZero
//   below); only the companion flag is never set. This assertion is
//   intentionally left failing to track the gap, not tolerating it.
TEST_F(AlwaysCombTest, NetAIsMarkedAsNetDeclAssign) {
  const hldb::Net *const a = getNetA();
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->getNetDeclAssign()) << "6.7.1: 'wire a = 0' is a net_decl_assignment";
}

TEST_F(AlwaysCombTest, NetAValueIsConstantZero) {
  const hldb::Net *const a = getNetA();
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const val = a->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'wire a = 0' must carry its net_decl_assignment value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- variable "b" (reg) --------------------------------------------------------

TEST_F(AlwaysCombTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
}

TEST_F(AlwaysCombTest, VariableBExists) { EXPECT_NE(getVariableB(), nullptr); }

// "reg b" has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// must not also appear in the module's net collection.
TEST_F(AlwaysCombTest, VariableBIsNotDuplicatedAsNet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", mod->getNets()), nullptr)
        << "'reg b' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(AlwaysCombTest, VariableBTypespecIsLogicTypespec) { EXPECT_NE(getVariableBTypespec(), nullptr); }

TEST_F(AlwaysCombTest, VariableBTypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getVariableBTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg' with no 'signed' keyword defaults to unsigned";
}

TEST_F(AlwaysCombTest, VariableBTypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getVariableBTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'reg b' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(AlwaysCombTest, VariableBInitialValueIsConstantZero) {
  const hldb::Variable *const b = getVariableB();
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const val = b->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'reg b = 0' must carry an initial value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- always_comb process structure ---------------------------------------------

TEST_F(AlwaysCombTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(AlwaysCombTest, TheOneProcessIsAlways) { EXPECT_NE(getAlwaysProcess(), nullptr); }

TEST_F(AlwaysCombTest, AlwaysTypeIsAlwaysComb) {
  const hldb::Always *const alw = getAlwaysProcess();
  ASSERT_NE(alw, nullptr);
  EXPECT_EQ(alw->getAlwaysType(), vpiAlwaysComb) << "9.2.2.2: 'always_comb' must have AlwaysType vpiAlwaysComb";
}

// 9.2.2.2: "always_comb" with no begin/end wraps a single statement_or_null
// -- the process' stmt must be the Assignment itself, not a Begin block.
TEST_F(AlwaysCombTest, AlwaysStmtIsDirectlyAnAssignment) {
  EXPECT_NE(getAssignment(), nullptr)
      << "9.2.2.2: a single-statement 'always_comb' body (no begin/end) must not be wrapped in a Begin block";
}

// --- assignment "b = ~a" -----------------------------------------------------

TEST_F(AlwaysCombTest, AssignmentIsBlocking) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "'b = ~a' uses the blocking assignment operator '='";
}

TEST_F(AlwaysCombTest, AssignmentLhsResolvesToVariableB) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableB());
}

TEST_F(AlwaysCombTest, AssignmentRhsIsBitNegOperation) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'~a' should be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp) << "11.4.1: '~' is bitwise negation (vpiBitNegOp), not '!' (vpiNotOp)";
}

TEST_F(AlwaysCombTest, BitNegOperationHasOneOperand) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 1u) << "'~a' is a unary operator with 1 operand";
}

TEST_F(AlwaysCombTest, BitNegOperandResolvesToNetA) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_GE(rhs->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "operand[0] of '~a' should be a RefObj";
  EXPECT_EQ(operand->getName(), "a");
  EXPECT_EQ(operand->getActual<hldb::Net>(), getNetA()) << "'a' is a Net (wire), not a Variable";
}

// --- compiler diagnostics ----------------------------------------------------

// The construct with the most real risk of a binding failure is "a"/"b"
// resolving back to their declarations.
TEST_F(AlwaysCombTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a' and 'b' in 'b = ~a' must bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
