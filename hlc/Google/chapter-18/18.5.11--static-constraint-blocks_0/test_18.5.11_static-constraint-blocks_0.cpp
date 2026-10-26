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

// ---------------------------------------------------------------------------
// SV source under test: tests/Google/chapter-18/18.5.11--static-constraint-blocks_0.sv
// (tag 18.5.11 is the IEEE 1800-2017 number; the same clause is 18.5.10
// "Static constraint blocks" in IEEE 1800-2023, which is the text used here)
//
//   class a;
//       rand int b;
//
//       static constraint c1 { b == 5; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal (Syntax 18-9:
//      "[ static ] constraint constraint_identifier constraint_block",
//      no dynamic_override_specifiers used) -> no fatal/syntax/error.
//   2. Class 'a' exists; it declares exactly one variable b which is
//      'rand int' -> vpiRandType == vpiRand, int typespec.
//   3. Class 'a' has exactly one constraint 'c1':
//        - parent is class 'a' (37.34 vpiParent),
//        - it is STATIC: 37.34 detail 1 says "vpiAutomatic == 0 implies the
//          constraint was declared static", so getAutomatic() must be false.
//          This is THE corner of this file.
//        - active by default (18.9 "All constraints are initially active"),
//        - in-body, so vpiAccessType is 0 (37.34 detail 3), not virtual,
//          not pure.
//   4. c1 has one item, the hard equality "b == 5": vpiEqOp, not soft,
//      left operand binds to variable b, right operand is the literal 5.
//
// CHECKED (TEST_F below):
//   - items 1 to 4 above.
//
// NOT CHECKED (and why):
//   - "calls to constraint_mode() shall affect all instances of the
//     specified constraint in all objects" -- that is run-time behavior and
//     this file has no object and no constraint_mode() call (see
//     static-constraint-blocks_1 for the run-time variant).
//   - The extern/prototype "static on both or neither" rule -- this file has
//     no constraint prototype.
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace {
hldb::AnyType typespecKind(const hldb::Variable *var) {
  const hldb::RefTypespec *const ref = var->getTypespec();
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

// Checks that 'item' is the relation "<var> <opType> <literal>" with the given soft flag.
void expectRelationOnLiteral(const hldb::Any *item, int32_t opType, const hldb::Variable *var, std::string_view literal,
                             bool soft) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(item);
  ASSERT_NE(op, nullptr) << "constraint item should be an Operation";
  EXPECT_EQ(op->getOpType(), opType);
  EXPECT_EQ(op->getSoft(), soft) << "soft flag (37.38 vpiSoft)";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(ref, nullptr) << "left operand should reference '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "'" << ref->getName() << "' binds to the variable";
  const hldb::Constant *const value = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(value, nullptr) << "right operand should be the literal " << literal;
  EXPECT_EQ(value->getDecompile(), literal);
}
}  // namespace

namespace hlc {
class StaticConstraintBlocks0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.11--static-constraint-blocks_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClassA() const { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }
};

// Check 1: legal file.
TEST_F(StaticConstraintBlocks0Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "'static constraint' is legal per IEEE 1800-2023 Sec 18.5.10 Syntax 18-9";
}

// Check 2: class 'a' with 'rand int b'.
TEST_F(StaticConstraintBlocks0Test, ClassAHasRandIntB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr) << "variable 'b' not found";
  EXPECT_EQ(b->getRandType(), vpiRand);
  EXPECT_EQ(typespecKind(b), hldb::AnyType::IntTypespec);
}

// Check 3: c1 is the only constraint and it is static (vpiAutomatic == 0).
TEST_F(StaticConstraintBlocks0Test, C1IsStaticConstraint) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 1u);
  const hldb::Constraint *const c1 = cls->getConstraints()->at(0);
  EXPECT_EQ(c1->getName(), "c1");
  EXPECT_EQ(c1->getParent(), static_cast<const hldb::Any *>(cls));
  EXPECT_FALSE(c1->getAutomatic()) << "c1 is declared 'static': IEEE 1800-2023 Sec 37.34 detail 1 says "
                                      "vpiAutomatic == 0 implies the constraint was declared static";
  EXPECT_TRUE(c1->getIsConstraintEnabled()) << "constraints are initially active (18.9)";
  EXPECT_EQ(c1->getAccessType(), 0) << "declared in-body, not extern";
  EXPECT_FALSE(c1->getVirtual());
  EXPECT_FALSE(c1->getPure());
}

// Check 4: c1 is the hard equality "b == 5".
TEST_F(StaticConstraintBlocks0Test, C1IsBEqualsFive) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 1u);
  expectRelationOnLiteral(c1->getConstraintItems()->at(0), vpiEqOp, b, "5", false);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
