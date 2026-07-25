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

// Tests for 8.15--super.sv (tags: 8.15)
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
//         incs = super.incs();
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
//     end
//   endmodule
//
// IEEE 1800-2017 8.15 "The super keyword": from WITHIN an overriding
// method, "super.<method>()" explicitly calls the BASE class's own
// version of that method, bypassing the override. This file's
// "test_cls::incs()" override calls "super.incs()" as part of its own
// body -- this is the "super" analogue of "super.new(...)" (already
// confirmed in chapter-8/8.13--inheritance and
// chapter-8/8.14--override_member), but for an ORDINARY method rather
// than a constructor.
//
// KNOWN COMPILER BUG #8 (super method-call resolution, CONFIRMED via
// ctest): "super.incs()" is parsed via the generic "Implicit_class_handle"
// ("super") + ordinary subroutine call path and modeled as a
// MethodFuncCall (unlike "super.new(...)", which is parsed via a
// dedicated "Super_dot_new" grammar rule and modeled as a FuncCall).
// getTaskFunc() on this MethodFuncCall resolves to test_cls's OWN "incs"
// override -- NOT super_cls's base "incs" -- which is the exact opposite
// of what "super." is supposed to guarantee, and completely defeats the
// purpose of writing "super.incs()" at all. This is a DIFFERENT defect
// than KNOWN COMPILER BUG #6 (ordinary "new(...)" never resolves
// getTaskFunc() at all, i.e. resolves to null): here getTaskFunc() DOES
// resolve to a real Function, just the WRONG one. "super.new(...)" itself
// is unaffected (confirmed still resolving correctly to super_cls's own
// constructor via TestNewFirstStmtIsSuperNewCall below) since it takes
// the separate FuncCall path, not MethodFuncCall.
//
// Checked:
//   - design has module work@class_tb with exactly 2 nets: "test_obj"
//     (typed test_cls) and "super_obj" (typed super_cls)
//   - the module has exactly 2 nested ClassDefns: "work@super_cls" and
//     "work@test_cls"
//   - ClassDefn "super_cls": same shape as
//     chapter-8/8.14--override_member.sv's super_cls (property "s",
//     methods "incs" and "new") -- see the KNOWN COMPILER BUG notes below
//   - ClassDefn "test_cls": EXTENDS super_cls (getExtends() resolves to
//     super_cls's own ClassDefn); has exactly 1 property ("a") and
//     exactly 2 methods ("incs", "new")
//   - test_cls's "incs" (the override): a Function distinct from
//     super_cls's "incs"; 2-statement body:
//     "s += 2;" -- a blocking Assignment desugared to an explicit "add"
//     Operation, "s" resolving to super_cls's inherited property Variable
//     (test_cls does not redeclare "s")
//     "incs = super.incs();" -- a blocking Assignment whose lhs RefObj
//     "incs" resolves, via getActual<Function>(), to test_cls's OWN
//     "incs" (the same "assign to own name" idiom as
//     chapter-8/8.10--static_methods), and whose rhs is a HierPath
//     "super.incs()" -- first path elem a RefObj "super" resolving (via
//     getActual<ClassDefn>()) to super_cls's ClassDefn, second path elem
//     a MethodFuncCall "incs" whose getTaskFunc() should resolve to
//     super_cls's OWN "incs" Function per IEEE 8.15, but per KNOWN
//     COMPILER BUG #8 (confirmed) actually resolves to test_cls's own
//     override instead
//   - test_cls's "new": same shape as chapter-8/8.13--inheritance and
//     chapter-8/8.14--override_member's derived constructors --
//     "super.new(def + 3);" (a FuncCall, confirmed-working per KNOWN
//     COMPILER BUG #6's established distinction) followed by "a = def;"
//   - the initial process' Begin block has exactly 4 statements:
//     "test_obj = new(37)", "super_obj = test_obj", "$display(test_obj.s)",
//     "$display(test_obj.incs())"
//   - "test_obj = new(37)": per KNOWN COMPILER BUG #6, this ordinary
//     "new(...)" call does not resolve getTaskFunc() to the real
//     constructor
//   - "super_obj = test_obj": the same upcast-assignment shape already
//     confirmed in chapter-8/8.14--override_member.sv -- a plain
//     Assignment with a plain RefObj rhs, no cast node
//   - "$display(test_obj.s)": HierPath resolving "s" to super_cls's own
//     property Variable
//   - "$display(test_obj.incs())": "test_obj.incs()" (test_obj statically
//     typed test_cls) must resolve getTaskFunc() to test_cls's OWN
//     "incs" (the override), consistent with
//     chapter-8/8.14--override_member.sv's equivalent confirmation
//   - design-level: exactly 2 classes (work@super_cls, work@test_cls)
//
// KNOWN COMPILER BUG #2 (property visibility defaulting) and KNOWN
// COMPILER BUG #4 (a method declared directly in a class body is not
// flagged via getMethod()): already confirmed independently across other
// chapter-8 files in this suite (see
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp and
// siblings). PropertySIsPublicByDefault, PropertyAIsPublicByDefault,
// SuperIncsIsRecognizedAsClassMethod, and TestIncsIsRecognizedAsClassMethod
// below assert the IEEE-mandated behavior and will FAIL until these are
// fixed.
//
// KNOWN COMPILER BUG #6 (constructor call resolution): already confirmed
// across chapter-8/8.7--constructor/test_8.7--constructor.cpp and
// siblings. FirstStmtIsTestObjNewWithThirtySeven below asserts the
// IEEE-mandated resolution and will FAIL until this is fixed. This is a
// DIFFERENT defect than KNOWN COMPILER BUG #8 above: here getTaskFunc()
// resolves to null (never resolves at all); for BUG #8, getTaskFunc()
// resolves to a real Function, just the wrong one.
//
// FORMERLY KNOWN COMPILER BUG #1 (class lifetime defaulting): confirmed
// fixed upstream per the ctest runs described in
// chapter-8/8.12--assignment/test_8.12--assignment.cpp and
// chapter-8/8.11--this/test_8.11--this.cpp. SuperClsIsAutomaticByDefault
// and TestClsIsAutomaticByDefault below are plain passing assertions
// accordingly.

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
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassSuperTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.15--super.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@class_tb", m_design->getAllModules());
  }

  static const hldb::ClassDefn *getSuperClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("work@super_cls", top->getClassDefns());
  }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("work@test_cls", top->getClassDefns());
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

  static const hldb::Net *getNetTestObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("test_obj", top->getNets());
  }

  static const hldb::Net *getNetSuperObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("super_obj", top->getNets());
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassSuperTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassSuperTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(ClassSuperTest, ModuleHasTwoClassDefns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 2u);
}

