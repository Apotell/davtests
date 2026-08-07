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
// name.  The SV source declares the variable with a backslash prefix:
//   reg \cpu3 ;
// and then references it without the backslash in a continuous assign:
//   assign reference_test = cpu3;
//
// UHDM structure:
//   Variable "cpu3"           -- escaped decl stored without backslash
//   Net "reference_test"      -- explicit wire declaration
//   ContAssign:
//     LHS: RefObj "reference_test"
//     RHS: RefObj "cpu3"  <- non-escaped reference resolves to the same variable
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
#include <hldb/variable.h>

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

// ----
// Module and variables
// ----
TEST_F(NonescapedAccess, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'identifiers' not found"; }

TEST_F(NonescapedAccess, OneVariableAndOneNetExist) {
  // `default_nettype none` only suppresses implicit nets; it has no effect on
  // explicit declarations. `reg \cpu3;` is an explicit variable declaration,
  // and `wire reference_test;` is an explicit net declaration.
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 1u);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u);
}

TEST_F(NonescapedAccess, VariableCpu3ExistsWithoutBackslash) {
  // \cpu3 is declared with backslash but stored in UHDM as plain "cpu3".
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("cpu3", m->getVariables()), nullptr)
      << "variable 'cpu3' should exist (escaped identifier stored without backslash)";
}

TEST_F(NonescapedAccess, NetReferenceTestExists) {
  // wire reference_test; is an explicit net declaration -- modeled as a Net,
  // not a Variable.
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("reference_test", m->getNets()), nullptr)
      << "net 'reference_test' not found";
}

TEST_F(NonescapedAccess, NetReferenceTestHasWireNetType) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("reference_test", m->getNets());
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getNetType(), vpiWire) << "'wire reference_test' should have net_type vpiWire";
}

TEST_F(NonescapedAccess, Cpu3IsNotDuplicatedAsNet) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("cpu3", m->getNets()), nullptr)
        << "'cpu3' is a reg (variable), must not also appear as a Net";
  }
}

TEST_F(NonescapedAccess, ReferenceTestIsNotDuplicatedAsVariable) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getVariables() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>("reference_test", m->getVariables()), nullptr)
        << "'reference_test' is a wire (net), must not also appear as a Variable";
  }
}

// ----
// Continuous assign: assign reference_test = cpu3
// ----
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
  // The non-escaped reference 'cpu3' resolves to the escaped-identifier variable
  // '\cpu3'.  The RHS RefObj carries name "cpu3" (no backslash).
  const hldb::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS should be a RefObj";
  EXPECT_EQ(rhs->getName(), "cpu3") << "non-escaped 'cpu3' should resolve to the \\cpu3 variable";
}

TEST_F(NonescapedAccess, ContAssignRhsResolvesToCpu3Variable) {
  // Confirm the RHS RefObj's vpiActual points to the cpu3 Variable node.
  const hldb::ContAssign *const ca = getContAssign(m_design);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  const hldb::Variable *const var = any_cast<hldb::Variable>(rhs->getActual());
  ASSERT_NE(var, nullptr) << "RHS vpiActual should resolve to a Variable";
  EXPECT_EQ(var->getName(), "cpu3");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
