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

// Tests for 8.14--override_member.sv (tags: 8.14)
//   module class_tb ();
//     class super_cls;
//       int s = 2;
//       function int incs();
//         ++s;
//         incs = s;
//       endfunction
//       function new(int def = 3);
//         s = def;
//       endfunction
//     endclass
//
//     class test_cls extends super_cls;
//       int a;
//       function int incs();
//         s += 2;
//         incs = s;
//       endfunction
//       function new(int def = 42);
//         super.new(def + 3);
//         a = def;
//       endfunction
//     endclass
//
//     test_cls test_obj;
//     super_cls super_obj;
//
//     initial begin
//       test_obj = new(37);
//       super_obj = test_obj;
//
//       $display(test_obj.s);
//       $display(test_obj.incs());
//       $display(test_obj.s);
//       $display(super_obj.incs());
//     end
//   endmodule
//
// IEEE 1800-2017 8.14 "Member overriding, [...] and static binding":
// test_cls declares its OWN "incs" method, with a different body than
// super_cls's "incs" -- this OVERRIDES (shadows) the base method for any
// handle statically typed as test_cls. Neither "incs" is declared
// "virtual", so per 8.14 dispatch is STATIC (compile-time), based on the
// handle's DECLARED type, not any runtime type: calling "incs()" through
// a test_cls-typed handle must reach test_cls's OWN "incs", while calling
// "incs()" through a super_cls-typed handle must reach super_cls's OWN
// "incs" -- even when (as here, via "super_obj = test_obj;") the
// super_cls-typed handle's underlying object was actually constructed as
// a test_cls. This file's crux is confirming that this static, per-handle
// resolution is exactly what the compiler produces -- not simply that
// name lookup found "some incs," but that it found the specific Function
// belonging to the handle's OWN declared class.
//
// HLC is a static parser/elaborator, not a simulator: it cannot observe
// runtime values (e.g. what "$display" would actually print). What it CAN
// and does model statically is exactly the call-resolution distinction
// above, which is what this file actually tests.
//
// Checked:
//   - design has module class_tb with exactly 2 variables: "test_obj"
//     (typed test_cls) and "super_obj" (typed super_cls)
//   - the module has exactly 2 nested ClassDefns: "super_cls" and
//     "test_cls"
//   - ClassDefn "super_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("s", signed IntTypespec) with initializer Constant "2",
//     and exactly 2 methods ("incs", "new"); the property defaults to
//     public visibility and both methods are correctly flagged as class
//     methods (see the FIXED COMPILER BUG notes below)
//   - super_cls's "incs": a Function (return type resolves to plain
//     IntTypespec, contrast with "new"), 2-statement body "++s;
//     incs = s;" (same shape as chapter-8/8.13--inheritance)
//   - super_cls's "new": return type resolves to a ClassTypespec matching
//     super_cls itself; 1 IODecl ("def", input, IntTypespec, default
//     Constant "3"); single bare Assignment body (not wrapped in Begin)
//     "s = def;"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, EXTENDS
//     super_cls (getExtends() resolves to super_cls's own ClassDefn, same
//     as chapter-8/8.13--inheritance). Has exactly 1 property ("a",
//     signed IntTypespec, no initializer) and exactly 2 methods ("incs",
//     "new")
//   - test_cls's "incs" (THE OVERRIDE): a Function distinct from
//     super_cls's "incs" -- a DIFFERENT declared Function object, not the
//     same one reused. Its 2-statement body is "s += 2; incs = s;":
//     "s += 2;" is a blocking Assignment desugared to an explicit "add"
//     Operation, whose lhs and first rhs operand RefObj "s" resolve to
//     super_cls's OWN property Variable "s" (test_cls declares no "s" of
//     its own -- only the METHOD is overridden, not the property);
//     "incs = s;" resolves its lhs, via getActual<Function>(), to
//     test_cls's OWN "incs" Function (not super_cls's)
//   - test_cls's "new": same shape as chapter-8/8.13--inheritance's
//     derived constructor -- "super.new(def + 3);" (a bare HierPath whose
//     MethodFuncCall resolves getTaskFunc() to super_cls's own "new")
//     followed by "a = def;"
//   - the initial process' Begin block has exactly 6 statements:
//     "test_obj = new(37)", "super_obj = test_obj", "$display(test_obj.s)",
//     "$display(test_obj.incs())", "$display(test_obj.s)",
//     "$display(super_obj.incs())"
//   - "test_obj = new(37)": this ordinary "new(...)" call resolves
//     getTaskFunc() to the real constructor (see FIXED COMPILER BUG #6
//     below)
//   - "super_obj = test_obj": THE STRUCTURAL NOTE of this file -- assigning
//     a test_cls-typed handle to a super_cls-typed variable (an implicit
//     upcast) is modeled as a PLAIN Assignment whose rhs is an ordinary
//     RefObj resolving to test_obj's Variable -- no explicit cast/conversion
//     node is inserted despite the two Variables having different
//     ClassTypespecs. This mirrors the same-type handle-copy shape
//     already confirmed in
//     chapter-8/8.12--assignment/test_8.12--assignment.cpp; it is not
//     asserted as a bug, since SystemVerilog upcast handle assignment
//     needs no runtime conversion.
//   - "$display(test_obj.s)" (both occurrences): HierPath resolving "s" to
//     super_cls's own property Variable, same object both times
//   - "$display(test_obj.incs())": THE CRUX of member overriding -- the
//     MethodFuncCall's getTaskFunc() must resolve to test_cls's OWN
//     "incs" (the override), NOT super_cls's, because "test_obj" is
//     statically typed test_cls
//   - "$display(super_obj.incs())": THE OTHER CRUX -- the MethodFuncCall's
//     getTaskFunc() must resolve to super_cls's OWN "incs" (the base
//     version), NOT test_cls's, because "super_obj" is statically typed
//     super_cls and neither "incs" is virtual (static/compile-time
//     dispatch, per 8.14) -- even though the object referenced was
//     actually constructed as a test_cls via the upcast assignment above
//   - design-level: exactly 2 classes (super_cls, test_cls)
//
// FIXED COMPILER BUG #2 (property visibility defaulting) and FIXED
// COMPILER BUG #4 (a method declared directly in a class body is not
// flagged via getMethod()): cross-checked at the time across other
// chapter-8 files in this suite (see
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp and
// siblings). PropertySIsPublicByDefault, PropertyAIsPublicByDefault,
// SuperIncsIsRecognizedAsClassMethod, and TestIncsIsRecognizedAsClassMethod
// below assert the IEEE-mandated behavior and now pass.
//
// FIXED COMPILER BUG #6 (constructor call resolution): cross-checked at
// the time across chapter-8/8.7--constructor/test_8.7--constructor.cpp and
// chapter-8/8.13--inheritance/test_8.13--inheritance.cpp.
// FirstStmtIsTestObjNewWithThirtySeven below asserts the IEEE-mandated
// resolution and now passes.
//
// FIXED COMPILER BUG #1 (class lifetime defaulting): cross-checked at the
// time per the ctest runs described in
// chapter-8/8.12--assignment/test_8.12--assignment.cpp and
// chapter-8/8.11--this/test_8.11--this.cpp. SuperClsIsAutomaticByDefault
// and TestClsIsAutomaticByDefault below now pass accordingly.

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
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassOverrideMemberTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.14--override_member.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("class_tb", m_design->getAllModules());
  }

  static const hldb::ClassDefn *getSuperClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("super_cls", top->getClassDefns());
  }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("test_cls", top->getClassDefns());
  }

  static const hldb::Variable *getPropertyS() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Variable *getPropertyA() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Function *getSuperIncsFunction() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::Function *getSuperNewFunction() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->size() < 2) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(1));
  }

  static const hldb::Function *getTestIncsFunction() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::Function *getTestNewFunction() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->size() < 2) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(1));
  }

  static const hldb::Variable *getVariableTestObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("test_obj", top->getVariables());
  }

  static const hldb::Variable *getVariableSuperObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("super_obj", top->getVariables());
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  // Verifies stmt[index] is "$display(<varName>.s)".
  static void ExpectSDisplay(size_t index, std::string_view varName, const hldb::Variable *var) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(index));
    ASSERT_NE(disp, nullptr) << "stmt[" << index << "] should be a $display SysTaskCall";
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 1u);

    const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
    ASSERT_NE(path, nullptr) << "'" << varName << ".s' should be a HierPath";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);
    const hldb::RefObj *const varRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(varRef, nullptr);
    EXPECT_EQ(varRef->getName(), varName);
    EXPECT_EQ(varRef->getActual<hldb::Variable>(), var);
    const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(sRef, nullptr);
    EXPECT_EQ(sRef->getName(), "s");
    EXPECT_EQ(sRef->getActual<hldb::Variable>(), getPropertyS())
        << "'" << varName << ".s' should resolve to super_cls's OWN declared property Variable";
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassOverrideMemberTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassOverrideMemberTest, ModuleHasTwoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(ClassOverrideMemberTest, ModuleHasTwoClassDefns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 2u);
}