// --- class "super_cls" ---------------------------------------------------------

TEST_F(ClassSuperTest, SuperClsExists) { EXPECT_NE(getSuperClsDefn(), nullptr); }

TEST_F(ClassSuperTest, SuperClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassSuperTest, SuperClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class super_cls' has no lifetime qualifier so it defaults to automatic";
}

TEST_F(ClassSuperTest, SuperClsHasOnePropertyS) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
}

TEST_F(ClassSuperTest, PropertySHasInitializerTwo) {
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  const hldb::Constant *const init = s->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "2");
}

TEST_F(ClassSuperTest, PropertySIsPublicByDefault) {
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getVisibility(), vpiPublicVis) << "8.14: 'int s' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassSuperTest, SuperClsHasTwoMethods) {
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

TEST_F(ClassSuperTest, SuperIncsIsRecognizedAsClassMethod) {
  const hldb::Function *const incs = getSuperIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_TRUE(incs->getMethod()) << "see KNOWN COMPILER BUG #4 above";
}

TEST_F(ClassSuperTest, SuperIncsReturnTypeIsPlainInt) {
  const hldb::Function *const incs = getSuperIncsFunction();
  ASSERT_NE(incs, nullptr);
  ASSERT_NE(incs->getReturn(), nullptr);
  const hldb::IntTypespec *const ret = incs->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ret, nullptr);
}

TEST_F(ClassSuperTest, SuperIncsBodyHasTwoStmts) {
  const hldb::Function *const incs = getSuperIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassSuperTest, SuperNewIsRecognizedAsConstructor) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ret = ctor->getReturn()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ret, nullptr);
  EXPECT_EQ(ret->getClassDefn(), getSuperClsDefn());
}

TEST_F(ClassSuperTest, SuperNewHasOneIODeclDefWithDefaultThree) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = ctor->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  const hldb::Constant *const def = io->getExpr<hldb::Constant>();
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getDecompile(), "3");
}

TEST_F(ClassSuperTest, SuperNewBodyAssignsSToDef) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Assignment *const assign = ctor->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'s = def;' is super_cls's only statement, so it is NOT wrapped in a Begin";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyS());
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassSuperTest, TestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassSuperTest, TestClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassSuperTest, TestClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic";
}

TEST_F(ClassSuperTest, TestClsExtendsSuperCls) {
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

TEST_F(ClassSuperTest, TestClsHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
}

TEST_F(ClassSuperTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "see KNOWN COMPILER BUG #2 above";
}

TEST_F(ClassSuperTest, TestClsHasTwoMethods) {
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

TEST_F(ClassSuperTest, TestIncsIsDistinctFromSuperIncs) {
  const hldb::Function *const testIncs = getTestIncsFunction();
  const hldb::Function *const superIncs = getSuperIncsFunction();
  ASSERT_NE(testIncs, nullptr);
  ASSERT_NE(superIncs, nullptr);
  EXPECT_NE(testIncs, superIncs);
}

TEST_F(ClassSuperTest, TestIncsIsRecognizedAsClassMethod) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_TRUE(incs->getMethod()) << "see KNOWN COMPILER BUG #4 above";
}

TEST_F(ClassSuperTest, TestIncsBodyHasTwoStmts) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassSuperTest, TestIncsFirstStmtIsSPlusEqualsTwo) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment ('s += 2;')";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyS());
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
}

