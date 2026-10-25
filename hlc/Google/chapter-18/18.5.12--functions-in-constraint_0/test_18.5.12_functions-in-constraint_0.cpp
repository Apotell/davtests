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
// SV source under test: tests/Google/chapter-18/18.5.12--functions-in-constraint_0.sv
// (IEEE 1800-2017 18.5.12 == IEEE 1800-2023 18.5.11 "Functions in
// constraints", which is the text used here)
//
//   class a;
//       rand int b1, b2;
//       function int F (input int d);
//           F=d;
//       endfunction
//
//       constraint c1 { b1 == 5; }
//       constraint c2 { b2 == F(b1); }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error. F obeys every 18.5.11
//      restriction: only an input argument, no side effects, automatic.
//   2. Class 'a' declares exactly b1 and b2 from ONE declaration
//      'rand int b1, b2;' -> both vpiRand, both int typespec (the 'rand'
//      qualifier applies to every name in the list).
//   3. Method F:
//        - is a Function (not a task) whose return type is int,
//        - is automatic: 8.6 "The lifetime of methods declared as part of a
//          class type shall be automatic", which is also what 18.5.11
//          requires ("shall be automatic ... and have no side effects"),
//        - has exactly one argument 'd', direction input, type int
//          (18.5.11: "shall not contain output, inout, or ref arguments"),
//        - its body is the single assignment "F = d" (return through the
//          function name), lhs F and rhs d.
//   4. Constraint order c1, c2 (37.34 detail 4); parent 'a'; non-static
//      (vpiAutomatic == 1, 37.34 detail 1); active; in-body.
//   5. c1 is the hard equality "b1 == 5".
//   6. c2 is the hard equality "b2 == F(b1)":
//        - left operand binds to b2,
//        - right operand is a call named F that is bound at compile time to
//          the class method F (the declaration it calls),
//        - the call has exactly one argument, a reference bound to b1.
//
// CHECKED (TEST_F below):
//   - items 1 to 6.
//
// NOT CHECKED (and why):
//   - Implicit variable ordering: "Random variables used as function
//     arguments shall establish an implicit variable ordering" (b1 solved
//     before b2), and "Functions shall be called before constraints are
//     solved". These are solver/run-time behaviors; there is no
//     randomize() call here (see functions-in-constraint_1 for the run-time
//     result b2 == 5).
//   - "no side effects" / "cannot call rand_mode/constraint_mode": not
//     represented as a property in the object model; F has no such calls, so
//     the legal-file check (item 1) covers it.
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tf_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace {
hldb::AnyType typespecKind(const hldb::RefTypespec *ref) {
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

// Checks that 'expr' is a reference named like 'var' that binds to 'var'.
void expectRefTo(const hldb::Any *expr, const hldb::Variable *var) {
  ASSERT_NE(expr, nullptr);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
  ASSERT_NE(ref, nullptr) << "operand should be a reference to '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "reference '" << ref->getName() << "' should bind to the class variable";
}

// The caller-side expression of a call argument.
const hldb::Any *argumentExpr(const hldb::NamedArgument *arg) {
  if (arg->getHighConn() != nullptr) return arg->getHighConn();
  return arg->getLowConn();
}

// A function body of one statement may be modeled with or without a begin/end wrapper.
const hldb::Any *singleStatement(const hldb::Any *body) {
  if (const hldb::Begin *const begin = any_cast<hldb::Begin>(body)) {
    if ((begin->getStmts() == nullptr) || (begin->getStmts()->size() != 1)) return nullptr;
    return begin->getStmts()->at(0);
  }
  return body;
}
}  // namespace

namespace hlc {
class FunctionsInConstraint0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.12--functions-in-constraint_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClassA() const { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }
};

// Check 1: legal file.
TEST_F(FunctionsInConstraint0Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "an input-only automatic function call in a constraint is legal "
                                 "(IEEE 1800-2023 Sec 18.5.11)";
}

// Check 2: 'rand int b1, b2;' gives two rand int variables.
TEST_F(FunctionsInConstraint0Test, B1AndB2AreRandInt) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u) << "class 'a' declares exactly b1 and b2";
  for (std::string_view name : {"b1", "b2"}) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, cls->getVariables());
    ASSERT_NE(var, nullptr) << "variable '" << name << "' not found";
    EXPECT_EQ(var->getRandType(), vpiRand) << name << ": 'rand' applies to every name in the list";
    EXPECT_EQ(typespecKind(var->getTypespec()), hldb::AnyType::IntTypespec) << name << " is 'int'";
  }
}

