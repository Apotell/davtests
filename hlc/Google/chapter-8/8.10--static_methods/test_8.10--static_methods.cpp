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

// Tests for 8.10--static_methods.sv (tags: 8.10)
//   module class_tb ();
//     class test_cls;
//       static int id = 0;
//       static function int next_id();
//         ++id;
//         next_id = id;
//       endfunction
//     endclass
//
//     test_cls test_obj0;
//     test_cls test_obj1;
//
//     initial begin
//       test_obj0 = new;
//       test_obj1 = new;
//
//       $display(test_obj0.next_id());
//       $display(test_obj1.next_id());
//     end
//   endmodule
//
// IEEE 1800-2017 8.10 "Static methods": a method declared "static" can be
// called on the class itself, without any object handle, and cannot access
// non-static (per-object) members. This file exercises a static property
// ("id") mutated and read only through a static method ("next_id"), called
// through two DIFFERENT object handles (test_obj0, test_obj1); per 8.9/8.10
// both calls must observe and mutate the SAME single "id" storage, not a
// per-object copy.
//
// Checked:
//   - design has module class_tb with exactly 2 variables: "test_obj0" and
//     "test_obj1", both typed as test_cls
//   - the module has exactly 1 nested ClassDefn: "test_cls"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("id", signed IntTypespec) with initializer Constant "0",
//     and exactly 1 method ("next_id") -- see the KNOWN COMPILER BUG notes
//     below for the class's lifetime, the property's visibility, and the
//     method's "method" flag
//   - "next_id": a Function (not a Task), public visibility, whose return
//     type resolves to a plain IntTypespec -- this is the structural
//     signal that it is an ordinary function and NOT a constructor (see
//     chapter-8/8.7--constructor/test_8.7--constructor.cpp and siblings,
//     whose constructors' implicit return type is the enclosing class
//     itself, not IntTypespec)
//   - "next_id"'s body is a 2-statement Begin:
//     "++id;" -- an Operation (vpiOpType vpiPreIncOp) with 1 operand, a
//     RefObj "id" resolving to the property Variable "id"
//     "next_id = id;" -- a blocking Assignment whose lhs RefObj "next_id"
//     resolves via getActual<Function>() to the enclosing Function ITSELF
//     (the SystemVerilog idiom of assigning to the function's own name to
//     set its return value), and whose rhs RefObj "id" resolves to the
//     same property Variable "id"
//   - the initial process' Begin block has exactly 4 statements:
//     "test_obj0 = new", "test_obj1 = new",
//     "$display(test_obj0.next_id())", "$display(test_obj1.next_id())"
//   - both "test_obj0.next_id()" and "test_obj1.next_id()" are HierPaths
//     whose second path element is a MethodFuncCall "next_id" that
//     resolves getTaskFunc() to the SAME "next_id" Function -- unlike
//     KNOWN COMPILER BUG #6 below (ordinary "new()" never resolves
//     getTaskFunc()), these ordinary (non-constructor) method calls DO
//     resolve correctly, confirming that bug is specific to construction,
//     not method-call resolution in general
//   - design-level: exactly 1 class (test_cls)
//
// KNOWN COMPILER BUG #1 (class lifetime defaulting), KNOWN COMPILER BUG #2
// (property visibility defaulting), and KNOWN COMPILER BUG #4 (a method
// declared directly in a class body is not flagged via getMethod()):
// already confirmed independently across other chapter-8 files in this
// suite (see hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp,
// hlc/Google/chapter-8/8.5--properties/test_8.5--properties.cpp, and
// hlc/Google/chapter-8/8.6--methods/test_8.6--methods.cpp). ClassIsAutomaticByDefault,
// PropertyIdIsPublicByDefault, and NextIdFunctionIsRecognizedAsClassMethod
// below assert the IEEE-mandated behavior and will FAIL until these are
// fixed.
//
// PROPERTY ALLOCATION SCHEME (static property "id"): per the RESOLVED
// QUESTION established in hlc/Google/chapter-8/8.9--static_properties/test_8.9--static_properties.cpp
// (cross-checked against the non-static property in 8.4--instantiation),
// Variable::getAutomatic() is the accessor for per-object ("automatic")
// vs class-wide ("static") property allocation. PropertyIdAllocationIsStatic
// below asserts the same "false" result for this file's static property,
// consistent with that resolution.
//
// METHOD "STATIC"-NESS: NOT ASSERTED. Unlike properties, no accessor on
// TaskFunc/Function in this HLDB model could be confidently grounded as
// reflecting whether a class METHOD is "static"-qualified (i.e., callable
// without an object handle, per 8.10). TaskFunc::getAutomatic() is NOT
// used for this: per IEEE 1800-2017 13.4.2, "automatic" on a task/function
// governs whether its OWN LOCAL variables get fresh per-call storage
// (reentrancy) -- an entirely different axis from whether the method
// itself needs an implicit "this" (8.10). Conflating the two would repeat
// the same mistake already walked back for the IODecl-as-named-argument
// claim in chapter-8/8.8--typed_constructor_param/test_8.8--typed_constructor_param.cpp.
// TaskFunc::getAccessType() is also a candidate by name, but its value
// space is undocumented anywhere in this codebase and no other test in
// this suite has grounded it against a known-good case, so it is left
// untested here rather than guessed at.

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
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassStaticMethodsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.10--static_methods.hlc"}); }
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

  static const hldb::Variable *getPropertyId() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Function *getNextIdFunction() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::Variable *getVariableTestObj0() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("test_obj0", top->getVariables());
  }

  static const hldb::Variable *getVariableTestObj1() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("test_obj1", top->getVariables());
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  // Verifies stmt[index] is "<varName> = new;": a blocking Assignment
  // whose lhs RefObj resolves to the given Variable and whose rhs is a
  // no-argument "new" MethodFuncCall.
  static void ExpectNewAssignment(size_t index, std::string_view varName, const hldb::Variable *var) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(index));
    ASSERT_NE(assign, nullptr) << "stmt[" << index << "] should be an Assignment (" << varName << " = new)";
    EXPECT_TRUE(assign->getBlocking());
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), varName);
    EXPECT_EQ(lhs->getActual<hldb::Variable>(), var);
    const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
    ASSERT_NE(newCall, nullptr) << "'new' should resolve to a MethodFuncCall";
    EXPECT_EQ(newCall->getName(), "new");
    EXPECT_EQ(newCall->getArguments(), nullptr);
  }

  // Verifies stmt[index] is "$display(<varName>.next_id())": a SysFuncCall
  // whose sole argument is a HierPath resolving "next_id" to a
  // MethodFuncCall whose getTaskFunc() resolves to the class's "next_id"
  // Function.
  static void ExpectNextIdDisplay(size_t index, std::string_view varName, const hldb::Variable *var) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(index));
    ASSERT_NE(disp, nullptr) << "stmt[" << index << "] should be a $display SysTaskCall";
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 1u);

    const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
    ASSERT_NE(path, nullptr) << "'" << varName << ".next_id()' should be a HierPath";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    const hldb::RefObj *const varRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(varRef, nullptr);
    EXPECT_EQ(varRef->getName(), varName);
    EXPECT_EQ(varRef->getActual<hldb::Variable>(), var);

    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
    ASSERT_NE(call, nullptr) << "'" << varName << ".next_id()' second path elem should be a MethodFuncCall";
    EXPECT_EQ(call->getName(), "next_id");
    EXPECT_EQ(call->getTaskFunc(), getNextIdFunction())
        << "'" << varName << ".next_id()' should resolve getTaskFunc() to the class's 'next_id' Function "
        << "(contrast with KNOWN COMPILER BUG #6, where ordinary 'new()' never resolves getTaskFunc())";
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassStaticMethodsTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassStaticMethodsTest, ModuleHasTwoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(ClassStaticMethodsTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassStaticMethodsTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassStaticMethodsTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassStaticMethodsTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to "
                                    "automatic (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassStaticMethodsTest, ClassHasOnePropertyId) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const id = getPropertyId();
  ASSERT_NE(id, nullptr);
  EXPECT_EQ(id->getName(), "id");
  ASSERT_NE(id->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = id->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "property 'id' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(ClassStaticMethodsTest, PropertyIdHasInitializerZero) {
  const hldb::Variable *const id = getPropertyId();
  ASSERT_NE(id, nullptr);
  const hldb::Constant *const init = id->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'static int id = 0;' should attach '0' as the property's own initializer value";
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(ClassStaticMethodsTest, PropertyIdIsPublicByDefault) {
  const hldb::Variable *const id = getPropertyId();
  ASSERT_NE(id, nullptr);
  EXPECT_EQ(id->getVisibility(), vpiPublicVis) << "8.14: 'static int id' with no visibility qualifier defaults "
                                                  "to public (see KNOWN COMPILER BUG #2 above)";
}

// See the PROPERTY ALLOCATION SCHEME note at the top of this file: this
// follows the resolution already reached jointly by
// chapter-8/8.4--instantiation/test_8.4--instantiation.cpp and
// chapter-8/8.9--static_properties/test_8.9--static_properties.cpp.
TEST_F(ClassStaticMethodsTest, PropertyIdAllocationIsStatic) {
  const hldb::Variable *const id = getPropertyId();
  ASSERT_NE(id, nullptr);
  EXPECT_FALSE(id->getAutomatic()) << "8.9: 'static int id' should NOT have per-object (automatic) allocation";
}

TEST_F(ClassStaticMethodsTest, ClassHasOneMethodNextId) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const fn = getNextIdFunction();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "next_id");
}

