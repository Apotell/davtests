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

// Tests for tests/LogicCast/dut.sv:
//
//   module top ();
//     always_comb begin : csr_read_write
//     dmstatus.allnonexistent = logic'(32'(hartsel_o) > (NrHarts - 1));
//     end
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 9.2.2.2 "Combinational logic always_comb procedure":
//   'always_comb' creates an Always process of the comb kind
//   (vpiAlwaysComb); its statement is the named sequential block
//   'csr_read_write' (Sec 9.3.1, 9.3.5).
// IEEE 1800-2023 Sec 10.4.1: '=' inside the block is a blocking procedural
//   assignment.
// IEEE 1800-2023 Sec 6.24.1 "Cast operator": "cast ::= casting_type '
//   ( expression )" with "casting_type ::= simple_type | constant_primary
//   | ...".
//   - 'logic'(...)' has a simple_type casting type: the result type is
//     'logic', a 1-bit 4-state type (Sec 6.11).
//   - '32'(hartsel_o)' has a constant_primary casting type: "If the casting
//     type is a constant expression with a positive integral value, the
//     expression in parentheses shall be padded or truncated to the size
//     specified" -- a size cast of 'hartsel_o'.
//   Both are cast operations (vpiCastOp).
// IEEE 1800-2023 Sec 11.4.4 / 11.4.3: '>' is a relational operation
//   (vpiGtOp) between the size cast and the parenthesized subtraction
//   'NrHarts - 1' (vpiSubOp).
// IEEE 1800-2023 Sec 6.10: 'dmstatus', 'hartsel_o' and 'NrHarts' are never
//   declared. Implicit nets are only inferred in port expressions and on
//   the LHS of continuous assignments, never for identifiers used in
//   procedural code -- every one of these references is an error.
//
// Not checked: how the size '32' of the size cast is represented in the
// model (operand vs. typespec), which the standard does not prescribe.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LogicCastTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LogicCast.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Always *getAlways() {
    const hldb::Module *const m = getTop();
    if (m == nullptr || m->getProcesses() == nullptr || m->getProcesses()->size() != 1) return nullptr;
    return any_cast<hldb::Always>(m->getProcesses()->at(0));
  }

  static const hldb::Begin *getBlock() {
    const hldb::Always *const a = getAlways();
    return (a == nullptr) ? nullptr : a->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::Begin *const b = getBlock();
    if (b == nullptr || b->getStmts() == nullptr || b->getStmts()->size() != 1) return nullptr;
    return any_cast<hldb::Assignment>(b->getStmts()->at(0));
  }

  static const hldb::Operation *getOuterCast() {
    const hldb::Assignment *const asg = getAssignment();
    return (asg == nullptr) ? nullptr : asg->getRhs<hldb::Operation>();
  }

  static const hldb::Operation *getGt() {
    const hldb::Operation *const cast = getOuterCast();
    if (cast == nullptr || cast->getOperands() == nullptr || cast->getOperands()->size() != 1) return nullptr;
    return any_cast<hldb::Operation>(cast->getOperands()->at(0));
  }

  static const hldb::Operation *getGtOperand(size_t index) {
    const hldb::Operation *const gt = getGt();
    if (gt == nullptr || gt->getOperands() == nullptr || gt->getOperands()->size() != 2) return nullptr;
    return any_cast<hldb::Operation>(gt->getOperands()->at(index));
  }
};

// ---------------------------------------------------------------------------
// Process / block / assignment -- Sec 9.2.2.2, 9.3.1, 10.4.1
// ---------------------------------------------------------------------------

TEST_F(LogicCastTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(LogicCastTest, SingleAlwaysCombProcess) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_EQ(m->getProcesses()->size(), 1u);
  const hldb::Always *const a = getAlways();
  ASSERT_NE(a, nullptr) << "process should be an Always";
  EXPECT_EQ(a->getAlwaysType(), vpiAlwaysComb);
}

TEST_F(LogicCastTest, StatementIsNamedBlockCsrReadWrite) {
  const hldb::Always *const a = getAlways();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getStmt(), nullptr);
  const hldb::Begin *const b = getBlock();
  ASSERT_NE(b, nullptr) << "'begin : csr_read_write ... end' should be a Begin";
  EXPECT_EQ(b->getName(), std::string_view("csr_read_write"));
}

