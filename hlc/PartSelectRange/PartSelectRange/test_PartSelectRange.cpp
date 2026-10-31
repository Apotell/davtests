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

// Validates the HLDB model built for tests/PartSelectRange/dut.sv:
//
//   module test();
//
//     parameter [3:0] MAP = 4'b01_00;
//     parameter N = 2;
//
//     genvar i;
//     for(i=0;i<N;i=i+1)
//        begin: loop
//          if (MAP[i*2+1:i*2] == 2'b00)
//            begin: foo
//               foo i0 ();
//            end
//          else if (MAP[i*2+1:i*2] == 2'b01)
//            begin: bar
//               bar i0 ();
//            end
//        end
//
//   endmodule
//
// The fixture selects 2-bit fields of a parameter with part-selects whose
// bounds depend on the loop index (11.5.1), inside a loop generate construct
// (27.4) holding an if-else-if generate construct (27.5).
//
// What is checked, and why:
//   - MAP is a parameter of type [3:0] (6.20.2) set to 4'b01_00, a 4-bit
//     binary literal (5.7.1); N is set to 2; neither is local (6.20.1, no
//     parameter port list)
//   - the only generate construct is a loop: i = 0, i < N with N bound to
//     the parameter, i = i + 1 (27.4)
//   - the loop body is the named block 'loop' holding one if-else-if (27.5)
//   - both conditions compare MAP[i*2+1:i*2] (11.5.1, bounds i*2+1 and i*2)
//     with 2'b00 and 2'b01; the else branch is itself an 'if' (27.5)
//   - the blocks foo and bar each hold the instance i0 of their module
//   - inside 'loop', the references to i are the implicit localparam (27.4)
//   - no fatal or syntax diagnostic
//
// KNOWN COMPILER BUG (reference to genvar inside generate block), not a
// defect in this test: in MAP[i*2+1:i*2] inside 'begin: loop', HLC binds
// each i to the genvar (the Variable i), although 27.4 says "Within the
// generate block of a loop generate construct, there is an implicit
// localparam declaration." and "any reference to this name inside the loop
// generate block will be a reference to the localparam, not to the genvar."
// LoopIndexInBlockIsNotTheGenvar is expected to fail until HLC is fixed; it
// is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Which block each iteration selects: that needs elaboration, and no
//     .hlc in this suite elaborates.
//   - The diagnostics for the undefined modules foo and bar. HLC reports
//     "Unsupported typespec" errors; whether an error is required for a
//     block that elaboration may never select is not pinned by this file.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PartSelectRangeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelectRange.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTest() { return hldb::findByDefName<hldb::Module>("test", m_design->getAllModules()); }

  static const hldb::GenFor *getFor() {
    const hldb::Module *const m = getTest();
    if ((m == nullptr) || (m->getGenStmts() == nullptr) || (m->getGenStmts()->size() != 1)) return nullptr;
    return any_cast<hldb::GenFor>(m->getGenStmts()->at(0));
  }

  static const hldb::GenIfElse *getIfElse() {
    const hldb::GenFor *const f = getFor();
    if (f == nullptr) return nullptr;
    const hldb::GenBlock *const b = any_cast<hldb::GenBlock>(f->getStmt());
    if ((b == nullptr) || (b->getStmts() == nullptr) || (b->getStmts()->size() != 1)) return nullptr;
    return any_cast<hldb::GenIfElse>(b->getStmts()->at(0));
  }

  static const hldb::Parameter *getParam(std::string_view name) {
    return hldb::findByName<hldb::Parameter>(name, getTest()->getParameters());
  }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  // Verifies 'e' is 'i * 2', returning the reference to i.
  static const hldb::RefObj *checkITimesTwo(const hldb::Any *e) {
    const hldb::Operation *const mul = any_cast<hldb::Operation>(e);
    EXPECT_NE(mul, nullptr) << "i*2 is an operation";
    if ((mul == nullptr) || (mul->getOperands() == nullptr) || (mul->getOperands()->size() != 2)) return nullptr;
    EXPECT_EQ(mul->getOpType(), vpiMultOp) << "11.4.3: '*'";
    ExpectConstant(mul->getOperands()->at(1), "2", "the factor");
    const hldb::RefObj *const i = any_cast<hldb::RefObj>(mul->getOperands()->at(0));
    EXPECT_NE(i, nullptr);
    if (i != nullptr) EXPECT_EQ(i->getName(), "i");
    return i;
  }

  // Verifies 'cond' is 'MAP[i*2+1:i*2] == lit' and returns the two
  // references to i in the bounds.
  static void checkCondition(const hldb::Any *cond, std::string_view lit, const hldb::RefObj **left,
                             const hldb::RefObj **right) {
    *left = nullptr;
    *right = nullptr;
    const hldb::Operation *const eq = any_cast<hldb::Operation>(cond);
    ASSERT_NE(eq, nullptr);
    EXPECT_EQ(eq->getOpType(), vpiEqOp) << "11.4.5: '=='";
    ASSERT_NE(eq->getOperands(), nullptr);
    ASSERT_EQ(eq->getOperands()->size(), 2u);
    ExpectConstant(eq->getOperands()->at(1), lit, "the compared literal");
    const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(eq->getOperands()->at(0));
    ASSERT_NE(ps, nullptr) << "11.5.1: MAP[i*2+1:i*2] is a part-select";
    const hldb::RefObj *const map = ps->getPrefix<hldb::RefObj>();
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->getActual(), getParam("MAP")) << "binds to the parameter MAP";
    ASSERT_NE(ps->getRange(), nullptr);
    const hldb::Operation *const add = any_cast<hldb::Operation>(ps->getRange()->getLeftExpr());
    ASSERT_NE(add, nullptr) << "the msb expression is i*2+1";
    EXPECT_EQ(add->getOpType(), vpiAddOp);
    ASSERT_NE(add->getOperands(), nullptr);
    ASSERT_EQ(add->getOperands()->size(), 2u);
    ExpectConstant(add->getOperands()->at(1), "1", "the addend");
    *left = checkITimesTwo(add->getOperands()->at(0));
    *right = checkITimesTwo(ps->getRange()->getRightExpr());
  }

  // Verifies 'stmt' is the block 'name' holding the instance i0 of 'name'.
  static void ExpectBlockWithInstance(const hldb::Any *stmt, std::string_view name) {
    const hldb::GenBlock *const b = any_cast<hldb::GenBlock>(stmt);
    ASSERT_NE(b, nullptr) << "27.5: a begin-end block";
    EXPECT_EQ(b->getName(), name) << "27.5: the block is named";
    ASSERT_NE(b->getStmts(), nullptr);
    const hldb::RefInstance *inst = nullptr;
    for (const hldb::Any *s : *b->getStmts()) {
      if (const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(s)) inst = ri;
    }
    ASSERT_NE(inst, nullptr) << "the block holds a module instance";
    EXPECT_EQ(inst->getName(), "i0");
    ASSERT_NE(inst->getTypespec(), nullptr);
    EXPECT_EQ(inst->getTypespec()->getName(), name) << "an instance of module " << name;
  }
};

