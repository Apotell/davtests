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

// Tests for 18.5--constraint-blocks_0.sv (tags: 18.5)
//   class a;
//       rand int b;
//       constraint c { b == 0; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5, "Constraint blocks":
// "constraint_declaration ::= [ static ] constraint
// [ dynamic_override_specifiers ] constraint_identifier constraint_block".
// "The values of random variables are determined using constraint
// expressions that are declared using constraint blocks. Constraint blocks
// are class members, like tasks, functions, and variables." c is a plain
// (not pure, not static) constraint block holding a single
// expression_or_dist item, "b == 0".
// Sec 18.9: "All constraints are initially active", so c is enabled.
//
// Expectations are derived from the .sv source, the LRM and the VPI object
// model (Clause 37) only (the golden .log was deliberately not consulted).
//
// Checked:
//   - design has class "a" (Sec 8.3, a user-defined class) with exactly 1
//     variable, b: typed by a signed IntTypespec (Sec 6.11), randomized
//     with rand type vpiRand (Sec 18.4), and public (Sec 8.18)
//   - class "a" has exactly 1 constraint, c, which is not pure
//     (Sec 18.5.1), not virtual, and enabled (Sec 18.9)
//   - c holds exactly 1 constraint item, "b == 0": an Operation vpiEqOp
//     with exactly 2 operands, a RefObj "b" bound to the class Variable b
//     and a Constant "0"
//   - compiler reports zero fatal, syntax and error diagnostics (the source
//     is legal SystemVerilog)
//
// KNOWN FAILURE: on HLC 577, 2 of the tests fail:
//   - CIsPlainEnabledConstraint: c has vpiIsConstraintEnabled false,
//     although Sec 18.9 says all constraints are initially active.
//   - CompilerReportsZeroErrors: HLC reports 1 error, [ERR:DB2038] at
//     line 18 ("constraint c { b == 0; }"): "'vpiConstraintItem' property
//     value has invalid objects" -- the HLDB checker rejects the plain
//     expression "b == 0" as a constraint item.
//
// NOT CHECKED:
//   - the static / dynamic_override properties of c -- the source uses
//     neither keyword, and the VPI encoding of their absence is a compiler
//     choice.
//   - the constant type (vpiConstType) and size of the integer literal.
//     Sec 5.7.1 only says an unsized decimal number is a signed integer of
//     at least 32 bits, so the literal is identified by its decompiled text
//     only.
//   - runtime effects (that randomize() sets b to 0) cannot be observed --
//     HLC is a compiler/elaborator with no simulation capability, so no
//     execution ever happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ConstraintBlocks0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5--constraint-blocks_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClass() { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }

  // "rand int b;"
  static const hldb::Variable *getVar() {
    const hldb::ClassDefn *const cls = getClass();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("b", cls->getVariables());
  }

  static const hldb::Constraint *getConstraint() {
    const hldb::ClassDefn *const cls = getClass();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  }

  // "b == 0" -- the single item of constraint c.
  static const hldb::Operation *getEquality() {
    const hldb::Constraint *const constraint = getConstraint();
    if (constraint == nullptr || constraint->getConstraintItems() == nullptr) {
      return nullptr;
    }
    if (constraint->getConstraintItems()->size() != 1u) {
      return nullptr;
    }
    return any_cast<hldb::Operation>(constraint->getConstraintItems()->at(0));
  }
};

// --- class a; rand int b; ----------------------------------------------------

TEST_F(ConstraintBlocks0Test, ClassExists) {
  const hldb::ClassDefn *const cls = getClass();
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass);
}

TEST_F(ConstraintBlocks0Test, ClassHasOneVariable) {
  const hldb::ClassDefn *const cls = getClass();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u) << "'rand int b;' declares one variable";
}

TEST_F(ConstraintBlocks0Test, BIsRandInt) {
  const hldb::Variable *const var = getVar();
  ASSERT_NE(var, nullptr) << "'b' should be a Variable of class 'a'";
  ASSERT_NE(var->getTypespec(), nullptr);
  const hldb::IntTypespec *const intTs = var->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(intTs, nullptr) << "'int' should have an IntTypespec";
  EXPECT_TRUE(intTs->getSigned()) << "6.11: 'int' is a signed type";
  EXPECT_TRUE(var->getIsRandomized());
  EXPECT_EQ(var->getRandType(), vpiRand) << "18.4: 'rand' modifier";
  EXPECT_EQ(var->getVisibility(), vpiPublicVis) << "8.18: members are public by default";
}

// --- constraint c { b == 0; } ------------------------------------------------

TEST_F(ConstraintBlocks0Test, ClassHasOneConstraint) {
  const hldb::ClassDefn *const cls = getClass();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  EXPECT_EQ(cls->getConstraints()->size(), 1u);
  EXPECT_NE(getConstraint(), nullptr);
}

TEST_F(ConstraintBlocks0Test, CIsPlainEnabledConstraint) {
  const hldb::Constraint *const constraint = getConstraint();
  ASSERT_NE(constraint, nullptr);
  EXPECT_FALSE(constraint->getPure()) << "18.5.1: 'c' is not declared 'pure'";
  EXPECT_FALSE(constraint->getVirtual()) << "'c' is a plain constraint block";
  EXPECT_TRUE(constraint->getIsConstraintEnabled()) << "18.9: all constraints are initially active";
}

TEST_F(ConstraintBlocks0Test, CIsBEquals0) {
  const hldb::Constraint *const constraint = getConstraint();
  ASSERT_NE(constraint, nullptr);
  ASSERT_NE(constraint->getConstraintItems(), nullptr);
  ASSERT_EQ(constraint->getConstraintItems()->size(), 1u) << "'c' holds a single constraint item";

  const hldb::Operation *const op = getEquality();
  ASSERT_NE(op, nullptr) << "'b == 0' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiEqOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u) << "'==' is a binary operator";

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr) << "'b' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "b");
  ASSERT_NE(getVar(), nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVar()) << "'b' should bind to the class Variable";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr) << "'0' should be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "0");
}

// --- compiler diagnostics -----------------------------------------------------

TEST_F(ConstraintBlocks0Test, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
