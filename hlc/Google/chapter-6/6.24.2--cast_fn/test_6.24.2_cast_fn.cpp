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

// Validates the UHDM graph for a module using the $cast system function:
//   module top();
//     int a;
//     initial
//       if (! $cast(a, 2.1 * 3.7))
//         $display("cast failed");
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'a' (IntTypespec, no initial value)
//   - 1 Initial process; Initial stmt = IfStmt
//   - IfStmt condition = vpiNotOp( SysFuncCall "$cast" )
//   - $cast has 2 args: RefObj "a" → Net, vpiMultOp(vpiRealConst "2.1", vpiRealConst "3.7")
//   - IfStmt then-branch = SysFuncCall "$display" with arg vpiStringConst "\"cast failed\""
//   - work@top has no continuous assignments
//
// Not checked:
//   - IfStmt has no else branch
//   - $cast return value (1=success / 0=failure, runtime-only)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/if_stmt.h>
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

class CastFn : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.24.2--cast_fn.hlc"});

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

TEST_F(CastFn, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net "a" → IntTypespec, no inline initializer
// ---------------------------------------------------------------------------
TEST_F(CastFn, NetAIsIntTypeWithNoValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr)
      << "int a; has no inline initializer — vpiValue should be null";
}

// ---------------------------------------------------------------------------
// Initial → IfStmt
// ---------------------------------------------------------------------------
TEST_F(CastFn, InitialHasIfStmt) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt<hldb::IfStmt>(), nullptr);
}

// ---------------------------------------------------------------------------
// IfStmt condition = Operation(vpiNotOp=3) with SysFuncCall "$cast"
// ---------------------------------------------------------------------------
TEST_F(CastFn, IfConditionIsNotOperation) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::IfStmt *const ifStmt = init->getStmt<hldb::IfStmt>();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiNotOp)
      << "! $cast(...) wraps the SysFuncCall in a NOT operation";
}

TEST_F(CastFn, NotOperandIsCastSysFuncCall) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::IfStmt *const ifStmt = init->getStmt<hldb::IfStmt>();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);
  const hldb::SysFuncCall *const castFn =
      any_cast<hldb::SysFuncCall>(cond->getOperands()->at(0));
  ASSERT_NE(castFn, nullptr);
  EXPECT_EQ(castFn->getName(), "$cast");
}

// ---------------------------------------------------------------------------
// $cast arguments: arg[0]=RefObj "a", arg[1]=Operation(multiply, 2.1, 3.7)
// ---------------------------------------------------------------------------
TEST_F(CastFn, CastFuncCallHasTwoArguments) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::IfStmt *const ifStmt = init->getStmt<hldb::IfStmt>();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  const hldb::SysFuncCall *const castFn =
      any_cast<hldb::SysFuncCall>(cond->getOperands()->at(0));
  ASSERT_NE(castFn, nullptr);
  ASSERT_NE(castFn->getArguments(), nullptr);
  EXPECT_EQ(castFn->getArguments()->size(), 2u);
}

TEST_F(CastFn, CastArgZeroIsRefToNetA) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::IfStmt *const ifStmt = init->getStmt<hldb::IfStmt>();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  const hldb::SysFuncCall *const castFn =
      any_cast<hldb::SysFuncCall>(cond->getOperands()->at(0));
  ASSERT_NE(castFn, nullptr);
  const hldb::RefObj *const arg0 =
      any_cast<hldb::RefObj>(castFn->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getName(), "a");
  EXPECT_NE(arg0->getActual<hldb::Net>(), nullptr);
}

TEST_F(CastFn, CastArgOneIsMultiplyOperation) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::IfStmt *const ifStmt = init->getStmt<hldb::IfStmt>();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  const hldb::SysFuncCall *const castFn =
      any_cast<hldb::SysFuncCall>(cond->getOperands()->at(0));
  ASSERT_NE(castFn, nullptr);
  const hldb::Operation *const multOp =
      any_cast<hldb::Operation>(castFn->getArguments()->at(1));
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
// IfStmt then-branch = SysFuncCall "$display" with string arg "cast failed"
// ---------------------------------------------------------------------------
TEST_F(CastFn, IfBodyIsDisplayCall) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::IfStmt *const ifStmt = init->getStmt<hldb::IfStmt>();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::SysFuncCall *const display = ifStmt->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(display, nullptr);
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 1u);
  const hldb::Constant *const msg =
      any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->getConstType(), vpiStringConst);
  EXPECT_EQ(msg->getDecompile(), "\"cast failed\"");
}

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(CastFn, OneNetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'a'";
}

TEST_F(CastFn, NoContAssigns) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