TEST_F(PartSelectRangeTest, MapAndNAreOverridableParameters) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(getParam("MAP"), nullptr);
  ASSERT_NE(getParam("N"), nullptr);
  EXPECT_FALSE(getParam("MAP")->getLocalParam()) << "6.20.1: no parameter port list";
  EXPECT_FALSE(getParam("N")->getLocalParam()) << "6.20.1: no parameter port list";
  ASSERT_NE(m->getParamAssigns(), nullptr);
  ASSERT_EQ(m->getParamAssigns()->size(), 2u);
  const hldb::Constant *const map = m->getParamAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(map->getDecompile(), "4'b01_00");
  EXPECT_EQ(map->getConstType(), vpiBinaryConst) << "5.7.1: a based binary literal";
  EXPECT_EQ(map->getSize(), 4) << "5.7.1: the size is 4";
  ExpectConstant(m->getParamAssigns()->at(1)->getRhs(), "2", "N's value");
}

TEST_F(PartSelectRangeTest, LoopRunsIFromZeroWhileLessThanN) {
  const hldb::GenFor *const f = getFor();
  ASSERT_NE(f, nullptr) << "27.4: the only generate construct is a loop";
  ASSERT_NE(f->getForInitStmts(), nullptr);
  ASSERT_EQ(f->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(f->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  ExpectConstant(init->getRhs(), "0", "27.4: i=0");
  const hldb::Operation *const cond = any_cast<hldb::Operation>(f->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp) << "i<N";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getActual(), getParam("N"));
  ASSERT_NE(f->getForIncStmts(), nullptr);
  ASSERT_EQ(f->getForIncStmts()->size(), 1u);
  const hldb::Assignment *const inc = any_cast<hldb::Assignment>(f->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "27.4: i=i+1";
  const hldb::Operation *const add = inc->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp);
}

TEST_F(PartSelectRangeTest, LoopBlockHoldsIfElseIf) {
  const hldb::GenFor *const f = getFor();
  ASSERT_NE(f, nullptr);
  const hldb::GenBlock *const b = any_cast<hldb::GenBlock>(f->getStmt());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "loop") << "27.4: the loop block is named";
  const hldb::GenIfElse *const ie = getIfElse();
  ASSERT_NE(ie, nullptr) << "27.5: the block holds one if-else";
  EXPECT_NE(any_cast<hldb::GenIf>(ie->getElseStmt()), nullptr) << "27.5: the else branch is an 'if'";
}

