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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>

namespace hlc {

class NonescapedAccess : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.1--nonescaped-access.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("identifiers", d->getAllModules());
}

static const hldb::ContAssign *getContAssign(const hldb::Design *d) {
  const hldb::Module *const top = getTop(d);
  if (!top || !top->getContAssigns()) return nullptr;
  return (*top->getContAssigns())[0];
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------
TEST_F(NonescapedAccess, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'identifiers' not found";
}

TEST_F(NonescapedAccess, TwoNetsExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 2u);
}

TEST_F(NonescapedAccess, NetCpu3ExistsWithoutBackslash) {
  // \cpu3 is declared with backslash but stored in UHDM as plain "cpu3".
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("cpu3", m->getNets()), nullptr)
      << "net 'cpu3' should exist (escaped identifier stored without backslash)";
}

TEST_F(NonescapedAccess, NetReferenceTestExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("reference_test", m->getNets()), nullptr) << "net 'reference_test' not found";
}

// ---------------------------------------------------------------------------
// Continuous assign: assign reference_test = cpu3
// ---------------------------------------------------------------------------
TEST_F(NonescapedAccess, OneContAssignExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  EXPECT_EQ(m->getContAssigns()->size(), 1u);
}

TEST_F(NonescapedAccess, ContAssignLhsIsReferenceTest) {
  const hldb::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS should be a RefObj";
  EXPECT_EQ(lhs->getName(), "reference_test");
}

TEST_F(NonescapedAccess, ContAssignRhsIsCpu3) {
  // The non-escaped reference 'cpu3' resolves to the escaped-identifier net
  // '\cpu3'.  The RHS RefObj carries name "cpu3" (no backslash).
  const hldb::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS should be a RefObj";
  EXPECT_EQ(rhs->getName(), "cpu3") << "non-escaped 'cpu3' should resolve to the \\cpu3 net";
}

TEST_F(NonescapedAccess, ContAssignRhsResolvesToCpu3Net) {
  // Confirm the RHS RefObj's vpiActual points to the cpu3 Net node.
  const hldb::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  const hldb::Net *const net = any_cast<hldb::Net>(rhs->getActual());
  ASSERT_NE(net, nullptr) << "RHS vpiActual should resolve to a Net";
  EXPECT_EQ(net->getName(), "cpu3");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
