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

// Validates that an escaped identifier can be referenced by its non-escaped
// name.  The SV source declares the net with a backslash prefix:
//   reg \cpu3 ;
// and then references it without the backslash in a continuous assign:
//   assign reference_test = cpu3;
//
// UHDM structure:
//   Net "cpu3"           — escaped decl stored without backslash
//   Net "reference_test" — plain wire
//   ContAssign:
//     LHS: RefObj "reference_test"
//     RHS: RefObj "cpu3"  ← non-escaped reference resolves to the same net
//
// Key assertion: the RHS of the ContAssign carries name "cpu3", confirming
// that the non-escaped form is treated as identical to the escaped form.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/cont_assign.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>

namespace SURELOG {

class NonescapedAccess : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.1--nonescaped-access.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@identifiers", d->getAllModules());
}

static const uhdm::ContAssign *getContAssign(const uhdm::Design *d) {
  const uhdm::Module *const top = getTop(d);
  if (!top || !top->getContAssigns()) return nullptr;
  return (*top->getContAssigns())[0];
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------
TEST_F(NonescapedAccess, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@identifiers' not found";
}

TEST_F(NonescapedAccess, TwoNetsExist) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 2u);
}

TEST_F(NonescapedAccess, NetCpu3ExistsWithoutBackslash) {
  // \cpu3 is declared with backslash but stored in UHDM as plain "cpu3".
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(uhdm::findByName<uhdm::Net>("cpu3", m->getNets()), nullptr)
      << "net 'cpu3' should exist (escaped identifier stored without backslash)";
}

TEST_F(NonescapedAccess, NetReferenceTestExists) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(uhdm::findByName<uhdm::Net>("reference_test", m->getNets()), nullptr)
      << "net 'reference_test' not found";
}

// ---------------------------------------------------------------------------
// Continuous assign: assign reference_test = cpu3
// ---------------------------------------------------------------------------
TEST_F(NonescapedAccess, OneContAssignExists) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  EXPECT_EQ(m->getContAssigns()->size(), 1u);
}

TEST_F(NonescapedAccess, ContAssignLhsIsReferenceTest) {
  const uhdm::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const uhdm::RefObj *const lhs = ca->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS should be a RefObj";
  EXPECT_EQ(lhs->getName(), "reference_test");
}

TEST_F(NonescapedAccess, ContAssignRhsIsCpu3) {
  // The non-escaped reference 'cpu3' resolves to the escaped-identifier net
  // '\cpu3'.  The RHS RefObj carries name "cpu3" (no backslash).
  const uhdm::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const uhdm::RefObj *const rhs = ca->getRhs<uhdm::RefObj>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS should be a RefObj";
  EXPECT_EQ(rhs->getName(), "cpu3")
      << "non-escaped 'cpu3' should resolve to the \\cpu3 net";
}

TEST_F(NonescapedAccess, ContAssignRhsResolvesToCpu3Net) {
  // Confirm the RHS RefObj's vpiActual points to the cpu3 Net node.
  const uhdm::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const uhdm::RefObj *const rhs = ca->getRhs<uhdm::RefObj>();
  ASSERT_NE(rhs, nullptr);
  const uhdm::Net *const net = any_cast<uhdm::Net>(rhs->getActual());
  ASSERT_NE(net, nullptr) << "RHS vpiActual should resolve to a Net";
  EXPECT_EQ(net->getName(), "cpu3");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