// --- class "super_cls" ---------------------------------------------------------

TEST_F(ClassOverrideMemberTest, SuperClsExists) { EXPECT_NE(getSuperClsDefn(), nullptr); }

TEST_F(ClassOverrideMemberTest, SuperClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassOverrideMemberTest, SuperClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class super_cls' has no lifetime qualifier so it defaults to automatic";
}

TEST_F(ClassOverrideMemberTest, SuperClsHasOnePropertyS) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
}

TEST_F(ClassOverrideMemberTest, PropertySHasInitializerTwo) {
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  const hldb::Constant *const init = s->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "2");
}

TEST_F(ClassOverrideMemberTest, PropertySIsPublicByDefault) {
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getVisibility(), vpiPublicVis) << "8.14: 'int s' with no visibility qualifier defaults to public "
                                                 "(see FIXED COMPILER BUG #2 above)";
}

TEST_F(ClassOverrideMemberTest, SuperClsHasTwoMethods) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 2u);
  const hldb::Function *const incs = getSuperIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_EQ(incs->getName(), "incs");
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassOverrideMemberTest, SuperIncsIsRecognizedAsClassMethod) {
  const hldb::Function *const incs = getSuperIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_TRUE(incs->getMethod()) << "see FIXED COMPILER BUG #4 above";
}

