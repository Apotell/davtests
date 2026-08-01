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
// ss.6.7 + ss.6.8: 'int a' has no net-type keyword (wire/tri/etc.), so per
// the standard it is a variable_declaration, not a net_declaration. It must
// be modeled as a Variable, found via Module::getVariables(), not as a Net.
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: 'a' (IntTypespec, no initial value)
//   - 1 Initial process; Initial stmt is directly SysFuncCall "$cast" (no IfStmt wrapper)
//   - $cast has 2 args: RefObj "a" -> Variable, vpiMultOp(vpiRealConst "2.1", vpiRealConst "3.7")
//   - top has no continuous assignments
//   - $cast SysFuncCall carries no static return typespec, consistent with the
//     return value being discarded in void (task) context

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class CastTask : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.24.2--cast_task.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CastTask, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Variable "a" -> IntTypespec, no inline initializer
// ----
TEST_F(CastTask, VariableAIsIntTypeWithNoValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr) << "int a; has no inline initializer -- vpiValue should be null";
}

// ----
// Initial statement is directly SysFuncCall "$cast" (no IfStmt wrapper)
// ----
TEST_F(CastTask, InitialStmtIsCastSysFuncCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr) << "$cast(a, ...) used as task: Initial stmt is SysFuncCall directly, no IfStmt";
  EXPECT_EQ(castFn->getName(), "$cast");
}

// ----
// $cast arguments: arg[0]=RefObj "a" -> Variable, arg[1]=Operation(multiply, ...)
// ----
TEST_F(CastTask, CastFuncCallHasTwoArguments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  ASSERT_NE(castFn->getArguments(), nullptr);
  EXPECT_EQ(castFn->getArguments()->size(), 2u);
}

TEST_F(CastTask, CastArgZeroIsRefToVariableA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  const hldb::RefObj *const arg0 = any_cast<hldb::RefObj>(castFn->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getName(), "a");
  EXPECT_NE(arg0->getActual<hldb::Variable>(), nullptr);
}

TEST_F(CastTask, CastArgOneIsMultiplyOperation) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

// ----
// Structural completeness
// ----
TEST_F(CastTask, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "expected exactly 1 variable: 'a'";
}

TEST_F(CastTask, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(CastTask, CastSysFuncCallHasNoStaticReturnTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  EXPECT_EQ(castFn->getTypespec(), nullptr)
      << "$cast used as a task discards its return value; HLC does not attach a static typespec";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