TEST_F(ClassStaticMethodsTest, NextIdFunctionIsRecognizedAsClassMethod) {
  const hldb::Function *const fn = getNextIdFunction();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getMethod()) << "8.10: 'next_id' is declared directly inside the class body and should be "
                                  "flagged as a class method (see KNOWN COMPILER BUG #4 above)";
}

TEST_F(ClassStaticMethodsTest, NextIdFunctionIsPublicByDefault) {
  const hldb::Function *const fn = getNextIdFunction();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "8.14: 'static function int next_id()' with no visibility "
                                                  "qualifier defaults to public";
}

TEST_F(ClassStaticMethodsTest, NextIdFunctionReturnTypeIsPlainInt) {
  const hldb::Function *const fn = getNextIdFunction();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getReturn(), nullptr);
  const hldb::IntTypespec *const ret = fn->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ret, nullptr) << "'next_id's return type should resolve to a plain IntTypespec, confirming this is "
                             "an ordinary function and not a constructor";
  EXPECT_TRUE(ret->getSigned());
}

// --- "next_id" body: "++id; next_id = id;" --------------------------------------

TEST_F(ClassStaticMethodsTest, NextIdBodyHasTwoStmts) {
  const hldb::Function *const fn = getNextIdFunction();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassStaticMethodsTest, FirstStmtIsPreIncrementOfId) {
  const hldb::Function *const fn = getNextIdFunction();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::Operation *const op = any_cast<hldb::Operation>(body->getStmts()->at(0));
  ASSERT_NE(op, nullptr) << "stmt[0] should be an Operation ('++id;')";
  EXPECT_EQ(op->getOpType(), vpiPreIncOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::RefObj *const idOperand = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(idOperand, nullptr);
  EXPECT_EQ(idOperand->getName(), "id");
  EXPECT_EQ(idOperand->getActual<hldb::Variable>(), getPropertyId());
}

TEST_F(ClassStaticMethodsTest, SecondStmtAssignsIdToNextIdReturnValue) {
  const hldb::Function *const fn = getNextIdFunction();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('next_id = id;')";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'next_id' (return-value write target) should be a RefObj";
  EXPECT_EQ(lhs->getName(), "next_id");
  EXPECT_EQ(lhs->getActual<hldb::Function>(), fn)
      << "assigning to a function's own name should resolve back to the enclosing Function itself";

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "id");
  EXPECT_EQ(rhs->getActual<hldb::Variable>(), getPropertyId());
}