TEST_F(ClassOverrideMemberTest, SuperIncsReturnTypeIsPlainInt) {
  const hldb::Function *const incs = getSuperIncsFunction();
  ASSERT_NE(incs, nullptr);
  ASSERT_NE(incs->getReturn(), nullptr);
  const hldb::IntTypespec *const ret = incs->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ret, nullptr) << "'incs's return type should resolve to a plain IntTypespec (not a constructor)";
}

TEST_F(ClassOverrideMemberTest, SuperIncsBodyHasTwoStmts) {
  const hldb::Function *const incs = getSuperIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassOverrideMemberTest, SuperNewIsRecognizedAsConstructor) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ret = ctor->getReturn()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ret, nullptr) << "'new's return type should resolve to a ClassTypespec";
  EXPECT_EQ(ret->getClassDefn(), getSuperClsDefn());
}

TEST_F(ClassOverrideMemberTest, SuperNewHasOneIODeclDefWithDefaultThree) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = ctor->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), "def");
  const hldb::Constant *const def = io->getExpr<hldb::Constant>();
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getDecompile(), "3");
}

TEST_F(ClassOverrideMemberTest, SuperNewBodyAssignsSToDef) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Assignment *const assign = ctor->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'s = def;' is super_cls's only statement, so it is NOT wrapped in a Begin";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyS());
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassOverrideMemberTest, TestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassOverrideMemberTest, TestClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassOverrideMemberTest, TestClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic";
}

TEST_F(ClassOverrideMemberTest, TestClsExtendsSuperCls) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  const hldb::Extends *const ext = c->getExtends();
  ASSERT_NE(ext, nullptr) << "'test_cls extends super_cls' should attach an Extends object";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const ref = ext->getClassTypespecs()->at(0);
  ASSERT_NE(ref, nullptr);
  const hldb::ClassTypespec *const ct = ref->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getSuperClsDefn());
}

TEST_F(ClassOverrideMemberTest, TestClsHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
}

TEST_F(ClassOverrideMemberTest, PropertyAHasNoInitializer) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr);
}

TEST_F(ClassOverrideMemberTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "see FIXED COMPILER BUG #2 above";
}

TEST_F(ClassOverrideMemberTest, TestClsHasTwoMethods) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 2u);
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_EQ(incs->getName(), "incs");
  const hldb::Function *const ctor = getTestNewFunction();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

