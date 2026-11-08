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

// Validates UHDM representation of a built-in string method call:
//   string a = "test";
//   $display("length check: %d\n", a.len());
//
// UHDM structure:
//   Variable "a" -> StringTypespec
//     vpiValue: Constant (vpiStringConst=6), getValue()="test"
//   Initial -> Begin
//     SysTaskCall "$display"
//       Arguments[0]: Constant (vpiStringConst=6) -- format string
//       Arguments[1]: MethodFuncCall "len" (IEEE 1800-2023 37.42 detail 2)
//         vpiPrefix: RefObj "a" -> Variable a (the object the method is applied to)
//         vpiTaskFunc: Function "len" of the builtin string class (6.16.1)
//
// Contrast with builtin-methods-arrays: same method-call pattern but the
// receiver is a StringTypespec variable instead of an ArrayTypespec.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/initial.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>

namespace hlc {

class BuiltinMethodsStrings : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.13-builtin-methods-strings.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByDefName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Variable *getVariableA(const hldb::Design *d) {
  const hldb::Module *const top = getTop(d);
  if (!top || !top->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>("a", top->getVariables());
}

static const hldb::SysTaskCall *getDisplay(const hldb::Design *d) {
  const hldb::Module *const top = getTop(d);
  if (!top || !top->getProcesses()) return nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      const hldb::Begin *const blk = i->getStmt<hldb::Begin>();
      if (!blk || !blk->getStmts()) return nullptr;
      for (const hldb::Any *const s : *blk->getStmts())
        if (const hldb::SysTaskCall *const c = any_cast<hldb::SysTaskCall>(s)) return c;
    }
  }
  return nullptr;
}

// ----
// Module and variable
// ----
TEST_F(BuiltinMethodsStrings, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr); }

TEST_F(BuiltinMethodsStrings, StringVariableAExists) {
  ASSERT_NE(getVariableA(m_design), nullptr) << "variable 'a' not found";
}

// `string` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 'a'
// must not appear as a Net.
TEST_F(BuiltinMethodsStrings, VariableAIsNotDuplicatedAsNet) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr)
        << "'a' is a string (variable) and must not also appear as a Net";
  }
}

// ----
// string a = "test" -- StringTypespec with initial Constant value
// ----
TEST_F(BuiltinMethodsStrings, VariableAHasStringTypespec) {
  const hldb::Variable *const n = getVariableA(m_design);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(n->getTypespec()->getActual()), nullptr)
      << "variable 'a' typespec should resolve to a StringTypespec";
}

TEST_F(BuiltinMethodsStrings, VariableAInitialValueIsStringConst) {
  const hldb::Variable *const n = getVariableA(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Constant *const val = n->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "variable 'a' should have a Constant initial value";
  // vpiStringConst = 6
  EXPECT_EQ(val->getConstType(), 6) << "initial value should have string const type";
}

TEST_F(BuiltinMethodsStrings, VariableAInitialValueIsTest) {
  const hldb::Variable *const n = getVariableA(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Constant *const val = n->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  // getValue() returns the raw string without surrounding quotes
  EXPECT_EQ(val->getValue(), "test");
}

// ----
// $display system call
// ----
TEST_F(BuiltinMethodsStrings, DisplayCallExists) {
  ASSERT_NE(getDisplay(m_design), nullptr) << "$display call not found";
}

TEST_F(BuiltinMethodsStrings, DisplayCallName) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "$display");
}

TEST_F(BuiltinMethodsStrings, DisplayCallHasTwoArguments) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 2u);
}

TEST_F(BuiltinMethodsStrings, FirstArgumentIsStringConstant) {
  const hldb::SysTaskCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = c->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr) << "first argument should be a Constant";
  EXPECT_EQ(fmt->getConstType(), 6) << "format string should have string const type";
}

// ----
// a.len() -- MethodFuncCall "len" applied to 'a' (37.42 detail 2)
// ----
static const hldb::MethodFuncCall *getLenCall(const hldb::Design *d) {
  const hldb::SysTaskCall *const c = getDisplay(d);
  if ((c == nullptr) || (c->getArguments() == nullptr) || (c->getArguments()->size() != 2)) return nullptr;
  const hldb::NamedArgument *const arg1 = c->getArguments()->at(1);
  return (arg1 == nullptr) ? nullptr : arg1->getHighConn<hldb::MethodFuncCall>();
}

TEST_F(BuiltinMethodsStrings, SecondArgumentIsLenMethodCall) {
  const hldb::MethodFuncCall *const fn = getLenCall(m_design);
  ASSERT_NE(fn, nullptr) << "second argument should be the method call a.len()";
  EXPECT_EQ(fn->getName(), std::string_view("len"));
  EXPECT_FALSE(fn->getIsProperty()) << "len() is written with parentheses";
  EXPECT_TRUE((fn->getArguments() == nullptr) || fn->getArguments()->empty()) << "len() takes no arguments";
}

TEST_F(BuiltinMethodsStrings, LenCallReceiverIsStringVariableA) {
  const hldb::MethodFuncCall *const fn = getLenCall(m_design);
  ASSERT_NE(fn, nullptr);
  const hldb::RefObj *const ref = fn->getPrefix<hldb::RefObj>();
  ASSERT_NE(ref, nullptr) << "vpiPrefix should be the receiver 'a'";
  EXPECT_EQ(ref->getName(), std::string_view("a"));
  ASSERT_NE(ref->getActual(), nullptr);
  EXPECT_EQ(ref->getActual(), getVariableA(m_design)) << "receiver 'a' binds to the string variable a";
}

// IEEE 1800-2023 6.16.1: len() is a method of the builtin string type.
TEST_F(BuiltinMethodsStrings, LenCallBindsToStringMethod) {
  const hldb::MethodFuncCall *const fn = getLenCall(m_design);
  ASSERT_NE(fn, nullptr);
  const hldb::TaskFunc *const tf = fn->getTaskFunc();
  ASSERT_NE(tf, nullptr) << "a.len() must bind";
  EXPECT_EQ(tf->getName(), std::string_view("len"));
  const hldb::ClassDefn *const owner = any_cast<hldb::ClassDefn>(tf->getParent());
  ASSERT_NE(owner, nullptr);
  EXPECT_EQ(owner->getName(), std::string_view("StringTypespec"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