TEST_F(LogicCastTest, BlockHoldsOneBlockingAssignment) {
  const hldb::Begin *const b = getBlock();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  ASSERT_EQ(b->getStmts()->size(), 1u);
  const hldb::Assignment *const asg = getAssignment();
  ASSERT_NE(asg, nullptr);
  EXPECT_TRUE(asg->getBlocking());
}

TEST_F(LogicCastTest, LhsIsDottedReferenceDmstatusAllnonexistent) {
  const hldb::Assignment *const asg = getAssignment();
  ASSERT_NE(asg, nullptr);
  const hldb::RefObj *const lhs = asg->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u);
  EXPECT_EQ(lhs->getPathElems()->at(0)->getName(), std::string_view("dmstatus"));
  EXPECT_EQ(lhs->getPathElems()->at(1)->getName(), std::string_view("allnonexistent"));
}

// ---------------------------------------------------------------------------
// Casts and operators -- Sec 6.24.1, 11.4
// ---------------------------------------------------------------------------

TEST_F(LogicCastTest, RhsIsLogicCast) {
  const hldb::Operation *const cast = getOuterCast();
  ASSERT_NE(cast, nullptr) << "RHS 'logic'(...)' should be an Operation";
  EXPECT_EQ(cast->getOpType(), vpiCastOp);
  ASSERT_NE(cast->getTypespec(), nullptr) << "cast target type 'logic' missing";
  ASSERT_NE(cast->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(cast->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  const hldb::LogicTypespec *const lts = cast->getTypespec()->getActual<hldb::LogicTypespec>();
  EXPECT_TRUE(lts->getRanges() == nullptr || lts->getRanges()->empty()) << "'logic' is 1 bit";
  EXPECT_FALSE(lts->getSigned());
}

TEST_F(LogicCastTest, LogicCastOperandIsGreaterThan) {
  const hldb::Operation *const cast = getOuterCast();
  ASSERT_NE(cast, nullptr);
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::Operation *const gt = getGt();
  ASSERT_NE(gt, nullptr);
  EXPECT_EQ(gt->getOpType(), vpiGtOp);
  ASSERT_NE(gt->getOperands(), nullptr);
  EXPECT_EQ(gt->getOperands()->size(), 2u);
}

TEST_F(LogicCastTest, GtLeftIsSizeCastOfHartselO) {
  const hldb::Operation *const sizeCast = getGtOperand(0);
  ASSERT_NE(sizeCast, nullptr) << "'32'(hartsel_o)' should be an Operation";
  EXPECT_EQ(sizeCast->getOpType(), vpiCastOp);
  ASSERT_NE(sizeCast->getOperands(), nullptr);
  const hldb::RefObj *hartsel = nullptr;
  for (const hldb::Any *const operand : *sizeCast->getOperands()) {
    if (const hldb::RefObj *const r = any_cast<hldb::RefObj>(operand)) {
      if (r->getName() == "hartsel_o") hartsel = r;
    }
  }
  EXPECT_NE(hartsel, nullptr) << "size cast should apply to 'hartsel_o'";
}

TEST_F(LogicCastTest, GtRightIsNrHartsMinusOne) {
  const hldb::Operation *const sub = getGtOperand(1);
  ASSERT_NE(sub, nullptr) << "'(NrHarts - 1)' should be an Operation";
  EXPECT_EQ(sub->getOpType(), vpiSubOp);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_EQ(sub->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(sub->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("NrHarts"));
  const hldb::Constant *const one = any_cast<hldb::Constant>(sub->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), std::string_view("1"));
}

// ---------------------------------------------------------------------------
// Undeclared identifiers -- Sec 6.10
// ---------------------------------------------------------------------------

TEST_F(LogicCastTest, UndeclaredDmstatusIsReported) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "dmstatus", 3, 3), nullptr)
      << "'dmstatus' is not declared (Sec 6.10)";
}

TEST_F(LogicCastTest, UndeclaredHartselOIsReported) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "hartsel_o", 3, 40), nullptr)
      << "'hartsel_o' is not declared (Sec 6.10)";
}

TEST_F(LogicCastTest, UndeclaredNrHartsIsReported) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "NrHarts", 3, 54), nullptr)
      << "'NrHarts' is not declared (Sec 6.10)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
