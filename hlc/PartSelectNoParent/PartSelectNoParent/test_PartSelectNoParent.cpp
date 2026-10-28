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

// Validates the HLDB model built for tests/PartSelectNoParent/dut.sv:
//
//   package my_pkg;
//      parameter int Shares = 2;
//      parameter int KeyWidth = 256;
//      parameter int NumRegsKey = 8;
//
//      typedef struct packed {
//         logic [Shares-1:0][KeyWidth-1:0] key;
//      } hw_key_req_t;
//   endpackage // my_pkg
//
//   module top(output int o);
//      import my_pkg::*;
//      hw_key_req_t keymgr_key_i = '1;
//      always_comb begin : key_sideload_get
//         for (int i = 0; i < NumRegsKey; i++) begin
//            o = keymgr_key_i.key[0][31 : 0];
//         end
//      end
//   endmodule
//
// The fixture selects key[0][31:0]: a part-select (11.5.1) whose prefix is
// the bit-select key[0] of a two-dimensional packed member (7.4.5) of a
// structure reached through a variable, inside a for loop (12.7.1) in an
// always_comb block (9.2.2.2).
//
// What is checked, and why:
//   - my_pkg holds the 3 int parameters; package parameters are local
//     (6.20.1 makes 'parameter' a synonym for 'localparam' in a package)
//   - member key has 2 packed dimensions, [Shares-1:0] then [KeyWidth-1:0]
//     (7.4.1, 7.4.5), whose left bounds are subtractions of 1 from the
//     package parameters
//   - top's only process is an always_comb (9.2.2.2) holding the named
//     block key_sideload_get (9.3.4)
//   - the for loop declares int i, initializes it to 0, tests
//     i < NumRegsKey with NumRegsKey bound to the package parameter through
//     the import (26.3), and steps with i++ (12.7.1, 11.4.2)
//   - the loop body is a blocking assignment (10.4.1) to the port variable o
//   - the RHS names keymgr_key_i, then the part-select [31:0] whose prefix
//     is the bit-select [0] of the member key
//   - no fatal, syntax or error diagnostic: the file is legal
//
// Model convention: the hierarchical RHS is one RefObj whose path elements
// are the RefObj keymgr_key_i and the PartSelect key[0][31:0].
//
// What is NOT checked, and why:
//   - The value o takes is a runtime property; HLC is a static
//     compiler/elaborator and not a simulator, so this is out of scope
//     permanently. RhsIsPartSelectOfBitSelectOfKey is the static evidence.
//   - The reduced bounds 1 and 255: no .hlc in this suite elaborates, and
//     the unreduced subtractions are asserted instead.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PartSelectNoParentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelectNoParent.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("my_pkg", m_design->getAllPackages()); }

  static const hldb::Parameter *getPkgParam(std::string_view name) {
    const hldb::Package *const p = getPkg();
    if (p == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, p->getParameters());
  }

  static const hldb::ForStmt *getFor() {
    const hldb::Module *const m = getTop();
    if ((m == nullptr) || (m->getProcesses() == nullptr) || (m->getProcesses()->size() != 1)) return nullptr;
    const hldb::Always *const a = any_cast<hldb::Always>(m->getProcesses()->at(0));
    if (a == nullptr) return nullptr;
    const hldb::Begin *const b = a->getStmt<hldb::Begin>();
    if ((b == nullptr) || (b->getStmts() == nullptr) || (b->getStmts()->size() != 1)) return nullptr;
    return any_cast<hldb::ForStmt>(b->getStmts()->at(0));
  }

  static const hldb::Assignment *getBodyAssign() {
    const hldb::ForStmt *const f = getFor();
    if (f == nullptr) return nullptr;
    const hldb::Begin *const b = any_cast<hldb::Begin>(f->getStmt());
    if ((b == nullptr) || (b->getStmts() == nullptr) || (b->getStmts()->size() != 1)) return nullptr;
    return any_cast<hldb::Assignment>(b->getStmts()->at(0));
  }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  // Verifies 'e' is 'name - 1' with 'name' bound to the package parameter.
  static void ExpectParamMinusOne(const hldb::Expr *e, std::string_view name) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(e);
    ASSERT_NE(op, nullptr) << name << "-1 is an operation";
    EXPECT_EQ(op->getOpType(), vpiSubOp) << "11.4.3: binary '-'";
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u);
    const hldb::RefObj *const r = any_cast<hldb::RefObj>(op->getOperands()->at(0));
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->getActual(), getPkgParam(name)) << "binds to my_pkg's parameter";
    ExpectConstant(op->getOperands()->at(1), "1", "the subtrahend");
  }
};