// THE structural crux of "override": test_cls's "incs" must be a
// DIFFERENT declared Function object than super_cls's "incs" -- a true
// shadowing declaration, not the same Function reused/aliased.
TEST_F(ClassOverrideMemberTest, TestIncsIsDistinctFromSuperIncs) {
  const hldb::Function *const testIncs = getTestIncsFunction();
  const hldb::Function *const superIncs = getSuperIncsFunction();
  ASSERT_NE(testIncs, nullptr);
  ASSERT_NE(superIncs, nullptr);
  EXPECT_NE(testIncs, superIncs);
}

TEST_F(ClassOverrideMemberTest, TestIncsIsRecognizedAsClassMethod) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_TRUE(incs->getMethod()) << "see FIXED COMPILER BUG #4 above";
}

TEST_F(ClassOverrideMemberTest, TestIncsReturnTypeIsPlainInt) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  ASSERT_NE(incs->getReturn(), nullptr);
  const hldb::IntTypespec *const ret = incs->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ret, nullptr);
}

TEST_F(ClassOverrideMemberTest, TestIncsBodyHasTwoStmts) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

// test_cls's override body reads/writes "s += 2;" -- "s" resolves to
// super_cls's OWN property Variable, since test_cls does not redeclare
// "s" (only the METHOD is overridden here, not the property).
TEST_F(ClassOverrideMemberTest, TestIncsFirstStmtIsSPlusEqualsTwo) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment ('s += 2;')";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "s");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyS())
      << "'s' inside the override should still resolve to super_cls's inherited property Variable";

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'+=' should desugar to an explicit 'add' Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const sOperand = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(sOperand, nullptr);
  EXPECT_EQ(sOperand->getActual<hldb::Variable>(), getPropertyS());
  const hldb::Constant *const twoOperand = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  ASSERT_NE(twoOperand, nullptr);
  EXPECT_EQ(twoOperand->getDecompile(), "2");
}

TEST_F(ClassOverrideMemberTest, TestIncsSecondStmtAssignsSToIncsReturnValue) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('incs = s;')";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "incs");
  EXPECT_EQ(lhs->getActual<hldb::Function>(), incs)
      << "assigning to 'incs' inside the OVERRIDE should resolve to test_cls's OWN incs Function, not super_cls's";

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getActual<hldb::Variable>(), getPropertyS());
}

TEST_F(ClassOverrideMemberTest, TestNewIsRecognizedAsConstructor) {
  const hldb::Function *const ctor = getTestNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ret = ctor->getReturn()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ret, nullptr);
  EXPECT_EQ(ret->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassOverrideMemberTest, TestNewHasOneIODeclDefWithDefaultFortyTwo) {
  const hldb::Function *const ctor = getTestNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = ctor->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  const hldb::Constant *const def = io->getExpr<hldb::Constant>();
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getDecompile(), "42");
}

TEST_F(ClassOverrideMemberTest, TestNewFirstStmtIsSuperNewCall) {
  const hldb::Function *const ctor = getTestNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(body->getStmts()->at(0));
  ASSERT_NE(path, nullptr) << "stmt[0] should be a bare HierPath ('super.new(def + 3)')";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const superRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(superRef, nullptr);
  EXPECT_EQ(superRef->getActual<hldb::ClassDefn>(), getSuperClsDefn());
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getTaskFunc(), getSuperNewFunction());
}

TEST_F(ClassOverrideMemberTest, TestNewSecondStmtAssignsAToDef) {
  const hldb::Function *const ctor = getTestNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('a = def;')";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyA());
}

// --- variables "test_obj" / "super_obj" ---------------------------------------------

TEST_F(ClassOverrideMemberTest, VariableTestObjExists) { EXPECT_NE(getVariableTestObj(), nullptr); }

TEST_F(ClassOverrideMemberTest, VariableTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Variable *const testObj = getVariableTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassOverrideMemberTest, VariableSuperObjExists) { EXPECT_NE(getVariableSuperObj(), nullptr); }

TEST_F(ClassOverrideMemberTest, VariableSuperObjTypespecResolvesToSuperClsClassDefn) {
  const hldb::Variable *const superObj = getVariableSuperObj();
  ASSERT_NE(superObj, nullptr);
  ASSERT_NE(superObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = superObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getSuperClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassOverrideMemberTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassOverrideMemberTest, InitialBeginHasSixStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 6u);
}

