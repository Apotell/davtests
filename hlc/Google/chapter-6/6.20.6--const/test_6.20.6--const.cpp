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

// Spec-based validation of IEEE 1800-2017 ss.6.20.6 const variable.
// SV: tests/Google/chapter-6/6.20.6--const.sv
//
//   module top();
//       class test_cls;
//           int a;
//           task test_method(int val);
//               $display("test_method");
//               a += val;
//           endtask
//       endclass
//
//       const test_cls test_obj = new;
//   endmodule
//
// -- ss.6.20.6 rules under test -----------------------------------------------
//
// Const variable (ss.6.20.6):
//   * The 'const' qualifier declares an elaboration-time constant.
//   * For a class-type variable, 'const' applies to the HANDLE, not the object:
//     the handle 'test_obj' cannot be reassigned after initialization, but the
//     object's members (e.g. 'a') can still be modified.
//   * 'const test_cls test_obj = new' declares a constant handle initialized
//     with a freshly constructed object via the 'new' constructor.
//
// ss.6.7 + ss.6.8: 'const' does not change net-vs-variable status by itself.
// 'test_cls' is a class type with no net-type keyword (wire/tri/etc.), so
// 'const test_cls test_obj' is a variable_declaration, not a net_declaration.
// It must be modeled as a Variable, found via Module::getVariables(), not
// as a Net.
//
// Class type (ss.8):
//   * 'test_cls' is a user-defined class declared inside the module.
//   * UHDM represents the class declaration as a ClassDefn node in
//     Module::getClassDefns().
//   * The type of 'test_obj' resolves to a ClassTypespec pointing to the
//     ClassDefn for 'test_cls'.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:top
//   +-- getClassDefns() (ClassDefnCollection, 1 item)
//   |   +-- [0] ClassDefn name:"test_cls"
//   |           getVariables() (VariableCollection, 1 item):
//   |             +-- [0] Variable name:"a"
//   |                         typespec: RefTypespec -> IntTypespec
//   |           getMethods() (TaskFuncCollection, 1 item):
//   |             +-- [0] Task name:"test_method"
//   +-- getVariables() (VariableCollection, 1 item)
//       +-- [0] Variable name:"test_obj"
//               typespec: RefTypespec -> ClassTypespec name:"test_cls"
//               value:    MethodFuncCall name:"new"
//
// NOTE: 'const test_cls test_obj' must be represented as a Variable node,
// not a Net node. The object initialization '= new' is stored as a
// MethodFuncCall in Variable::getValue(), not via a ParamAssign.
//
// NOTE on const qualification: the HLDB Variable type exposes no isConst() or
// equivalent method. The const qualifier is not directly testable through
// the public HLDB API in this respect.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>

#include <string>

namespace hlc {

class ConstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.6--const.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::ClassDefn *getClassDefn(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getClassDefns()) return nullptr;
  return hldb::findByName<hldb::ClassDefn>(name, m->getClassDefns());
}

static const hldb::Variable *getVar(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>(name, m->getVariables());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ConstTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// class test_cls  (ss.8)
// ===========================================================================

// ss.8: 'class test_cls' must produce a ClassDefn node inside the module.
TEST_F(ConstTest, ClassDefnCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getClassDefns(), nullptr) << "module 'top' must have a class definition collection";
}

// ss.8: exactly one class is declared in this module.
TEST_F(ConstTest, ClassDefnCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getClassDefns(), nullptr);
  EXPECT_EQ(m->getClassDefns()->size(), 1u) << "module 'top' declares exactly one class: test_cls";
}

// ss.8: the class is named 'test_cls'; UHDM uses the fully-qualified form
// 'test_cls'.
TEST_F(ConstTest, TestCls_Exists) {
  EXPECT_NE(getClassDefn(m_design, "test_cls"), nullptr)
      << "ClassDefn 'test_cls' not found in module class definitions";
}

// ===========================================================================
// class test_cls -- member variable 'a'  (ss.8)
// ===========================================================================

// ss.8: 'class test_cls' declares one data member 'int a'. The variable
// collection of the ClassDefn (via Scope::getVariables()) must be non-null.
TEST_F(ConstTest, TestCls_HasVariables) {
  const hldb::ClassDefn *cd = getClassDefn(m_design, "test_cls");
  ASSERT_NE(cd, nullptr);
  EXPECT_NE(cd->getVariables(), nullptr) << "ss.8: class 'test_cls' must have a non-null variable collection";
}

// ss.8: exactly one data member ('int a') is declared in 'test_cls'.
TEST_F(ConstTest, TestCls_VariableCount) {
  const hldb::ClassDefn *cd = getClassDefn(m_design, "test_cls");
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getVariables(), nullptr);
  EXPECT_EQ(cd->getVariables()->size(), 1u) << "ss.8: class 'test_cls' declares exactly one member variable: a";
}

