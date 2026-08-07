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

// Tests for 8.11--this.sv (tags: 8.11)
//   module class_tb ();
//     class test_cls;
//       int a;
//       task test_method(int a);
//         $display("test_method");
//         this.a += a;
//       endtask
//     endclass
//   endmodule
//
// IEEE 1800-2017 8.11 "this": "this" is used within a class method to
// unambiguously refer to the current object's own members, most commonly
// when a method argument or local variable shadows a property of the same
// name -- exactly the case here, where the task argument "a" shadows the
// class property "a". "this.a" must refer to the PROPERTY, while the bare
// "a" inside the same expression must refer to the shadowing ARGUMENT.
//
// Checked:
//   - design has module class_tb with exactly 1 nested ClassDefn:
//     "test_cls" (this file declares no object handles/nets at all)
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("a", signed IntTypespec, no initializer) and exactly 1
//     method ("test_method", a Task) -- see the FIXED COMPILER BUGS note
//     below for the class's lifetime, the property's visibility, and the
//     method's "method" flag
//   - "test_method": a Task (not a Function, since it is declared with
//     "task"), public visibility, with exactly 1 IODecl ("a", direction
//     input, signed IntTypespec)
//   - "test_method"'s body is a 2-statement Begin:
//     `$display("test_method");` -- here (inside a TASK body) this is a
//     SysTaskCall, NOT a SysFuncCall as seen for the same-looking call
//     inside an "initial" procedural block in other chapter-8 files (see
//     chapter-8/8.12--assignment/test_8.12--assignment.cpp)
//     `this.a += a;` -- a blocking Assignment (the "+=" operator-assignment
//     is desugared to a plain Assignment whose rhs is an explicit "add"
//     Operation), where:
//       - the LHS is a HierPath "this.a" whose first path element is a
//         RefObj "this" resolving (via getActual<ClassDefn>()) to the
//         enclosing ClassDefn itself, and whose second path element is a
//         RefObj "a" resolving to the class's property Variable "a"
//       - the RHS Operation's first operand is the SAME shape of HierPath
//         "this.a" (property Variable "a" again)
//       - the RHS Operation's second operand is a bare RefObj "a"
//         resolving instead to the IODecl "a" (the task's own argument) --
//         a DIFFERENT declaration than the one "this.a" resolves to, which
//         is the entire point of "this": disambiguating the shadowed name
//   - design-level: exactly 1 class (test_cls)
//
// "THIS" RESOLUTION SHAPE (not a bug): "this" is represented as a plain
// RefObj (the same node type used for any other identifier reference)
// whose getActual<ClassDefn>() resolves to the ENCLOSING CLASS DEFINITION,
// not to some dedicated "current object" node type. This is a reasonable,
// correct static representation: HLC is a parser/elaborator, not a
// simulator, so it never allocates or tracks actual object instances at
// compile time -- "this" statically can only ever mean "the current
// class's own scope," which is precisely what resolving to the ClassDefn
// captures. The property being tested here is NOT what "this" resolves to
// internally, but whether "this.a" and the shadowing bare "a" correctly
// resolve to two DIFFERENT declarations (Variable vs IODecl) -- see
// SecondStmtRhsSecondOperandIsShadowingArgumentA below, which confirms
// this is the case.
//
// FIXED COMPILER BUGS (previously tracked as KNOWN COMPILER BUG #1 class-lifetime
// defaulting, #2 property-visibility defaulting, and #4 a method declared
// directly in a class body not being flagged via getMethod()): all three are
// confirmed fixed. Phase2ModelBuilder::enterPA_Class_declaration defaults a
// class with no explicit lifetime keyword to automatic; leavePA_Class_property
// defaults an unqualified property to vpiPublicVis (8.18: "unqualified class
// properties and methods are public"); and leavePA_Class_method unconditionally
// calls setMethod(true) for any Task/Function/constructor reached via the
// Class_method AST production. ClassIsAutomaticByDefault, PropertyAIsPublicByDefault,
// and TestMethodIsRecognizedAsClassMethod below assert this and now pass.
//
// Note on ClassIsAutomaticByDefault's citation: IEEE 1800-2023's class_declaration
// grammar (8.3/Annex A.1.2) no longer has a [lifetime] slot at all -- it was
// replaced by [final_specifier] for the new "final class" feature; only
// 1800-2017 allowed an explicit "class automatic"/"class static" keyword. HLC's
// own grammar (SV3_1aParser.g4's class_declaration rule) still accepts the
// legacy lifetime keyword for backward compatibility. Either way, "class
// test_cls;" here uses neither keyword, and getAutomatic() defaulting true
// reflects 8.4's invariant that class objects are always created/destroyed
// dynamically, independent of which revision's grammar is in play.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassThisTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.11--this.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("class_tb", m_design->getAllModules());
  }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("test_cls", top->getClassDefns());
  }

  static const hldb::Variable *getPropertyA() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Task *getTestMethodTask() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Task>(c->getMethods()->at(0));
  }

  static const hldb::Begin *getMethodBody() {
    const hldb::Task *const t = getTestMethodTask();
    if (t == nullptr) return nullptr;
    return t->getStmt<hldb::Begin>();
  }

  // Verifies the given node is a HierPath "this.a" whose first path elem
  // is a RefObj "this" resolving to the class's own ClassDefn, and whose
  // second path elem is a RefObj "a" resolving to the class property
  // Variable "a".
  static void ExpectThisDotAPath(const hldb::HierPath *path) {
    ASSERT_NE(path, nullptr) << "'this.a' should be a HierPath";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);

    const hldb::RefObj *const thisRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(thisRef, nullptr);
    EXPECT_EQ(thisRef->getName(), "this");
    EXPECT_EQ(thisRef->getActual<hldb::ClassDefn>(), getTestClsDefn())
        << "'this' should resolve to the enclosing ClassDefn (see THIS RESOLUTION SHAPE note above)";

    const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(aRef, nullptr);
    EXPECT_EQ(aRef->getName(), "a");
    EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA())
        << "'this.a' should resolve to the class's property Variable 'a', not the shadowing argument";
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassThisTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassThisTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassThisTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassThisTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassThisTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "'class test_cls' has no lifetime qualifier; per IEEE 1800-2023 8.4, class "
                                     "objects are always created/destroyed dynamically, so this defaults to "
                                     "automatic (FIXED COMPILER BUG, see header comment above)";
}

