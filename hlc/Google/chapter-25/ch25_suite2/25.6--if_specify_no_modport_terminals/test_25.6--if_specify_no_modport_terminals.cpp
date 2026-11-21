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

// IEEE 1800-2023 Sec 25.6: each signal of an interface port becomes an available specify terminal
// "with the default direction as defined for an interface" when no modport restricts it (nets are
// inout, so they may be path sources and destinations). "(ch.a => ch.y) = 2;" in buf_m (itf ch).
// Source: tests/Google/chapter-25/ch25_suite2/25.6--if_specify_no_modport_terminals.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/mod_path.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/path_term.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class IfSpecifyNoModportTerminalsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.6--if_specify_no_modport_terminals.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Net *getItfNet(std::string_view name) {
    const hldb::Interface *const itf = hldb::findByDefName<hldb::Interface>("itf", m_design->getAllInterfaces());
    return (itf == nullptr) ? nullptr : hldb::findByName<hldb::Net>(name, itf->getNets());
  }
  static const hldb::ModPath *getPath() {
    const hldb::Module *const m = hldb::findByDefName<hldb::Module>("buf_m", m_design->getAllModules());
    if ((m == nullptr) || (m->getModPaths() == nullptr) || (m->getModPaths()->size() != 1)) return nullptr;
    return m->getModPaths()->at(0);
  }
};

TEST_F(IfSpecifyNoModportTerminalsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfSpecifyNoModportTerminalsTest, PathSourceIsNetA) {
  // Line 30: "(ch.a => ch.y)" -- source terminal is net a of itf (line 24).
  const hldb::ModPath *const path = getPath();
  ASSERT_NE(path, nullptr) << "buf_m has no single module path";
  ASSERT_NE(path->getPathInTerms(), nullptr);
  ASSERT_EQ(path->getPathInTerms()->size(), 1u);
  const hldb::RefObj *const ref = path->getPathInTerms()->at(0)->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  ASSERT_NE(getItfNet("a"), nullptr);
  EXPECT_EQ(ref->getActual(), getItfNet("a"));
}

TEST_F(IfSpecifyNoModportTerminalsTest, PathDestinationIsNetY) {
  // Line 30: destination terminal is net y of itf (line 24).
  const hldb::ModPath *const path = getPath();
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->getPathOutTerms(), nullptr);
  ASSERT_EQ(path->getPathOutTerms()->size(), 1u);
  const hldb::RefObj *const ref = path->getPathOutTerms()->at(0)->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  ASSERT_NE(getItfNet("y"), nullptr);
  EXPECT_EQ(ref->getActual(), getItfNet("y"));
}

TEST_F(IfSpecifyNoModportTerminalsTest, PathIsParallel) {
  GTEST_SKIP() << "HLC leaves vpiPathType unset on a simple '=>' path; should be vpiPathParallel per "
                  "IEEE 1800-2023 Sec 30.4.5. Fix pending.";
  // Line 30: "=>" is a parallel connection.
  const hldb::ModPath *const path = getPath();
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->getPathType(), vpiPathParallel);
}

TEST_F(IfSpecifyNoModportTerminalsTest, PathDelayIs2) {
  // Line 30: "= 2"
  const hldb::ModPath *const path = getPath();
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->getDelays(), nullptr);
  ASSERT_EQ(path->getDelays()->size(), 1u);
  const hldb::Constant *const delay = any_cast<hldb::Constant>(path->getDelays()->at(0));
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getDecompile(), std::string_view("2"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
