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

// Validates the HLDB model built for tests/PartSelectHier/dut.sv:
//
//   package tlul_pkg;
//     typedef struct packed {
//       logic  [10:0]  d_source;
//     } tl_d2h_t;
//   endpackage
//   module dut ();
//     tlul_pkg::tl_d2h_t drsp_fifo_o;
//     logic [5:0] hfifo_rspid;
//     assign hfifo_rspid = drsp_fifo_o.d_source[8:3];
//   endmodule
//
// The fixture is a part-select (11.5.1) of a member of a packed structure
// (7.2) whose type is named through a package scope (26.3). The selected
// bits d_source[8:3] are 6 bits wide, the width of hfifo_rspid.
//
// What is checked, and why:
//   - package tlul_pkg declares the typedef tl_d2h_t (6.18, 26.2)
//   - drsp_fifo_o's type 'tlul_pkg::tl_d2h_t' binds to that typedef, and the
//     prefix 'tlul_pkg' binds to the package (26.3)
//   - d_source is a member of type logic [10:0] (7.2)
//   - hfifo_rspid is logic [5:0]
//   - dut's '()' declares one null port (37.14 detail 10)
//   - the continuous assignment (10.3) targets hfifo_rspid; its RHS names
//     drsp_fifo_o, then the part-select d_source[8:3] whose prefix binds to
//     the structure member and whose bounds are 8 and 3 (11.5.1)
//   - no fatal, syntax or error diagnostic: the file is legal
//
// Model convention: the hierarchical RHS is one RefObj whose path elements
// are the RefObj drsp_fifo_o (bound to the variable) and the PartSelect
// d_source[8:3]; the RefObj itself binds to that PartSelect.
//
// What is NOT checked, and why:
//   - The value hfifo_rspid takes is a runtime property; HLC is a static
//     compiler/elaborator and not a simulator, so this is out of scope
//     permanently. RhsIsPartSelectOfMemberDSource is the static evidence.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PartSelectHierTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelectHier.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Module *const m = getDut();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, m->getVariables());
  }

  static const hldb::RefObj *getRhs() {
    const hldb::Module *const m = getDut();
    if ((m == nullptr) || (m->getContAssigns() == nullptr) || (m->getContAssigns()->size() != 1)) return nullptr;
    return m->getContAssigns()->at(0)->getRhs<hldb::RefObj>();
  }

  static void ExpectConstant(const hldb::Expr *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  static void ExpectLogicRange(const hldb::RefTypespec *rt, std::string_view left, std::string_view right,
                               std::string_view what) {
    ASSERT_NE(rt, nullptr) << what << " has a type";
    const hldb::LogicTypespec *const lt = rt->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << what << " is of type logic";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u) << "7.4.1: " << what << " has one packed dimension";
    ExpectConstant(lt->getRanges()->at(0)->getLeftExpr(), left, "7.4.1: left bound");
    ExpectConstant(lt->getRanges()->at(0)->getRightExpr(), right, "7.4.1: right bound");
  }
};

TEST_F(PartSelectHierTest, PackageDeclaresTlD2hT) {
  const hldb::Package *const p = hldb::findByName<hldb::Package>("tlul_pkg", m_design->getAllPackages());
  ASSERT_NE(p, nullptr) << "26.2: package tlul_pkg";
  EXPECT_NE(hldb::findByName<hldb::TypedefTypespec>("tl_d2h_t", p->getTypespecs()), nullptr)
      << "6.18: the package declares the typedef tl_d2h_t";
}

TEST_F(PartSelectHierTest, DrspFifoOIsPackageScopedTypedef) {
  const hldb::Variable *const v = getVar("drsp_fifo_o");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const td = v->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(td, nullptr) << "26.3: 'tlul_pkg::tl_d2h_t' names the package typedef";
  EXPECT_EQ(td->getName(), "tl_d2h_t");
  const hldb::Package *const p = hldb::findByName<hldb::Package>("tlul_pkg", m_design->getAllPackages());
  EXPECT_EQ(td->getParent(), p) << "26.3: the typedef is the one declared in tlul_pkg";
}

TEST_F(PartSelectHierTest, HfifoRspidIsLogic5To0) {
  const hldb::Variable *const v = getVar("hfifo_rspid");
  ASSERT_NE(v, nullptr);
  ExpectLogicRange(v->getTypespec(), "5", "0", "hfifo_rspid");
}

TEST_F(PartSelectHierTest, DutHasOneNullPort) {
  const hldb::Module *const m = getDut();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u);
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "a null port has no name";
  EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10";
}

TEST_F(PartSelectHierTest, LhsIsHfifoRspid) {
  const hldb::Module *const m = getDut();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_EQ(m->getContAssigns()->size(), 1u) << "10.3: one continuous assignment";
  const hldb::RefObj *const lhs = m->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), getVar("hfifo_rspid"));
}

TEST_F(PartSelectHierTest, RhsIsPartSelectOfMemberDSource) {
  const hldb::RefObj *const rhs = getRhs();
  ASSERT_NE(rhs, nullptr) << "the RHS is a hierarchical name";
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u) << "drsp_fifo_o . d_source[8:3]";
  const hldb::RefObj *const head = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getName(), "drsp_fifo_o");
  EXPECT_EQ(head->getActual(), getVar("drsp_fifo_o"));
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(rhs->getPathElems()->at(1));
  ASSERT_NE(ps, nullptr) << "11.5.1: d_source[8:3] is a part-select";
  const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "d_source");
  const hldb::TypespecMember *const member = prefix->getActual<hldb::TypespecMember>();
  ASSERT_NE(member, nullptr) << "7.2: 'd_source' names the structure member";
  ExpectLogicRange(member->getTypespec(), "10", "0", "member d_source");
  ASSERT_NE(ps->getRange(), nullptr);
  ExpectConstant(ps->getRange()->getLeftExpr(), "8", "11.5.1: msb expression");
  ExpectConstant(ps->getRange()->getRightExpr(), "3", "11.5.1: lsb expression");
  EXPECT_EQ(rhs->getActual(), ps) << "model convention: the name binds to its last element";
}

TEST_F(PartSelectHierTest, NoFatalSyntaxOrErrorDiagnostics) {
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
