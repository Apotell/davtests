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

// Validates the HLDB model built for tests/PartSelectParent/dut.sv:
//
//   module top(output int o);
//      initial begin
//         o = 0;
//         {o[1:0]} = 2'b11;
//      end
//
//      assign   {o[1:0]} = 2'b11;
//   endmodule // top
//
// The fixture writes the part-select o[1:0] inside a concatenation (11.4.12)
// used as an assignment target (10.4.1, 10.3). o is a variable ('output int'
// declares a variable, 23.2.2.3), and it is written both procedurally (the
// initial block) and by a continuous assignment, which 6.5 forbids.
//
// What is checked, and why:
//   - o is the only port, an output, connected to the variable o
//   - the initial procedure (9.2.1) holds a begin-end block with two
//     blocking assignments (10.4.1): 'o = 0', then '{o[1:0]} = 2'b11'
//   - each '{o[1:0]}' target is a concatenation with one operand, the
//     part-select o[1:0] of the variable o with bounds 1 and 0 (11.5.1)
//   - 2'b11 is a 2-bit binary literal (5.7.1)
//   - the continuous assignment (10.3) has the same target shape
//   - an error is reported for the mixed procedural and continuous writes
//
// KNOWN COMPILER BUG (mixed procedural and continuous writes unreported),
// not a defect in this test: HLC accepts the procedural writes of o in the
// initial block together with 'assign {o[1:0]} = 2'b11;' and reports
// nothing, although 6.5 says "it shall be an error to have multiple
// continuous assignments or a mixture of procedural and continuous
// assignments writing to any term in the expansion of the longest static
// prefix of a variable". MixedWritesOfOAreAnError is expected to fail until
// HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The value of o is a runtime property; HLC is a static
//     compiler/elaborator and not a simulator, so this is out of scope
//     permanently.
//   - Which error identifier is used, and its wording: tool conventions.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PartSelectParentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelectParent.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getO() {
    const hldb::Module *const m = getTop();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("o", m->getVariables());
  }

  static const hldb::Begin *getInitialBlock() {
    const hldb::Module *const m = getTop();
    if ((m == nullptr) || (m->getProcesses() == nullptr) || (m->getProcesses()->size() != 1)) return nullptr;
    const hldb::Initial *const i = any_cast<hldb::Initial>(m->getProcesses()->at(0));
    if (i == nullptr) return nullptr;
    return i->getStmt<hldb::Begin>();
  }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  // Verifies 'e' is the target '{o[1:0]}'.
  static void ExpectConcatOfO1To0(const hldb::Any *e, std::string_view what) {
    const hldb::Operation *const cat = any_cast<hldb::Operation>(e);
    ASSERT_NE(cat, nullptr) << what;
    EXPECT_EQ(cat->getOpType(), vpiConcatOp) << "11.4.12: " << what << " is a concatenation";
    ASSERT_NE(cat->getOperands(), nullptr);
    ASSERT_EQ(cat->getOperands()->size(), 1u) << what << " has one operand";
    const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(cat->getOperands()->at(0));
    ASSERT_NE(ps, nullptr) << "11.5.1: o[1:0] is a part-select";
    const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getActual(), getO()) << "binds to the variable o";
    ASSERT_NE(ps->getRange(), nullptr);
    ExpectConstant(ps->getRange()->getLeftExpr(), "1", "11.5.1: msb expression");
    ExpectConstant(ps->getRange()->getRightExpr(), "0", "11.5.1: lsb expression");
  }

  static void ExpectTwoBitOnes(const hldb::Any *e) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getDecompile(), "2'b11");
    EXPECT_EQ(c->getConstType(), vpiBinaryConst) << "5.7.1: a based binary literal";
    EXPECT_EQ(c->getSize(), 2) << "5.7.1: the size is 2";
  }
};

TEST_F(PartSelectParentTest, OIsTheOnlyPortAnOutputVariable) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  const hldb::Port *const p = m->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "o");
  EXPECT_EQ(p->getDirection(), vpiOutput);
  const hldb::RefObj *const low = any_cast<hldb::RefObj>(p->getLowConn());
  ASSERT_NE(low, nullptr);
  ASSERT_NE(getO(), nullptr) << "23.2.2.3: 'output int o' declares a variable";
  EXPECT_EQ(low->getActual(), getO());
}

TEST_F(PartSelectParentTest, InitialHoldsTwoBlockingAssignments) {
  const hldb::Begin *const b = getInitialBlock();
  ASSERT_NE(b, nullptr) << "9.2.1: one initial procedure with a begin-end block";
  ASSERT_NE(b->getStmts(), nullptr);
  ASSERT_EQ(b->getStmts()->size(), 2u);
  const hldb::Assignment *const a0 = any_cast<hldb::Assignment>(b->getStmts()->at(0));
  const hldb::Assignment *const a1 = any_cast<hldb::Assignment>(b->getStmts()->at(1));
  ASSERT_NE(a0, nullptr);
  ASSERT_NE(a1, nullptr);
  EXPECT_TRUE(a0->getBlocking()) << "10.4.1";
  EXPECT_TRUE(a1->getBlocking()) << "10.4.1";
  const hldb::RefObj *const lhs0 = a0->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs0, nullptr);
  EXPECT_EQ(lhs0->getActual(), getO());
  ExpectConstant(a0->getRhs(), "0", "'o = 0'");
  ExpectConcatOfO1To0(a1->getLhs(), "the second target");
  ExpectTwoBitOnes(a1->getRhs());
}

TEST_F(PartSelectParentTest, ContAssignTargetsConcatOfO1To0) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_EQ(m->getContAssigns()->size(), 1u) << "10.3: one continuous assignment";
  const hldb::ContAssign *const ca = m->getContAssigns()->at(0);
  ExpectConcatOfO1To0(ca->getLhs(), "the continuous target");
  ExpectTwoBitOnes(ca->getRhs());
}

TEST_F(PartSelectParentTest, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (mixed
// procedural and continuous writes unreported) in the file header.
TEST_F(PartSelectParentTest, MixedWritesOfOAreAnError) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbError, 1) << "6.5: it shall be an error to have a mixture of procedural and continuous "
                                 "assignments writing to any term in the expansion of the longest static prefix "
                                 "of a variable";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
