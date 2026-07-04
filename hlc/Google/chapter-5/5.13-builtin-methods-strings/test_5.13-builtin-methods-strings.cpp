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
//   Net "a" → StringTypespec
//     vpiValue: Constant (vpiStringConst=6), getValue()="test"
//   Initial → Begin
//     SysFuncCall "$display"
//       Arguments[0]: Constant (vpiStringConst=6) — format string
//       Arguments[1]: HierPath "a.len()"
//         PathElems[0]: RefObj "a"
//         PathElems[1]: FuncCall "len"
//
// Contrast with builtin-methods-arrays: same HierPath/FuncCall pattern
// but the receiver is a StringTypespec net instead of an ArrayTypespec.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>

namespace hlc {

class BuiltinMethodsStrings : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.13-builtin-methods-strings.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNetA(const hldb::Design *d) {
  const hldb::Module *const top = getTop(d);
  if (!top || !top->getNets()) return nullptr;
  for (const hldb::Net *const n : *top->getNets())
    if (n->getName() == "a") return n;
  return nullptr;
}

static const hldb::SysFuncCall *getDisplay(const hldb::Design *d) {
  const hldb::Module *const top = getTop(d);
  if (!top || !top->getProcesses()) return nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      const hldb::Begin *const blk = i->getStmt<hldb::Begin>();
      if (!blk || !blk->getStmts()) return nullptr;
      for (const hldb::Any *const s : *blk->getStmts())
        if (const hldb::SysFuncCall *const c = any_cast<hldb::SysFuncCall>(s)) return c;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module and net
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsStrings, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr); }

TEST_F(BuiltinMethodsStrings, StringNetAExists) { ASSERT_NE(getNetA(m_design), nullptr) << "net 'a' not found"; }

// ---------------------------------------------------------------------------
// string a = "test" — StringTypespec with initial Constant value
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsStrings, NetAHasStringTypespec) {
  const hldb::Net *const n = getNetA(m_design);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(n->getTypespec()->getActual()), nullptr)
      << "net 'a' typespec should resolve to a StringTypespec";
}

TEST_F(BuiltinMethodsStrings, NetAInitialValueIsStringConst) {
  const hldb::Net *const n = getNetA(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Constant *const val = n->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "net 'a' should have a Constant initial value";
  // vpiStringConst = 6
  EXPECT_EQ(val->getConstType(), 6) << "initial value should have string const type";
}

TEST_F(BuiltinMethodsStrings, NetAInitialValueIsTest) {
  const hldb::Net *const n = getNetA(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Constant *const val = n->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  // getValue() returns the raw string without surrounding quotes
  EXPECT_EQ(val->getValue(), "test");
}

// ---------------------------------------------------------------------------
// $display system call
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsStrings, DisplayCallExists) {
  ASSERT_NE(getDisplay(m_design), nullptr) << "$display call not found";
}

TEST_F(BuiltinMethodsStrings, DisplayCallName) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "$display");
}

TEST_F(BuiltinMethodsStrings, DisplayCallHasTwoArguments) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 2u);
}

TEST_F(BuiltinMethodsStrings, FirstArgumentIsStringConstant) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>((*c->getArguments())[0]);
  ASSERT_NE(fmt, nullptr) << "first argument should be a Constant";
  EXPECT_EQ(fmt->getConstType(), 6) << "format string should have string const type";
}

// ---------------------------------------------------------------------------
// a.len() — HierPath with RefObj + FuncCall
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsStrings, SecondArgumentIsHierPath) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr) << "second argument should be a HierPath";
  EXPECT_EQ(hp->getName(), "a.len()");
}

TEST_F(BuiltinMethodsStrings, HierPathHasTwoPathElems) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  EXPECT_EQ(hp->getPathElems()->size(), 2u);
}

TEST_F(BuiltinMethodsStrings, FirstPathElemIsStringRefA) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const hldb::RefObj *const ref = any_cast<hldb::RefObj>((*hp->getPathElems())[0]);
  ASSERT_NE(ref, nullptr) << "pathElems[0] should be a RefObj";
  EXPECT_EQ(ref->getName(), "a");
}

TEST_F(BuiltinMethodsStrings, SecondPathElemIsLenFuncCall) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const hldb::MethodFuncCall *const fn = any_cast<hldb::MethodFuncCall>((*hp->getPathElems())[1]);
  ASSERT_NE(fn, nullptr) << "pathElems[1] should be a MethodFuncCall";
  EXPECT_EQ(fn->getName(), "len");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
