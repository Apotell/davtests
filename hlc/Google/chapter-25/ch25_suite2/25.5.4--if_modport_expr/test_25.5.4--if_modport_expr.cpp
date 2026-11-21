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

// IEEE 1800-2023 Sec 25.5.4 (modport expressions): ".P(r[3:0])" names a port P bound to a
// part-select of r, ".Q(x)" and ".Q(2)" bind Q to a constant variable and a literal, and the plain
// item R is both a reference to interface item R and the port name. P and Q are visible only
// through the modport connection, each modport list having its own port name space.
// Source: tests/Google/chapter-25/ch25_suite2/25.5.4--if_modport_expr.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class IfModportExprTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.4--if_modport_expr.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getI() {
    return hldb::findByDefName<hldb::Interface>("I", m_design->getAllInterfaces());
  }
  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Interface *const i = getI();
    return (i == nullptr) ? nullptr : hldb::findByName<hldb::Variable>(name, i->getVariables());
  }
  static const hldb::IODecl *getPort(std::string_view mpName, std::string_view port) {
    const hldb::Interface *const i = getI();
    if (i == nullptr) return nullptr;
    const hldb::Modport *const mp = hldb::findByName<hldb::Modport>(mpName, i->getModports());
    return (mp == nullptr) ? nullptr : hldb::findByName<hldb::IODecl>(port, mp->getIODecls());
  }
  // P is bound to r[left:right].
  static void expectPartSelectOfR(std::string_view mpName, std::string_view left, std::string_view right) {
    const hldb::IODecl *const p = getPort(mpName, "P");
    ASSERT_NE(p, nullptr) << mpName;
    const hldb::PartSelect *const ps = p->getExpr<hldb::PartSelect>();
    ASSERT_NE(ps, nullptr) << mpName << ".P is not bound to a part-select";
    const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(ps->getPrefix());
    ASSERT_NE(prefix, nullptr) << mpName;
    ASSERT_NE(getVar("r"), nullptr);
    EXPECT_EQ(prefix->getActual(), getVar("r")) << mpName;
    ASSERT_NE(ps->getRange(), nullptr) << mpName;
    const hldb::Constant *const l = ps->getRange()->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = ps->getRange()->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr) << mpName;
    ASSERT_NE(r, nullptr) << mpName;
    EXPECT_EQ(l->getDecompile(), left) << mpName;
    EXPECT_EQ(r->getDecompile(), right) << mpName;
  }
};

TEST_F(IfModportExprTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportExprTest, ModportPortsAreP_Q_R) {
  // Lines 27-28: both modports declare ports P (output), Q (input) and R (input), in that order.
  const hldb::Interface *const i = getI();
  ASSERT_NE(i, nullptr);
  for (std::string_view mpName : {"A", "B"}) {
    const hldb::Modport *const mp = hldb::findByName<hldb::Modport>(mpName, i->getModports());
    ASSERT_NE(mp, nullptr) << mpName;
    ASSERT_NE(mp->getIODecls(), nullptr) << mpName;
    ASSERT_EQ(mp->getIODecls()->size(), 3u) << mpName;
    EXPECT_EQ(mp->getIODecls()->at(0)->getName(), std::string_view("P")) << mpName;
    EXPECT_EQ(mp->getIODecls()->at(0)->getDirection(), vpiOutput) << mpName;
    EXPECT_EQ(mp->getIODecls()->at(1)->getName(), std::string_view("Q")) << mpName;
    EXPECT_EQ(mp->getIODecls()->at(1)->getDirection(), vpiInput) << mpName;
    EXPECT_EQ(mp->getIODecls()->at(2)->getName(), std::string_view("R")) << mpName;
    EXPECT_EQ(mp->getIODecls()->at(2)->getDirection(), vpiInput) << mpName;
  }
}

TEST_F(IfModportExprTest, APIsLowNibbleOfR) {
  // Line 27: ".P(r[3:0])"
  expectPartSelectOfR("A", "3", "0");
}

TEST_F(IfModportExprTest, BPIsHighNibbleOfR) {
  // Line 28: ".P(r[7:4])"
  expectPartSelectOfR("B", "7", "4");
}

TEST_F(IfModportExprTest, AQIsConstantVariableX) {
  // Line 27: ".Q(x)" where line 25 declares "const int x = 1;".
  const hldb::IODecl *const q = getPort("A", "Q");
  ASSERT_NE(q, nullptr);
  const hldb::RefObj *const ref = q->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  ASSERT_NE(getVar("x"), nullptr);
  EXPECT_EQ(ref->getActual(), getVar("x"));
}

TEST_F(IfModportExprTest, BQIsLiteral2) {
  // Line 28: ".Q(2)"
  const hldb::IODecl *const q = getPort("B", "Q");
  ASSERT_NE(q, nullptr);
  const hldb::Constant *const c = q->getExpr<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "B.Q is not bound to a constant";
  EXPECT_EQ(c->getDecompile(), std::string_view("2"));
}

TEST_F(IfModportExprTest, RIsInterfaceVariableR) {
  // Lines 26-28: the simple item R references "bit R;" in both modports.
  ASSERT_NE(getVar("R"), nullptr);
  for (std::string_view mpName : {"A", "B"}) {
    const hldb::IODecl *const r = getPort(mpName, "R");
    ASSERT_NE(r, nullptr) << mpName;
    const hldb::RefObj *const ref = r->getExpr<hldb::RefObj>();
    ASSERT_NE(ref, nullptr) << mpName;
    EXPECT_EQ(ref->getActual(), getVar("R")) << mpName;
  }
}

TEST_F(IfModportExprTest, PAndQAreNotInterfaceItems) {
  // P and Q are port identifiers of the modport lists only; I declares no items with those names.
  const hldb::Interface *const i = getI();
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(getVar("P"), nullptr);
  EXPECT_EQ(getVar("Q"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
