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

// Validates UHDM representation of a built-in array method call:
//   reg [7:0] array [3];
//   $display("Array size %d\n", array.size());
//
// UHDM structure:
//   Net "array" → ArrayTypespec (static)
//     Range: vpiLeftRange = Operation(subtract, opType=11) with Constant "3"
//     ElemTypespec → LogicTypespec [7:0]
//   Initial → Begin
//     SysFuncCall "$display"
//       Arguments[0]: Constant (vpiStringConst=6) — format string
//       Arguments[1]: HierPath "array.size()"
//         PathElems[0]: RefObj "array"
//         PathElems[1]: FuncCall "size"
//
// Key API:
//   SysFuncCall inherits TFCall::getArguments() → AnyCollection*
//   HierPath::getPathElems()                    → AnyCollection*
//   FuncCall inherits TFCall::getName()         → std::string_view

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/begin.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/func_call.h>
#include <uhdm/hier_path.h>
#include <uhdm/initial.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/range.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/sys_func_call.h>

namespace SURELOG {

class BuiltinMethodsArrays : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.13-builtin-methods-arrays.hlc"});

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

static const uhdm::Net *getNetArray(const uhdm::Design *d) {
  const uhdm::Module *const top = getTop(d);
  if (!top || !top->getNets()) return nullptr;
  for (const uhdm::Net *const n : *top->getNets())
    if (n->getName() == "array") return n;
  return nullptr;
}

// Returns the $display SysFuncCall inside initial begin.
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
TEST_F(BuiltinMethodsArrays, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr);
}

TEST_F(BuiltinMethodsArrays, NetArrayExists) {
  ASSERT_NE(getNetArray(m_design), nullptr) << "net 'array' not found";
}

// ---------------------------------------------------------------------------
// Array typespec: static ArrayTypespec with LogicTypespec [7:0] element
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsArrays, NetArrayHasArrayTypespec) {
  const uhdm::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual()), nullptr)
      << "net 'array' should resolve to an ArrayTypespec";
}

TEST_F(BuiltinMethodsArrays, ArrayDimensionLeftRangeIsSubtractOp) {
  const uhdm::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const at =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr) << "ArrayTypespec has no range";

  // [3] is encoded as a subtract Operation in the left range boundary.
  const uhdm::Operation *const left =
      at->getRange()->getLeftExpr<uhdm::Operation>();
  ASSERT_NE(left, nullptr) << "left range should be a subtract Operation";
  EXPECT_EQ(left->getOpType(), vpiSubOp) << "expected vpiSubOp (11)";
}

TEST_F(BuiltinMethodsArrays, ArrayDimensionOperandIsConstantThree) {
  const uhdm::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const at =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  const uhdm::Operation *const left =
      at->getRange()->getLeftExpr<uhdm::Operation>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 1u);

  const uhdm::Constant *const c =
      any_cast<uhdm::Constant>((*left->getOperands())[0]);
  ASSERT_NE(c, nullptr) << "subtract operand should be a Constant";
  EXPECT_EQ(c->getDecompile(), "3") << "array dimension should be 3";
}

TEST_F(BuiltinMethodsArrays, ArrayElemTypespecIsLogicTypespec) {
  const uhdm::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const at =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(any_cast<uhdm::LogicTypespec>(at->getElemTypespec()->getActual()), nullptr)
      << "element typespec should be LogicTypespec";
}

// ---------------------------------------------------------------------------
// $display system call
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsArrays, DisplayCallExists) {
  ASSERT_NE(getDisplay(m_design), nullptr) << "$display call not found";
}

TEST_F(BuiltinMethodsArrays, DisplayCallName) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "$display");
}

TEST_F(BuiltinMethodsArrays, DisplayCallHasTwoArguments) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 2u);
}

TEST_F(BuiltinMethodsArrays, FirstArgumentIsStringConstant) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>((*c->getArguments())[0]);
  ASSERT_NE(fmt, nullptr) << "first argument should be a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(fmt->getConstType(), 6) << "format string should have string const type";
}

// ---------------------------------------------------------------------------
// array.size() — HierPath with RefObj + FuncCall
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsArrays, SecondArgumentIsHierPath) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr) << "second argument should be a HierPath";
  EXPECT_EQ(hp->getName(), "array.size()");
}

TEST_F(BuiltinMethodsArrays, HierPathHasTwoPathElems) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  EXPECT_EQ(hp->getPathElems()->size(), 2u);
}

TEST_F(BuiltinMethodsArrays, FirstPathElemIsArrayRef) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const uhdm::RefObj *const ref =
      any_cast<uhdm::RefObj>((*hp->getPathElems())[0]);
  ASSERT_NE(ref, nullptr) << "pathElems[0] should be a RefObj";
  EXPECT_EQ(ref->getName(), "array");
}

TEST_F(BuiltinMethodsArrays, SecondPathElemIsSizeFuncCall) {
  const uhdm::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const uhdm::FuncCall *const fn =
      any_cast<uhdm::FuncCall>((*hp->getPathElems())[1]);
  ASSERT_NE(fn, nullptr) << "pathElems[1] should be a FuncCall";
  EXPECT_EQ(fn->getName(), "size");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