TEST_F(ClassThisTest, ClassIsNotVirtualByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_FALSE(c->getVirtual()) << "'class test_cls' has no 'virtual' qualifier, so it should not be flagged "
                                    "as a virtual class";
}

TEST_F(ClassThisTest, ClassHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "property 'a' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(ClassThisTest, PropertyAHasNoInitializer) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr) << "'int a;' declares no initializer";
}

TEST_F(ClassThisTest, PropertyAIsNotConstant) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->getConstantVariable()) << "'int a;' has no 'const' qualifier";
}

TEST_F(ClassThisTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.18: 'int a' with no visibility qualifier defaults to public "
                                                 "(FIXED COMPILER BUG, see header comment above)";
}

TEST_F(ClassThisTest, ClassHasOneMethodTestMethod) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getName(), "test_method");
}

TEST_F(ClassThisTest, TestMethodIsRecognizedAsClassMethod) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_TRUE(t->getMethod()) << "8.6: 'test_method' is declared directly inside the class body and should be "
                                 "flagged as a class method (FIXED COMPILER BUG, see header comment above)";
}

TEST_F(ClassThisTest, TestMethodIsPublicByDefault) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getVisibility(), vpiPublicVis) << "8.18: 'task test_method(...)' with no visibility qualifier "
                                                 "defaults to public";
}

TEST_F(ClassThisTest, TestMethodIsNotVirtualByDefault) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_FALSE(t->getVirtual()) << "'task test_method(...)' has no 'virtual' qualifier";
}

TEST_F(ClassThisTest, TestMethodHasOneIODeclA) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getIODecls(), nullptr);
  ASSERT_EQ(t->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = t->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), "a");
  EXPECT_EQ(io->getDirection(), vpiInput);
  ASSERT_NE(io->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = io->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "argument 'a' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned()) << "'int a' argument should resolve to a signed IntTypespec";
}

// --- "test_method" body: '$display("test_method"); this.a += a;' ---------------

TEST_F(ClassThisTest, TestMethodBodyHasTwoStmts) {
  const hldb::Begin *const body = getMethodBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassThisTest, FirstStmtDisplaysTestMethod) {
  const hldb::Begin *const body = getMethodBody();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  ASSERT_NE(disp, nullptr) << "stmt[0] should be a $display SysTaskCall (task-body context)";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "\"test_method\"");
}

TEST_F(ClassThisTest, SecondStmtIsThisADotPlusEqualsA) {
  const hldb::Begin *const body = getMethodBody();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('this.a += a;')";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::HierPath *const lhs = assign->getLhs<hldb::HierPath>();
  ExpectThisDotAPath(lhs);

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'+=' should desugar to an explicit 'add' Operation on the rhs";
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::HierPath *const firstOperand = any_cast<hldb::HierPath>(rhs->getOperands()->at(0));
  ExpectThisDotAPath(firstOperand);
}

// The crux of this file: the bare "a" on the rhs of "this.a += a;" must
// resolve to the TASK ARGUMENT (an IODecl), a DIFFERENT declaration than
// the class property Variable "a" that "this.a" resolves to -- confirming
// "this" correctly disambiguates the shadowed name per 8.11.
TEST_F(ClassThisTest, SecondStmtRhsSecondOperandIsShadowingArgumentA) {
  const hldb::Begin *const body = getMethodBody();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::RefObj *const secondOperand = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
  ASSERT_NE(secondOperand, nullptr);
  EXPECT_EQ(secondOperand->getName(), "a");
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getIODecls(), nullptr);
  ASSERT_GT(t->getIODecls()->size(), 0u);
  EXPECT_EQ(secondOperand->getActual<hldb::IODecl>(), t->getIODecls()->at(0))
      << "bare 'a' should resolve to the task's own argument (IODecl), NOT the class property Variable";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassThisTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
