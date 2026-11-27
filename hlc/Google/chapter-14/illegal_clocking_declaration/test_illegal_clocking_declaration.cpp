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

// IEEE 1800-2023 Sec 14.12: at most one default clocking per module, interface, program or checker.
// Sec 14.14: at most one global clocking per module, interface or checker. The second one is an
// error, reported where it is written: a second declaration (cb2, g2, late, ib, pb, kb, kg2) or a
// "default clocking <name>;" naming a different block (other). The first one stays the scope's
// default or global clocking. m_one_each has one of each, which is legal.
// Arguments: illegal_clocking_declaration.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/program.h>
#include <hldb/unit.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class IllegalClockingDeclarationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_clocking_declaration.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *findModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::CheckerDecl *findChecker(std::string_view name) {
    if (m_design->getUnits() == nullptr) return nullptr;
    for (const hldb::Unit *unit : *m_design->getUnits()) {
      if (const hldb::CheckerDecl *const checker = hldb::findByName<hldb::CheckerDecl>(name, unit->getCheckerDecls())) {
        return checker;
      }
    }
    return nullptr;
  }
};

TEST_F(IllegalClockingDeclarationTest, SecondDefaultClockingInModule) {
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "cb2", 20, 3), nullptr);
  const hldb::Module *const m = findModule("m_two_default");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getDefaultClocking(), nullptr);
  EXPECT_EQ(m->getDefaultClocking()->getName(), std::string_view("cb1"));
}

TEST_F(IllegalClockingDeclarationTest, SecondGlobalClockingInModule) {
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "g2", 25, 3), nullptr);
  const hldb::Module *const m = findModule("m_two_global");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGlobalClocking(), nullptr);
  EXPECT_EQ(m->getGlobalClocking()->getName(), std::string_view("g1"));
}

TEST_F(IllegalClockingDeclarationTest, DefaultClockingReferenceAfterDeclaration) {
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "other", 31, 20), nullptr);
  const hldb::Module *const m = findModule("m_decl_then_ref");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getDefaultClocking(), nullptr);
  EXPECT_EQ(m->getDefaultClocking()->getName(), std::string_view("d1"));
}

TEST_F(IllegalClockingDeclarationTest, DefaultClockingDeclarationAfterReference) {
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "late", 37, 3), nullptr);
  const hldb::Module *const m = findModule("m_ref_then_decl");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getDefaultClocking(), nullptr);
  EXPECT_EQ(m->getDefaultClocking()->getName(), std::string_view("c0"));
}

TEST_F(IllegalClockingDeclarationTest, OneDefaultAndOneGlobalClocking) {
  const hldb::Module *const m = findModule("m_one_each");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getDefaultClocking(), nullptr);
  EXPECT_EQ(m->getDefaultClocking()->getName(), std::string_view("dd"));
  ASSERT_NE(m->getGlobalClocking(), nullptr);
  EXPECT_EQ(m->getGlobalClocking()->getName(), std::string_view("gg"));
}

TEST_F(IllegalClockingDeclarationTest, SecondDefaultClockingInInterface) {
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "ib", 47, 3), nullptr);
  const hldb::Interface *const i =
      hldb::findByDefName<hldb::Interface>("i_two_default", m_design->getAllInterfaces());
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->getDefaultClocking(), nullptr);
  EXPECT_EQ(i->getDefaultClocking()->getName(), std::string_view("ia"));
}

TEST_F(IllegalClockingDeclarationTest, SecondDefaultClockingInProgram) {
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "pb", 52, 3), nullptr);
  const hldb::Program *const p = hldb::findByDefName<hldb::Program>("p_two_default", m_design->getAllPrograms());
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getDefaultClocking(), nullptr);
  EXPECT_EQ(p->getDefaultClocking()->getName(), std::string_view("pa"));
}

TEST_F(IllegalClockingDeclarationTest, SecondDefaultAndGlobalClockingInChecker) {
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "kb", 57, 3), nullptr);
  EXPECT_NE(findEvent(EventType::CompIllegalClockingDeclaration, "kg2", 59, 3), nullptr);
  const hldb::CheckerDecl *const k = findChecker("k_two");
  ASSERT_NE(k, nullptr);
  ASSERT_NE(k->getDefaultClocking(), nullptr);
  EXPECT_EQ(k->getDefaultClocking()->getName(), std::string_view("ka"));
  ASSERT_NE(k->getGlobalClocking(), nullptr);
  EXPECT_EQ(k->getGlobalClocking()->getName(), std::string_view("kg1"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
