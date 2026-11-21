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

// IEEE 1800-2023 Sec 25.5.4: "The port expression is optional because ports can be defined that do
// not connect to anything internal to the port." ".N()" defines port N with no expression.
// Source: tests/Google/chapter-25/ch25_suite2/25.5.4--if_modport_expr_empty.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class IfModportExprEmptyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.4--if_modport_expr_empty.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getI() {
    return hldb::findByDefName<hldb::Interface>("I", m_design->getAllInterfaces());
  }
  static const hldb::Modport *getA() {
    const hldb::Interface *const i = getI();
    return (i == nullptr) ? nullptr : hldb::findByName<hldb::Modport>("A", i->getModports());
  }
};

TEST_F(IfModportExprEmptyTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportExprEmptyTest, ModportHasPortsPAndN) {
  // Line 25: "modport A (output .P(r), input .N());"
  const hldb::Modport *const a = getA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getIODecls(), nullptr);
  ASSERT_EQ(a->getIODecls()->size(), 2u);
  EXPECT_EQ(a->getIODecls()->at(0)->getName(), std::string_view("P"));
  EXPECT_EQ(a->getIODecls()->at(0)->getDirection(), vpiOutput);
  EXPECT_EQ(a->getIODecls()->at(1)->getName(), std::string_view("N"));
  EXPECT_EQ(a->getIODecls()->at(1)->getDirection(), vpiInput);
}

TEST_F(IfModportExprEmptyTest, PortNHasNoExpression) {
  // Line 25: ".N()" connects to nothing internal to the interface.
  const hldb::Modport *const a = getA();
  ASSERT_NE(a, nullptr);
  const hldb::IODecl *const n = hldb::findByName<hldb::IODecl>("N", a->getIODecls());
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getExpr(), nullptr);
}

TEST_F(IfModportExprEmptyTest, PortPIsVariableR) {
  // Line 24: "logic [7:0] r;" -- ".P(r)" binds P to the whole variable r.
  const hldb::Interface *const i = getI();
  ASSERT_NE(i, nullptr);
  const hldb::Modport *const a = getA();
  ASSERT_NE(a, nullptr);
  const hldb::IODecl *const p = hldb::findByName<hldb::IODecl>("P", a->getIODecls());
  ASSERT_NE(p, nullptr);
  const hldb::RefObj *const ref = p->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Variable>("r", i->getVariables());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(ref->getActual(), expected);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
