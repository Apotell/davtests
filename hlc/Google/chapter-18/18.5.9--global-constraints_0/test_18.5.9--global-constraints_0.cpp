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

// Tests for 18.5.9--global-constraints_0.sv (tags: 18.5.9)
//   class a;
//       rand int v;
//   endclass
//
//   class b;
//       rand a aObj;
//       rand int v;
//
//       constraint c { aObj.v < v; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.9, "Global constraints":
// "When an object member of a class is declared rand, all of its
// constraints and random variables are randomized simultaneously along with
// the other class variables and constraints. Constraint expressions
// involving random variables from other objects are called global
// constraints." Here "rand a aObj;" makes the object member random, and c
// relates a variable of that object (aObj.v) to a variable of b (v).
// Sec 8.22 / Sec 23.7: the unqualified name "v" inside class b resolves to
// b's own member v, while "aObj.v" selects member v of the object aObj, of
// class a.
//
// Expectations are derived from the .sv source and the LRM only (the golden
// .log was deliberately not consulted). "aObj.v" is expected as a
// hierarchical reference whose path elements are the references "aObj" and
// "v" (the same shape the chapter-18 tests 18.5.8.2_0/_1 confirm for
// "B.sum()" / "obj.B.sum()").
//
// Checked:
//   class a:
//   - design has class "a" (Sec 8.3, a user-defined class) with exactly 1
//     variable, v: typed by a signed IntTypespec (Sec 6.11), randomized
//     with rand type vpiRand (Sec 18.4), and public (Sec 8.18)
//   - class "a" has no constraints
//   class b:
//   - design has class "b" (a user-defined class) with exactly 2
//     variables, aObj and v
//   - aObj is randomized with rand type vpiRand, public, and typed by a
//     ClassTypespec that resolves to the ClassDefn "a" (Sec 18.5.9: a rand
//     object handle)
//   - b's v is a rand, public, signed int, and is a different Variable from
//     a's v
//   - class "b" has exactly 1 constraint, c, holding exactly 1 constraint
//     item, "aObj.v < v": an Operation vpiLtOp with exactly 2 operands
//   - the left operand "aObj.v" has exactly 2 path elements: a RefObj
//     "aObj" bound to b's Variable aObj, and a RefObj "v" bound to a's
//     Variable v
//   - the right operand is a RefObj "v" bound to b's Variable v (not a's)
//   - compiler reports zero fatal, syntax and error diagnostics (the source
//     is legal SystemVerilog)
//
// KNOWN FAILURE: on HLC 577, 1 of the tests fails:
//   - CompilerReportsZeroErrors: HLC reports 1 error, [ERR:DB2038] at
//     line 24 ("constraint c { aObj.v < v; }"): "'vpiConstraintItem'
//     property value has invalid objects" -- the HLDB checker rejects the
//     plain expression "aObj.v < v" as a constraint item.
//
// NOT CHECKED:
//   - runtime effects (that randomizing a b object also randomizes aObj.v
//     and solves aObj.v < v jointly) cannot be observed -- HLC is a
//     compiler/elaborator with no simulation capability, so no execution
//     ever happens for this test to check. No object is ever constructed
//     in this source either.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
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

class GlobalConstraints0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.9--global-constraints_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }

  static const hldb::Variable *getVar(std::string_view className, std::string_view varName) {
    const hldb::ClassDefn *const cls = getClass(className);
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(varName, cls->getVariables());
  }

  static const hldb::Constraint *getConstraint() {
    const hldb::ClassDefn *const cls = getClass("b");
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  }

  // "aObj.v < v" -- the single item of constraint c.
  static const hldb::Operation *getLessThan() {
    const hldb::Constraint *const constraint = getConstraint();
    if (constraint == nullptr || constraint->getConstraintItems() == nullptr) {
      return nullptr;
    }
    if (constraint->getConstraintItems()->size() != 1u) {
      return nullptr;
    }
    return any_cast<hldb::Operation>(constraint->getConstraintItems()->at(0));
  }

  // Operand operandIndex of "aObj.v < v".
  static const hldb::Any *getOperand(size_t operandIndex) {
    const hldb::Operation *const op = getLessThan();
    if (op == nullptr || op->getOperands() == nullptr) {
      return nullptr;
    }
    if (op->getOperands()->size() != 2u) {
      return nullptr;
    }
    return op->getOperands()->at(operandIndex);
  }

  // "rand int <varName>;" in class className.
  static void expectRandInt(std::string_view className, std::string_view varName) {
    const hldb::Variable *const var = getVar(className, varName);
    ASSERT_NE(var, nullptr) << "'" << varName << "' should be a Variable of class '" << className << "'";

    const hldb::RefTypespec *const ref = var->getTypespec();
    ASSERT_NE(ref, nullptr);
    const hldb::IntTypespec *const intTs = ref->getActual<hldb::IntTypespec>();
    ASSERT_NE(intTs, nullptr) << "'int' should have an IntTypespec";
    EXPECT_TRUE(intTs->getSigned()) << "6.11: 'int' is a signed type";

    EXPECT_TRUE(var->getIsRandomized());
    EXPECT_EQ(var->getRandType(), vpiRand) << "18.4: 'rand' modifier";
    EXPECT_EQ(var->getVisibility(), vpiPublicVis) << "8.18: members are public by default";
  }

  // A reference "name" bound to the Variable var.
  static void expectRefTo(const hldb::Any *elem, std::string_view name, const hldb::Variable *var) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(ref->getActual<hldb::Variable>(), var) << "'" << name << "' should bind to its Variable";
  }
};

