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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>

namespace hlc {

class BuiltinMethodsArrays : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.13-builtin-methods-arrays.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNetArray(const hldb::Design *d) {
  const hldb::Module *const top = getTop(d);
  if (!top || !top->getNets()) return nullptr;
  for (const hldb::Net *const n : *top->getNets())
    if (n->getName() == "array") return n;
  return nullptr;
}

// Returns the $display SysFuncCall inside initial begin.
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
TEST_F(BuiltinMethodsArrays, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr); }

TEST_F(BuiltinMethodsArrays, NetArrayExists) { ASSERT_NE(getNetArray(m_design), nullptr) << "net 'array' not found"; }

// ---------------------------------------------------------------------------
// Array typespec: static ArrayTypespec with LogicTypespec [7:0] element
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsArrays, NetArrayHasArrayTypespec) {
  const hldb::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual()), nullptr)
      << "net 'array' should resolve to an ArrayTypespec";
}

TEST_F(BuiltinMethodsArrays, ArrayDimensionLeftRangeIsSubtractOp) {
  const hldb::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr) << "ArrayTypespec has no range";

  // [3] is encoded as a subtract Operation in the left range boundary.
  const hldb::Operation *const left = at->getRange()->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "left range should be a subtract Operation";
  EXPECT_EQ(left->getOpType(), vpiSubOp) << "expected vpiSubOp (11)";
}

TEST_F(BuiltinMethodsArrays, ArrayDimensionOperandIsConstantThree) {
  const hldb::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  const hldb::Operation *const left = at->getRange()->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 1u);

  const hldb::Constant *const c = any_cast<hldb::Constant>((*left->getOperands())[0]);
  ASSERT_NE(c, nullptr) << "subtract operand should be a Constant";
  EXPECT_EQ(c->getDecompile(), "3") << "array dimension should be 3";
}

TEST_F(BuiltinMethodsArrays, ArrayElemTypespecIsLogicTypespec) {
  const hldb::Net *const n = getNetArray(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(any_cast<hldb::LogicTypespec>(at->getElemTypespec()->getActual()), nullptr)
      << "element typespec should be LogicTypespec";
}

// ---------------------------------------------------------------------------
// $display system call
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsArrays, DisplayCallExists) {
  ASSERT_NE(getDisplay(m_design), nullptr) << "$display call not found";
}

TEST_F(BuiltinMethodsArrays, DisplayCallName) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "$display");
}

TEST_F(BuiltinMethodsArrays, DisplayCallHasTwoArguments) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  EXPECT_EQ(c->getArguments()->size(), 2u);
}

TEST_F(BuiltinMethodsArrays, FirstArgumentIsStringConstant) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>((*c->getArguments())[0]);
  ASSERT_NE(fmt, nullptr) << "first argument should be a Constant";
  // vpiStringConst = 6
  EXPECT_EQ(fmt->getConstType(), 6) << "format string should have string const type";
}

// ---------------------------------------------------------------------------
// array.size() — HierPath with RefObj + FuncCall
// ---------------------------------------------------------------------------
TEST_F(BuiltinMethodsArrays, SecondArgumentIsHierPath) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getArguments(), nullptr);
  ASSERT_EQ(c->getArguments()->size(), 2u);

  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr) << "second argument should be a HierPath";
  EXPECT_EQ(hp->getName(), "array.size");
}

TEST_F(BuiltinMethodsArrays, HierPathHasTwoPathElems) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  EXPECT_EQ(hp->getPathElems()->size(), 2u);
}

TEST_F(BuiltinMethodsArrays, FirstPathElemIsArrayRef) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const hldb::RefObj *const ref = any_cast<hldb::RefObj>((*hp->getPathElems())[0]);
  ASSERT_NE(ref, nullptr) << "pathElems[0] should be a RefObj";
  EXPECT_EQ(ref->getName(), "array");
}

TEST_F(BuiltinMethodsArrays, SecondPathElemIsSizeMethodFuncCall) {
  const hldb::SysFuncCall *const c = getDisplay(m_design);
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>((*c->getArguments())[1]);
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const hldb::MethodFuncCall *const fn = any_cast<hldb::MethodFuncCall>((*hp->getPathElems())[1]);
  ASSERT_NE(fn, nullptr) << "pathElems[1] should be a MethodFuncCall";
  EXPECT_EQ(fn->getName(), "size");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