TEST_F(PartSelectRangeTest, ConditionsSelectTwoBitFieldsOfMap) {
  const hldb::GenIfElse *const ie = getIfElse();
  ASSERT_NE(ie, nullptr);
  const hldb::RefObj *left = nullptr;
  const hldb::RefObj *right = nullptr;
  checkCondition(ie->getCondition(), "2'b00", &left, &right);
  const hldb::GenIf *const elseIf = any_cast<hldb::GenIf>(ie->getElseStmt());
  ASSERT_NE(elseIf, nullptr);
  checkCondition(elseIf->getCondition(), "2'b01", &left, &right);
}

TEST_F(PartSelectRangeTest, BranchesInstantiateFooAndBar) {
  const hldb::GenIfElse *const ie = getIfElse();
  ASSERT_NE(ie, nullptr);
  ExpectBlockWithInstance(ie->getStmt(), "foo");
  const hldb::GenIf *const elseIf = any_cast<hldb::GenIf>(ie->getElseStmt());
  ASSERT_NE(elseIf, nullptr);
  ExpectBlockWithInstance(elseIf->getStmt(), "bar");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (reference to
// genvar inside generate block) in the file header.
TEST_F(PartSelectRangeTest, LoopIndexInBlockIsNotTheGenvar) {
  const hldb::GenIfElse *const ie = getIfElse();
  ASSERT_NE(ie, nullptr);
  const hldb::Variable *const genvar = hldb::findByName<hldb::Variable>("i", getTest()->getVariables());
  ASSERT_NE(genvar, nullptr) << "27.4: 'genvar i'";
  const hldb::RefObj *left = nullptr;
  const hldb::RefObj *right = nullptr;
  checkCondition(ie->getCondition(), "2'b00", &left, &right);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_NE(left->getActual(), genvar) << "27.4: a reference inside the loop block is to the localparam, not to "
                                          "the genvar";
  EXPECT_NE(right->getActual(), genvar) << "27.4: a reference inside the loop block is to the localparam, not to "
                                           "the genvar";
}

TEST_F(PartSelectRangeTest, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