// --- class a; rand int v; endclass -------------------------------------------

TEST_F(GlobalConstraints0Test, ClassAExists) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass);
}

TEST_F(GlobalConstraints0Test, ClassAHasOneVariable) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u) << "'rand int v;' declares one variable";
}

TEST_F(GlobalConstraints0Test, AVIsRandInt) { expectRandInt("a", "v"); }

TEST_F(GlobalConstraints0Test, ClassAHasNoConstraints) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  EXPECT_TRUE(cls->getConstraints() == nullptr || cls->getConstraints()->empty())
      << "class 'a' declares no constraint blocks";
}

// --- class b; rand a aObj; rand int v; ---------------------------------------

TEST_F(GlobalConstraints0Test, ClassBExists) {
  const hldb::ClassDefn *const cls = getClass("b");
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass);
}

TEST_F(GlobalConstraints0Test, ClassBHasTwoVariables) {
  const hldb::ClassDefn *const cls = getClass("b");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u) << "'rand a aObj;' and 'rand int v;'";
}

TEST_F(GlobalConstraints0Test, AObjIsRandHandleOfClassA) {
  const hldb::Variable *const aObj = getVar("b", "aObj");
  ASSERT_NE(aObj, nullptr) << "'aObj' should be a Variable of class 'b'";
  EXPECT_TRUE(aObj->getIsRandomized());
  EXPECT_EQ(aObj->getRandType(), vpiRand) << "18.5.9: 'rand' on an object member";
  EXPECT_EQ(aObj->getVisibility(), vpiPublicVis) << "8.18: members are public by default";

  ASSERT_NE(aObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const classTs = aObj->getTypespec()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(classTs, nullptr) << "'a aObj' should be typed by a ClassTypespec";
  ASSERT_NE(getClass("a"), nullptr);
  EXPECT_EQ(classTs->getClassDefn(), getClass("a")) << "'aObj' should be a handle of class 'a'";
}

TEST_F(GlobalConstraints0Test, BVIsRandInt) { expectRandInt("b", "v"); }

TEST_F(GlobalConstraints0Test, BVIsDistinctFromAV) {
  ASSERT_NE(getVar("a", "v"), nullptr);
  ASSERT_NE(getVar("b", "v"), nullptr);
  EXPECT_NE(getVar("a", "v"), getVar("b", "v")) << "classes 'a' and 'b' each declare their own 'v'";
}

// --- constraint c { aObj.v < v; } --------------------------------------------

TEST_F(GlobalConstraints0Test, ClassBHasOneConstraint) {
  const hldb::ClassDefn *const cls = getClass("b");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  EXPECT_EQ(cls->getConstraints()->size(), 1u);
  EXPECT_NE(getConstraint(), nullptr);
}

TEST_F(GlobalConstraints0Test, CIsLessThan) {
  const hldb::Constraint *const constraint = getConstraint();
  ASSERT_NE(constraint, nullptr);
  ASSERT_NE(constraint->getConstraintItems(), nullptr);
  ASSERT_EQ(constraint->getConstraintItems()->size(), 1u);

  const hldb::Operation *const op = getLessThan();
  ASSERT_NE(op, nullptr) << "'aObj.v < v' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiLtOp);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "'<' is a binary operator";
}

TEST_F(GlobalConstraints0Test, LhsIsAObjDotV) {
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(getOperand(0));
  ASSERT_NE(lhs, nullptr) << "'aObj.v' should be a hierarchical reference";
  ASSERT_NE(lhs->getPathElems(), nullptr) << "'aObj.v' should hold the path 'aObj' . 'v'";
  ASSERT_EQ(lhs->getPathElems()->size(), 2u) << "'aObj' and 'v'";
  expectRefTo(lhs->getPathElems()->at(0), "aObj", getVar("b", "aObj"));
  expectRefTo(lhs->getPathElems()->at(1), "v", getVar("a", "v"));
}

TEST_F(GlobalConstraints0Test, RhsIsBV) { expectRefTo(getOperand(1), "v", getVar("b", "v")); }

// --- compiler diagnostics -----------------------------------------------------

TEST_F(GlobalConstraints0Test, CompilerReportsZeroErrors) {
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
