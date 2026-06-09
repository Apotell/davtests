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

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/begin.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/func_call.h>
#include <uhdm/hier_path.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>
#include <uhdm/sys_func_call.h>

namespace SURELOG {

class BuiltinMethodsStrings : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.13-builtin-methods-strings.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

static const uhdm::Net *getNetA(const uhdm::Design *d) {
  const uhdm::Module *const top = getTop(d);
  if (!top || !top->getNets()) return nullptr;
  for (const uhdm::Net *const n : *top->getNets())
    if (n->getName() == "a") return n;
  return nullptr;
}

static const uhdm::SysFuncCall *getDisplay(const uhdm::Design *d) {
  const uhdm::Module *const top = getTop(d);
  if (!top || !top->getProcesses()) return nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      const uhdm::Begin *const blk = i->getStmt<uhdm::Begin>();
      if (!blk || !blk->getStmts()) return nullptr;
      for (const uhdm::Any *const s : *blk->getStmts())
        if (const uhdm::SysFuncCall *const c = any_cast<uhdm::SysFuncCall>(s))
          return c;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module and net
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsStrings, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr);
}

TEST_F(BuiltinMethodsStrings, StringNetAExists) {
  ASSERT_NE(getNetA(m_design), nullptr) << "net 'a' not found";
}

// ---------------------------------------------------------------------------
// string a = "test" — StringTypespec with initial Constant value
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsStrings, NetAHasStringTypespec) {
  const uhdm::Net *const n = getNetA(m_design);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(any_cast<uhdm::StringTypespec>(n->getTypespec()->getActual()), nullptr)
      << "net 'a' typespec should resolve to a StringTypespec";
}

TEST_F(BuiltinMethodsStrings, NetAInitialValueIsStringConst) {
  const uhdm::Net *const n = getNetA(m_design);
  ASSERT_NE(n, nullptr);

  const uhdm::Constant *const val = n->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "net 'a' should have a Constant initial value";
  // vpiStringConst = 6
  EXPECT_EQ(val->getConstType(), 6) << "initial value should have string const type";
}

TEST_F(BuiltinMethodsStrings, NetAInitialValueIsTest) {
  const uhdm::Net *const n = getNetA(m_design);
  ASSERT_NE(n, nullptr);

  const uhdm::Constant *const val = n->getValue<uhdm::Constant>();
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
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "$display");
}

TEST_F(BuiltinMethodsStrings, DisplayCallHasTwoArguments) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 2u);
}

TEST_F(BuiltinMethodsStrings, FirstArgumentIsStringConstant) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>((*c->getArguments())[0]);
  ASSERT_NE(fmt, nullptr) << "first argument should be a Constant";
  EXPECT_EQ(fmt->getConstType(), 6) << "format string should have string const type";
}

// ---------------------------------------------------------------------------
// a.len() — HierPath with RefObj + FuncCall
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsStrings, SecondArgumentIsHierPath) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr) << "second argument should be a HierPath";
  EXPECT_EQ(hp->getName(), "a.len()");
}

TEST_F(BuiltinMethodsStrings, HierPathHasTwoPathElems) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  EXPECT_EQ(hp->getPathElems()->size(), 2u);
}

TEST_F(BuiltinMethodsStrings, FirstPathElemIsStringRefA) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const uhdm::RefObj *const ref =
      any_cast<uhdm::RefObj>((*hp->getPathElems())[0]);
  ASSERT_NE(ref, nullptr) << "pathElems[0] should be a RefObj";
  EXPECT_EQ(ref->getName(), "a");
}

TEST_F(BuiltinMethodsStrings, SecondPathElemIsLenFuncCall) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const uhdm::FuncCall *const fn =
      any_cast<uhdm::FuncCall>((*hp->getPathElems())[1]);
  ASSERT_NE(fn, nullptr) << "pathElems[1] should be a FuncCall";
  EXPECT_EQ(fn->getName(), "len");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