// THE CRUX of this file, and KNOWN COMPILER BUG #8 (confirmed):
// "incs = super.incs();" -- the lhs correctly resolves to test_cls's OWN
// "incs" (assign-to-own-name idiom, as usual), and "super" correctly
// resolves to super_cls's ClassDefn, but the call itself (a
// MethodFuncCall, NOT a FuncCall as "super.new(...)" uses) resolves
// getTaskFunc() to test_cls's OWN override instead of super_cls's "incs"
// -- the opposite of what "super." is supposed to guarantee. This
// EXPECT_EQ against getSuperIncsFunction() asserts the IEEE-mandated
// resolution and FAILS until this is fixed; the paired EXPECT_NE against
// getTestIncsFunction() documents the actual (wrong) value it currently
// resolves to.
TEST_F(ClassSuperTest, TestIncsSecondStmtCallsSuperIncs) {
  const hldb::Function *const incs = getTestIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('incs = super.incs();')";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'incs' (return-value write target) should be a RefObj";
  EXPECT_EQ(lhs->getActual<hldb::Function>(), incs)
      << "assigning to 'incs' inside the override should resolve to test_cls's OWN incs Function";

  const hldb::HierPath *const path = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(path, nullptr) << "'super.incs()' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const superRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(superRef, nullptr);
  EXPECT_EQ(superRef->getName(), "super");
  EXPECT_EQ(superRef->getActual<hldb::ClassDefn>(), getSuperClsDefn());

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super.incs()' second path elem should be a MethodFuncCall (unlike "
                              "'super.new(...)', which is a FuncCall)";
  EXPECT_EQ(call->getName(), "incs");
  EXPECT_EQ(call->getTaskFunc(), getSuperIncsFunction())
      << "8.15: 'super.incs()' should resolve getTaskFunc() to super_cls's OWN 'incs', bypassing the override";
  EXPECT_NE(call->getTaskFunc(), getTestIncsFunction())
      << "8.15: 'super.incs()' must NOT resolve to test_cls's own override -- that would defeat the purpose of "
         "'super'";
}

TEST_F(ClassSuperTest, TestNewIsRecognizedAsConstructor) {
  const hldb::Function *const ctor = getTestNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ret = ctor->getReturn()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ret, nullptr);
  EXPECT_EQ(ret->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassSuperTest, TestNewHasOneIODeclDefWithDefaultFortyTwo) {
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

TEST_F(ClassSuperTest, TestNewFirstStmtIsSuperNewCall) {
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
  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super.new(...)' second path elem should be a FuncCall (contrast with "
                              "'super.incs()' above, which is a MethodFuncCall)";
  EXPECT_EQ(call->getTaskFunc(), getSuperNewFunction());
}

TEST_F(ClassSuperTest, TestNewSecondStmtAssignsAToDef) {
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

// --- nets "test_obj" / "super_obj" ---------------------------------------------

TEST_F(ClassSuperTest, NetTestObjExists) { EXPECT_NE(getNetTestObj(), nullptr); }

TEST_F(ClassSuperTest, NetTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj = getNetTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassSuperTest, NetSuperObjExists) { EXPECT_NE(getNetSuperObj(), nullptr); }

TEST_F(ClassSuperTest, NetSuperObjTypespecResolvesToSuperClsClassDefn) {
  const hldb::Net *const superObj = getNetSuperObj();
  ASSERT_NE(superObj, nullptr);
  ASSERT_NE(superObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = superObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getSuperClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassSuperTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassSuperTest, InitialBeginHasFourStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u);
}

// See KNOWN COMPILER BUG #6 above.
TEST_F(ClassSuperTest, FirstStmtIsTestObjNewWithThirtySeven) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment (test_obj = new(37))";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetTestObj());
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  EXPECT_EQ(newCall->getTaskFunc(), getTestNewFunction())
      << "an ordinary 'new(...)' call should resolve getTaskFunc() to the user-written constructor (see KNOWN "
         "COMPILER BUG #6 above)";
}

TEST_F(ClassSuperTest, SecondStmtAssignsSuperObjToTestObjHandle) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment (super_obj = test_obj)";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetSuperObj());
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'test_obj' (handle being upcast) should be a plain RefObj, with no cast node";
  EXPECT_EQ(rhs->getActual<hldb::Net>(), getNetTestObj());
}

TEST_F(ClassSuperTest, ThirdStmtDisplaysTestObjS) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 2u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr) << "stmt[2] should be a $display SysTaskCall";
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj.s' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj());
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getActual<hldb::Variable>(), getPropertyS());
}

// "test_obj.incs()" (test_obj statically typed test_cls) must resolve to
// test_cls's OWN override, same confirmation as
// chapter-8/8.14--override_member.sv.
TEST_F(ClassSuperTest, FourthStmtDisplaysTestObjIncs) {
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
  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj());

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'test_obj.incs()' second path elem should be a MethodFuncCall";
  EXPECT_EQ(call->getName(), "incs");
  EXPECT_EQ(call->getTaskFunc(), getTestIncsFunction())
      << "'test_obj.incs()' (test_obj is statically typed test_cls) should resolve to test_cls's OWN override";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassSuperTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
