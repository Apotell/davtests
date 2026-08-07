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

// Tests for 6.24.2--cast_task.sv (tags: 6.24.2 8.16)
//   module top();
//     int a;
//     initial
//       $cast(a, 2.1 * 3.7);
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, checked before any test code was written):
//   6.8's data_type grammar lists "integer_atom_type" ("int" among them)
//   as a variable-declaring alternative, never a net_type (6.7). "int a"
//   declared directly in a module body must therefore be a Variable, not
//   a Net, regardless of module-level scope. This file has no
//   :should_fail_because: tag -- it is legal per spec.
//
//   A prior version of this test used hldb::Net/getNets() for "a" --
//   the same net/variable misclassification bug found and fixed across
//   6.5, 6.9.1, 6.12, 6.13, 6.14, 6.16, 6.17, 6.18, 6.19, 6.23, and
//   6.24.1/6.24.2--cast_fn this session. This version targets
//   hldb::Variable for "a" instead.
//
// What is checked:
//   - module top has no Nets and exactly 1 Variable "a" (IntTypespec, no
//     inline initializer)
//   - 1 Initial process; Initial stmt is directly SysFuncCall "$cast"
//     (no IfStmt wrapper, since the return value is discarded)
//   - $cast has 2 args: RefObj "a" -> Variable, vpiMultOp(vpiRealConst
//     "2.1", vpiRealConst "3.7")
//   - top has no continuous assignments
//   - $cast SysFuncCall carries no static return typespec, consistent
//     with the return value being discarded in void (task) context
//   - compiler reports zero errors (this file is fully legal per 6.8)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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

class CastTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.24.2--cast_task.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(CastTaskTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Variable "a" -> IntTypespec, no inline initializer
// ---------------------------------------------------------------------------
TEST_F(CastTaskTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'int a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'int a' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u) << "expected exactly 1 variable: 'a'";
}

TEST_F(CastTaskTest, AIsIntTypeWithNoValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr) << "int a; has no inline initializer -- vpiValue should be null";
}

// ---------------------------------------------------------------------------
// Initial statement is directly SysFuncCall "$cast" (no IfStmt wrapper)
// ---------------------------------------------------------------------------
TEST_F(CastTaskTest, InitialStmtIsCastSysFuncCall) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr) << "$cast(a, ...) used as task: Initial stmt is SysFuncCall directly, no IfStmt";
  EXPECT_EQ(castFn->getName(), "$cast");
}

// ---------------------------------------------------------------------------
// $cast arguments: arg[0]=RefObj "a" -> Variable, arg[1]=Operation(multiply, ...)
// ---------------------------------------------------------------------------
TEST_F(CastTaskTest, CastFuncCallHasTwoArguments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  ASSERT_NE(castFn->getArguments(), nullptr);
  EXPECT_EQ(castFn->getArguments()->size(), 2u);
}

TEST_F(CastTaskTest, CastArgZeroIsRefToVariableA) {
  const hldb::Module *const top = getTop();
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

TEST_F(CastTaskTest, CastArgOneIsMultiplyOperation) {
  const hldb::Module *const top = getTop();
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
TEST_F(CastTaskTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(CastTaskTest, CastSysFuncCallHasNoStaticReturnTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const castFn = init->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(castFn, nullptr);
  EXPECT_EQ(castFn->getTypespec(), nullptr)
      << "$cast used as a task discards its return value; HLC does not attach a static typespec";
}

TEST_F(CastTaskTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
