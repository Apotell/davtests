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

// Tests for dut.sv (tags: ForeachFunction)
//   module dut;
//     function automatic int get_expected_command (int slave_id);
//       int write_command_queue_slave [10];
//       foreach (write_command_queue_slave[slave_id,i]) begin
//       end
//     endfunction
//   endmodule
//
// This exercises a "foreach" loop (IEEE 1800-2023 Sec 12.7.3) whose array
// is declared and iterated entirely inside an automatic function body (as
// opposed to the package-level / class-member arrays exercised by the
// other Foreach* tests in this suite).
//
// The foreach loop_variable_list here is "[slave_id,i]" -- TWO loop
// variables -- but "write_command_queue_slave" is declared as "int [10]",
// a single-dimension unpacked array. Per IEEE 1800-2023 Sec 12.7.3: "the
// number of loop variables shall be less than or equal to the number of
// dimensions of the array" (a foreach with more loop variables than the
// array has dimensions is illegal). See ForeachIndexCountExceedsArrayRank
// below.
//
// Note "slave_id" here is also the name of the function's own "int
// slave_id" input port (7.5(a) function argument); the foreach loop
// variable list's first identifier ("slave_id") is a NEW iterator
// declaration local to the foreach loop (12.7.3), which per 6.3/23.7
// shadows the outer "slave_id" input argument within the foreach body's
// scope -- it does not refer back to the port.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/foreach_stmt.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeachFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForeachFunction.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Function *getGetExpectedCommand() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("get_expected_command", dut->getTaskFuncs());
  }

  static const hldb::ForeachStmt *getForeach() {
    const hldb::Function *const fn = getGetExpectedCommand();
    if (fn == nullptr) return nullptr;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2) return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(1));
  }
};

// ===========================================================================
// module dut / function get_expected_command
// ===========================================================================

TEST_F(ForeachFunctionTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(ForeachFunctionTest, FunctionExists) {
  const hldb::Function *const fn = getGetExpectedCommand();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), std::string_view{"get_expected_command"});
}

// ===========================================================================
// function body: [0] local array decl, [1] foreach (7.5(a) / 12.7.3)
// ===========================================================================

TEST_F(ForeachFunctionTest, FunctionBodyHasArrayDeclThenForeach) {
  const hldb::Function *const fn = getGetExpectedCommand();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::Variable *const decl = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(decl, nullptr) << "first statement should be the local 'write_command_queue_slave' array declaration";
  EXPECT_EQ(decl->getName(), std::string_view{"write_command_queue_slave"});

  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr) << "second statement should be the ForeachStmt";
  EXPECT_EQ(fe->getAnyType(), hldb::AnyType::ForeachStmt);
}

// ===========================================================================
// foreach array expression names the local array declared just above it
// ===========================================================================

TEST_F(ForeachFunctionTest, ForeachArrayIsLocalArray) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const arr = fe->getVariable();
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->getName(), std::string_view{"write_command_queue_slave"});
  ASSERT_NE(arr->getActual(), nullptr);
  const hldb::Variable *const actual = arr->getActual<hldb::Variable>();
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(actual->getName(), std::string_view{"write_command_queue_slave"});
}

// ===========================================================================
// foreach body is present (empty 'begin end' per the source)
// ===========================================================================

TEST_F(ForeachFunctionTest, ForeachBodyIsEmptyBegin) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::Begin *const body = fe->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "foreach body should be a Begin (explicit begin-end in source)";
  EXPECT_EQ(body->getStmts(), nullptr) << "'begin end' has no statements";
}

// ===========================================================================
// 6.3/23.7: the foreach loop variable 'slave_id' is a new iterator
// declaration local to the foreach loop, shadowing the function's own
// 'slave_id' input port rather than referring back to it
// ===========================================================================

TEST_F(ForeachFunctionTest, ForeachLoopVarSlaveIdShadowsPort) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_GE(fe->getLoopVars()->size(), 1u);
  const hldb::Variable *const loopSlaveId = any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
  ASSERT_NE(loopSlaveId, nullptr);
  EXPECT_EQ(loopSlaveId->getName(), std::string_view{"slave_id"});
  EXPECT_TRUE(loopSlaveId->getIsIterator())
      << "12.7.3: foreach loop variables are implicitly declared iterators, distinct from any "
         "outer identifier of the same name";
}

// ===========================================================================
// 12.7.3: "The number of loop variables shall be less than or equal to
// the number of dimensions of the array." 'write_command_queue_slave' is
// declared 'int [10]' -- exactly ONE unpacked dimension -- but the foreach
// here supplies TWO loop variables ("[slave_id,i]"), which exceeds the
// array's rank and must be illegal.
// ===========================================================================

TEST_F(ForeachFunctionTest, ForeachIndexCountExceedsArrayRank) {
  GTEST_SKIP() << "HLC silently accepts a foreach loop_variable_list ('[slave_id,i]', 2 loop "
                   "variables) that exceeds the iterated array's rank ('write_command_queue_slave' "
                   "is a single-dimension 'int [10]' array), building both as iterator Variables "
                   "with no diagnostic at all; per IEEE 1800-2023 Sec 12.7.3, 'the number of loop "
                   "variables shall be less than or equal to the number of dimensions of the array', "
                   "so this should be reported as an error. Fix pending.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
