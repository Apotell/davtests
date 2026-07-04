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

// Validates the UHDM graph for a module using $cast as a task (void context):
//   module top();
//     int a;
//     initial
//       $cast(a, 2.1 * 3.7);
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'a' (IntTypespec, no initial value)
//   - 1 Initial process; Initial stmt is directly SysFuncCall "$cast" (no IfStmt wrapper)
//   - $cast has 2 args: RefObj "a" → Net, vpiMultOp(vpiRealConst "2.1", vpiRealConst "3.7")
//   - work@top has no continuous assignments
//
// Not checked:
//   - when $cast is used as a task, the return value is discarded (void context)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class CastTask : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.24.2--cast_task.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CastTask, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net "a" → IntTypespec, no inline initializer
// ---------------------------------------------------------------------------
TEST_F(CastTask, NetAIsIntTypeWithNoValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr) << "int a; has no inline initializer — vpiValue should be null";
}

// ---------------------------------------------------------------------------
// Initial statement is directly SysFuncCall "$cast" (no IfStmt wrapper)
// ---------------------------------------------------------------------------
TEST_F(CastTask, InitialStmtIsCastSysFuncCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr) << "$cast(a, ...) used as task: Initial stmt is SysFuncCall directly, no IfStmt";
  EXPECT_EQ(castFn->getName(), "$cast");
}

// ---------------------------------------------------------------------------
// $cast arguments: arg[0]=RefObj "a" → Net, arg[1]=Operation(multiply, ...)
// ---------------------------------------------------------------------------
TEST_F(CastTask, CastFuncCallHasTwoArguments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  ASSERT_NE(castFn->getArguments(), nullptr);
  EXPECT_EQ(castFn->getArguments()->size(), 2u);
}

TEST_F(CastTask, CastArgZeroIsRefToNetA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  const hldb::RefObj *const arg0 = any_cast<hldb::RefObj>(castFn->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getName(), "a");
  EXPECT_NE(arg0->getActual<hldb::Net>(), nullptr);
}

TEST_F(CastTask, CastArgOneIsMultiplyOperation) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  const hldb::Operation *const multOp = any_cast<hldb::Operation>(castFn->getArguments()->at(1));
  ASSERT_NE(multOp, nullptr);
  EXPECT_EQ(multOp->getOpType(), vpiMultOp);
  ASSERT_NE(multOp->getOperands(), nullptr);
  ASSERT_EQ(multOp->getOperands()->size(), 2u);
  const hldb::Constant *const lhs = any_cast<hldb::Constant>(multOp->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getConstType(), vpiRealConst);
  EXPECT_EQ(lhs->getDecompile(), "2.1");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(multOp->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "3.7");
}

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(CastTask, OneNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'a'";
}

TEST_F(CastTask, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