// --- variables "test_obj0" / "test_obj1" ---------------------------------------------

TEST_F(ClassStaticMethodsTest, VariableTestObj0Exists) { EXPECT_NE(getVariableTestObj0(), nullptr); }

TEST_F(ClassStaticMethodsTest, VariableTestObj1Exists) { EXPECT_NE(getVariableTestObj1(), nullptr); }

TEST_F(ClassStaticMethodsTest, VariableTestObj0TypespecResolvesToTestClsClassDefn) {
  const hldb::Variable *const testObj0 = getVariableTestObj0();
  ASSERT_NE(testObj0, nullptr);
  ASSERT_NE(testObj0->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj0->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassStaticMethodsTest, VariableTestObj1TypespecResolvesToTestClsClassDefn) {
  const hldb::Variable *const testObj1 = getVariableTestObj1();
  ASSERT_NE(testObj1, nullptr);
  ASSERT_NE(testObj1->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj1->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassStaticMethodsTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassStaticMethodsTest, InitialBeginHasFourStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u);
}

// --- test_obj0 = new; test_obj1 = new; (stmt[0], stmt[1]) ------------------------

TEST_F(ClassStaticMethodsTest, FirstStmtIsTestObj0New) { ExpectNewAssignment(0, "test_obj0", getVariableTestObj0()); }

TEST_F(ClassStaticMethodsTest, SecondStmtIsTestObj1New) { ExpectNewAssignment(1, "test_obj1", getVariableTestObj1()); }

// --- $display(test_obj0.next_id()); $display(test_obj1.next_id()); (stmt[2], stmt[3]) --

TEST_F(ClassStaticMethodsTest, ThirdStmtDisplaysTestObj0NextId) {
  ExpectNextIdDisplay(2, "test_obj0", getVariableTestObj0());
}

// The crux of this file: calling the static method through a DIFFERENT
// handle (test_obj1) than the one used just before (test_obj0) must still
// resolve "next_id" to the SAME declared Function.
TEST_F(ClassStaticMethodsTest, FourthStmtDisplaysTestObj1NextId) {
  ExpectNextIdDisplay(3, "test_obj1", getVariableTestObj1());
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassStaticMethodsTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