// ss.8: the data member 'a' must be registered in the class scope.
TEST_F(ConstTest, TestCls_Variable_A_Exists) {
  const hldb::ClassDefn *cd = getClassDefn(m_design, "test_cls");
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("a", cd->getVariables()), nullptr)
      << "ss.8: member variable 'a' must appear in the class scope";
}

// ===========================================================================
// class test_cls -- task 'test_method'  (ss.8)
// ===========================================================================

// ss.8: 'class test_cls' declares one task 'test_method'. The method
// collection (ClassDefn::getMethods()) must be non-null.
TEST_F(ConstTest, TestCls_HasMethods) {
  const hldb::ClassDefn *cd = getClassDefn(m_design, "test_cls");
  ASSERT_NE(cd, nullptr);
  EXPECT_NE(cd->getMethods(), nullptr) << "ss.8: class 'test_cls' must have a non-null method collection";
}

// ss.8: exactly one task ('test_method') is declared in 'test_cls'.
TEST_F(ConstTest, TestCls_MethodCount) {
  const hldb::ClassDefn *cd = getClassDefn(m_design, "test_cls");
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getMethods(), nullptr);
  EXPECT_EQ(cd->getMethods()->size(), 1u) << "ss.8: class 'test_cls' declares exactly one method: test_method";
}

// ss.8: the task 'test_method' must be registered in the method collection.
TEST_F(ConstTest, TestCls_Method_TestMethod_Exists) {
  const hldb::ClassDefn *cd = getClassDefn(m_design, "test_cls");
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getMethods(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::TaskFunc>("test_method", cd->getMethods()), nullptr)
      << "ss.8: task 'test_method' must appear in the class method collection";
}

// ===========================================================================
// const test_cls test_obj = new  (ss.6.20.6)
// ===========================================================================

// ss.6.20.6 + ss.6.7/ss.6.8: 'const test_cls test_obj' has no net-type
// keyword, so it must be represented as a Variable node. The module's
// variable collection must exist and be non-null.
TEST_F(ConstTest, VariableCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getVariables(), nullptr) << "ss.6.20.6: 'const test_cls test_obj' must produce a non-null variable collection";
}

// ss.6.20.6: exactly one variable (the const handle) is present in this module.
TEST_F(ConstTest, VariableCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 1u) << "module 'top' has exactly one variable: test_obj";
}

// ss.6.20.6: 'test_obj' must appear in getVariables().
TEST_F(ConstTest, TestObj_Exists) {
  EXPECT_NE(getVar(m_design, "test_obj"), nullptr) << "'test_obj' not found in variable collection";
}

// ss.6.7 + ss.6.8: a const class handle with no net-type keyword is a
// Variable, NOT a Net. 'test_obj' must not appear in the module's net
// collection (if the collection is even populated).
TEST_F(ConstTest, TestObj_NotInNets) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getNets() == nullptr) return;
  EXPECT_TRUE(m->getNets()->empty()) << "ss.6.20.6: 'const test_cls test_obj' must NOT appear in getNets()";
}

// ss.6.20.6 + ss.8: the typespec of 'test_obj' must be non-null since the
// variable has an explicit class type 'test_cls'.
TEST_F(ConstTest, TestObj_TypespecExists) {
  const hldb::Variable *v = getVar(m_design, "test_obj");
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec(), nullptr) << "ss.6.20.6: 'const test_cls test_obj' must have a non-null typespec";
}

// ss.8: the explicit 'test_cls' type must resolve to a ClassTypespec.
TEST_F(ConstTest, TestObj_Typespec_IsClassTypespec) {
  const hldb::Variable *v = getVar(m_design, "test_obj");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *rt = v->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ClassTypespec>(), nullptr) << "ss.8: 'test_cls' must resolve to a ClassTypespec";
}

// ss.8: the ClassTypespec must refer to the 'test_cls' class defined in the
// module (fully qualified as 'test_cls').
TEST_F(ConstTest, TestObj_Typespec_ClassIs_TestCls) {
  const hldb::Variable *v = getVar(m_design, "test_obj");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *rt = v->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::ClassTypespec *ct = rt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getName(), "test_cls") << "ss.8: ClassTypespec must point to the 'test_cls' class definition";
}

// ss.6.20.6: 'const test_cls test_obj = new' initializes the handle with a
// new object. The initializer must be a MethodFuncCall (the 'new' constructor).
TEST_F(ConstTest, TestObj_InitValue_IsNew) {
  const hldb::Variable *v = getVar(m_design, "test_obj");
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getValue<hldb::MethodFuncCall>(), nullptr)
      << "ss.6.20.6: '= new' must be represented as a MethodFuncCall";
}

// ss.6.20.6: the constructor call must be named 'new'.
TEST_F(ConstTest, TestObj_InitValue_Named_New) {
  const hldb::Variable *v = getVar(m_design, "test_obj");
  ASSERT_NE(v, nullptr);
  const hldb::MethodFuncCall *mfc = v->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(mfc, nullptr);
  EXPECT_EQ(mfc->getName(), "new") << "ss.6.20.6: the constructor call must be named 'new'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