// See FIXED COMPILER BUG #6 above.
TEST_F(ClassOverrideMemberTest, FirstStmtIsTestObjNewWithThirtySeven) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment (test_obj = new(37))";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableTestObj());
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  EXPECT_EQ(newCall->getTaskFunc(), getTestNewFunction())
      << "an ordinary 'new(...)' call should resolve getTaskFunc() to the user-written constructor (see KNOWN "
         "COMPILER BUG #6 above)";
}

// THE STRUCTURAL NOTE of this file: an upcast handle assignment
// ("super_obj = test_obj;", super_cls-typed lhs, test_cls-typed rhs) is a
// plain Assignment with a plain RefObj rhs -- no explicit cast/conversion
// node, mirroring the same-type handle-copy shape from
// chapter-8/8.12--assignment/test_8.12--assignment.cpp.
TEST_F(ClassOverrideMemberTest, SecondStmtAssignsSuperObjToTestObjHandle) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment (super_obj = test_obj)";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "super_obj");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableSuperObj());

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'test_obj' (handle being upcast) should be a plain RefObj, with no cast node";
  EXPECT_EQ(rhs->getName(), "test_obj");
  EXPECT_EQ(rhs->getActual<hldb::Variable>(), getVariableTestObj());
}

TEST_F(ClassOverrideMemberTest, ThirdStmtDisplaysTestObjS) { ExpectSDisplay(2, "test_obj", getVariableTestObj()); }

// THE CRUX of member overriding: "test_obj.incs()" (test_obj statically
// typed test_cls) must resolve getTaskFunc() to test_cls's OWN "incs"
// (the override), NOT super_cls's.
TEST_F(ClassOverrideMemberTest, FourthStmtDisplaysTestObjIncs) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 3u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr) << "stmt[3] should be a $display SysTaskCall";
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj.incs()' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const varRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(varRef, nullptr);
  EXPECT_EQ(varRef->getActual<hldb::Variable>(), getVariableTestObj());

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'test_obj.incs()' second path elem should be a MethodFuncCall";
  EXPECT_EQ(call->getName(), "incs");
  EXPECT_EQ(call->getTaskFunc(), getTestIncsFunction())
      << "8.14: 'test_obj.incs()' (test_obj is statically typed test_cls) should resolve to test_cls's OWN "
         "override of 'incs', not super_cls's";
  EXPECT_NE(call->getTaskFunc(), getSuperIncsFunction())
      << "8.14: 'test_obj.incs()' must NOT resolve to super_cls's base 'incs' -- that would mean the override "
         "is being ignored";
}

TEST_F(ClassOverrideMemberTest, FifthStmtDisplaysTestObjS) { ExpectSDisplay(4, "test_obj", getVariableTestObj()); }

// THE OTHER CRUX of member overriding / static binding: "super_obj.incs()"
// (super_obj statically typed super_cls, even though its underlying
// object was constructed as a test_cls via the upcast assignment above)
// must resolve getTaskFunc() to super_cls's OWN "incs" -- neither "incs"
// is declared "virtual", so dispatch is static (compile-time), based on
// the HANDLE's declared type, not any runtime type.
TEST_F(ClassOverrideMemberTest, SixthStmtDisplaysSuperObjIncs) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 5u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(5));
  ASSERT_NE(disp, nullptr) << "stmt[5] should be a $display SysTaskCall";
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'super_obj.incs()' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const varRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(varRef, nullptr);
  EXPECT_EQ(varRef->getName(), "super_obj");
  EXPECT_EQ(varRef->getActual<hldb::Variable>(), getVariableSuperObj());

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super_obj.incs()' second path elem should be a MethodFuncCall";
  EXPECT_EQ(call->getName(), "incs");
  EXPECT_EQ(call->getTaskFunc(), getSuperIncsFunction())
      << "8.14: 'super_obj.incs()' (super_obj is statically typed super_cls, and neither 'incs' is virtual) "
         "should resolve to super_cls's OWN base 'incs', not test_cls's override -- static/compile-time dispatch";
  EXPECT_NE(call->getTaskFunc(), getTestIncsFunction())
      << "8.14: 'super_obj.incs()' must NOT resolve to test_cls's override -- that would imply (incorrect) "
         "dynamic dispatch on a non-virtual method";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassOverrideMemberTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