TEST_F(PartSelectNoParentTest, PackageHoldsThreeLocalIntParameters) {
  const char *const names[] = {"Shares", "KeyWidth", "NumRegsKey"};
  ASSERT_NE(getPkg(), nullptr) << "26.2: package my_pkg";
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Parameter *const p = getPkgParam(names[i]);
    ASSERT_NE(p, nullptr) << names[i];
    EXPECT_TRUE(p->getLocalParam()) << "6.20.1: 'parameter' in a package is a local parameter";
  }
}

TEST_F(PartSelectNoParentTest, KeyHasTwoPackedDimensions) {
  const hldb::Package *const p = getPkg();
  ASSERT_NE(p, nullptr);
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  const hldb::Assignment *const as = getBodyAssign();
  ASSERT_NE(as, nullptr);
  const hldb::RefObj *const rhs = as->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(rhs->getPathElems()->at(1));
  ASSERT_NE(ps, nullptr);
  const hldb::BitSelect *const bs = ps->getPrefix<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr);
  const hldb::RefObj *const key = bs->getPrefix<hldb::RefObj>();
  ASSERT_NE(key, nullptr);
  const hldb::TypespecMember *const mem = key->getActual<hldb::TypespecMember>();
  ASSERT_NE(mem, nullptr) << "7.2: 'key' names the structure member";
  ASSERT_NE(mem->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = mem->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "7.4.1: logic";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 2u) << "7.4.5: two packed dimensions";
  ExpectParamMinusOne(lt->getRanges()->at(0)->getLeftExpr(), "Shares");
  ExpectConstant(lt->getRanges()->at(0)->getRightExpr(), "0", "right bound of [Shares-1:0]");
  ExpectParamMinusOne(lt->getRanges()->at(1)->getLeftExpr(), "KeyWidth");
  ExpectConstant(lt->getRanges()->at(1)->getRightExpr(), "0", "right bound of [KeyWidth-1:0]");
}

TEST_F(PartSelectNoParentTest, AlwaysCombHoldsNamedBlock) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_EQ(m->getProcesses()->size(), 1u);
  const hldb::Always *const a = any_cast<hldb::Always>(m->getProcesses()->at(0));
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getAlwaysType(), vpiAlwaysComb) << "9.2.2.2: always_comb";
  const hldb::Begin *const b = a->getStmt<hldb::Begin>();
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "key_sideload_get") << "9.3.4: the block is named";
}

TEST_F(PartSelectNoParentTest, ForLoopOverNumRegsKey) {
  const hldb::ForStmt *const f = getFor();
  ASSERT_NE(f, nullptr) << "12.7.1: the block holds one for loop";
  ASSERT_NE(f->getForInitStmts(), nullptr);
  ASSERT_EQ(f->getForInitStmts()->size(), 1u);
  const hldb::Operation *const cond = any_cast<hldb::Operation>(f->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp) << "11.4.4: '<'";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const i = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(i, nullptr);
  const hldb::Variable *const iv = i->getActual<hldb::Variable>();
  ASSERT_NE(iv, nullptr) << "12.7.1: i is the loop variable";
  EXPECT_EQ(iv->getName(), "i");
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getActual(), getPkgParam("NumRegsKey")) << "26.3: visible through the wildcard import";
  ASSERT_NE(f->getForIncStmts(), nullptr);
  ASSERT_EQ(f->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(f->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp) << "11.4.2: i++";
}

TEST_F(PartSelectNoParentTest, BodyAssignsPortO) {
  const hldb::Assignment *const as = getBodyAssign();
  ASSERT_NE(as, nullptr) << "the loop body holds one assignment";
  EXPECT_TRUE(as->getBlocking()) << "10.4.1: '=' is blocking";
  const hldb::RefObj *const lhs = as->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("o", getTop()->getVariables()))
      << "23.2.2.3: 'output int o' is a variable";
}

TEST_F(PartSelectNoParentTest, RhsIsPartSelectOfBitSelectOfKey) {
  const hldb::Assignment *const as = getBodyAssign();
  ASSERT_NE(as, nullptr);
  const hldb::RefObj *const rhs = as->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const head = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getActual(), hldb::findByName<hldb::Variable>("keymgr_key_i", getTop()->getVariables()));
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(rhs->getPathElems()->at(1));
  ASSERT_NE(ps, nullptr) << "11.5.1: [31 : 0] is a part-select";
  ASSERT_NE(ps->getRange(), nullptr);
  ExpectConstant(ps->getRange()->getLeftExpr(), "31", "11.5.1: msb expression");
  ExpectConstant(ps->getRange()->getRightExpr(), "0", "11.5.1: lsb expression");
  const hldb::BitSelect *const bs = ps->getPrefix<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr) << "7.4.5: key[0] selects the first packed dimension";
  ExpectConstant(bs->getIndex(), "0", "the index of key[0]");
  const hldb::RefObj *const key = bs->getPrefix<hldb::RefObj>();
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->getName(), "key");
  EXPECT_NE(key->getActual<hldb::TypespecMember>(), nullptr) << "7.2: the structure member";
}

TEST_F(PartSelectNoParentTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