// Check 3: method F is an automatic int function with one input int argument d.
TEST_F(FunctionsInConstraint0Test, FunctionFIsAutomaticWithOneInputArgument) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "class 'a' declares exactly one method, F";
  const hldb::Function *const f = hldb::findByName<hldb::Function>("F", cls->getMethods());
  ASSERT_NE(f, nullptr) << "function 'F' not found";
  EXPECT_TRUE(f->getMethod()) << "F is a class method";
  EXPECT_TRUE(f->getAutomatic()) << "class methods are automatic (IEEE 1800-2023 Sec 8.6), as required for "
                                    "functions used in constraints (Sec 18.5.11)";
  EXPECT_EQ(typespecKind(f->getReturn()), hldb::AnyType::IntTypespec) << "F returns int";

  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u) << "F has exactly one argument";
  const hldb::IODecl *const d = f->getIODecls()->at(0);
  EXPECT_EQ(d->getName(), "d");
  EXPECT_EQ(d->getDirection(), vpiInput) << "18.5.11: no output, inout or ref arguments";
  EXPECT_EQ(typespecKind(d->getTypespec()), hldb::AnyType::IntTypespec) << "argument d is 'int'";
}

// Check 3 (body): F's body is the single assignment "F = d".
TEST_F(FunctionsInConstraint0Test, FunctionFBodyAssignsDToF) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Function *const f = hldb::findByName<hldb::Function>("F", cls->getMethods());
  ASSERT_NE(f, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(singleStatement(f->getStmt()));
  ASSERT_NE(assign, nullptr) << "F's body is the single statement 'F=d;'";
  EXPECT_TRUE(assign->getBlocking()) << "'=' is a blocking assignment";
  ASSERT_NE(assign->getLhs(), nullptr);
  EXPECT_EQ(assign->getLhs()->getName(), "F") << "value is returned through the function name";
  ASSERT_NE(assign->getRhs(), nullptr);
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "d");
}

// Check 4: constraint order and properties.
TEST_F(FunctionsInConstraint0Test, ConstraintsC1AndC2InDeclarationOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 2u);
  EXPECT_EQ(cls->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(cls->getConstraints()->at(1)->getName(), "c2");
  for (const hldb::Constraint *const c : *cls->getConstraints()) {
    EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls)) << c->getName();
    EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not declared static, so vpiAutomatic is 1 (37.34 detail 1)";
    EXPECT_TRUE(c->getIsConstraintEnabled()) << c->getName() << ": initially active (18.9)";
    EXPECT_EQ(c->getAccessType(), 0) << c->getName() << ": in-body, not extern";
  }
}

// Check 5: c1 is "b1 == 5".
TEST_F(FunctionsInConstraint0Test, C1IsB1EqualsFive) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 1u);
  const hldb::Operation *const eq = any_cast<hldb::Operation>(c1->getConstraintItems()->at(0));
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  EXPECT_FALSE(eq->getSoft());
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  expectRefTo(eq->getOperands()->at(0), b1);
  const hldb::Constant *const five = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(five, nullptr);
  EXPECT_EQ(five->getDecompile(), "5");
}

// Check 6: c2 is "b2 == F(b1)" with the call bound to method F and argument b1.
TEST_F(FunctionsInConstraint0Test, C2IsB2EqualsCallOfFOnB1) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  const hldb::Function *const f = hldb::findByName<hldb::Function>("F", cls->getMethods());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b2, nullptr);
  ASSERT_NE(f, nullptr);

  const hldb::Constraint *const c2 = hldb::findByName<hldb::Constraint>("c2", cls->getConstraints());
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u);
  const hldb::Operation *const eq = any_cast<hldb::Operation>(c2->getConstraintItems()->at(0));
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  EXPECT_FALSE(eq->getSoft());
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  expectRefTo(eq->getOperands()->at(0), b2);

  const hldb::TFCall *const call = any_cast<hldb::TFCall>(eq->getOperands()->at(1));
  ASSERT_NE(call, nullptr) << "right operand of '==' is the function call F(b1)";
  EXPECT_EQ(call->getName(), "F");
  EXPECT_EQ(call->getTaskFunc(), static_cast<const hldb::TaskFunc *>(f)) << "call F(b1) binds to class method F";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "F is called with exactly one argument";
  expectRefTo(argumentExpr(call->getArguments()->at(0)), b1);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
