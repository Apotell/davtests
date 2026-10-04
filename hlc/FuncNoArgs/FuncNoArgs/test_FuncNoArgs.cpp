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

// Tests for FuncNoArgs.hlc (tests/FuncNoArgs/dut.sv):
//   module my_opt_reduce_or();
//     parameter A_SIGNED = 0;
//     parameter A_WIDTH = 1;
//     parameter logic [1:0][2:0] _TECHMAP_CONSTMSK_A_ = '{0, 0};
//
//     function integer count_nonconst_bits;
//       integer i;
//       begin
//         count_nonconst_bits = 0;
//         for (i = 0; i < A_WIDTH; i=i+1)
//           if (!_TECHMAP_CONSTMSK_A_[i])
//             count_nonconst_bits = count_nonconst_bits+1;
//       end
//     endfunction
//
//     reg [count_nonconst_bits()-1:0] tmp;
//   endmodule
//
// "function integer count_nonconst_bits;" declares zero formal arguments --
// there is no "input"/"output"/"inout" declaration list at all (IEEE
// 1800-2023 13.4: the port_list of a function_declaration is optional; a
// function with an empty port list takes no arguments and, per 13.4.3,
// "a constant function can be a function with no arguments" is a
// legal/common case for use inside constant expressions such as the
// unpacked/packed range of "reg [count_nonconst_bits()-1:0] tmp;").
//
// What is checked:
//   - module my_opt_reduce_or and its single function
//     "count_nonconst_bits" exist
//   - the function is NOT automatic (13.4.2: default static lifetime)
//   - the function has NO formal IODecls at all (null or empty collection)
//   - the function's return typespec resolves to IntegerTypespec
//   - the function body is a Begin block of 2 statements: an Assignment
//     to the function's own name ("count_nonconst_bits = 0;", the
//     return-by-name idiom of 13.4.1) followed by a ForStmt
//   - a local variable "i" of type IntegerTypespec exists in the
//     function's own scope (declared via "integer i;" inside the function)
//
// What is NOT checked and why: the body of the ForStmt (the nested IfStmt
// and its bit-select condition), and the module-level "tmp" declaration
// that calls "count_nonconst_bits()" as a constant function call in its
// range, are unrelated to the "function with no formal arguments" shape
// under test and are left unchecked. No .log file was consulted to decide
// this file's shape.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/function.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncNoArgsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncNoArgs.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("my_opt_reduce_or", m_design->getAllModules());
  }

  static const hldb::Function *getCountNonconstBits() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("count_nonconst_bits", top->getTaskFuncs());
  }
};

TEST_F(FuncNoArgsTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(FuncNoArgsTest, ModuleHasExactlyOneFunction) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
  EXPECT_NE(getCountNonconstBits(), nullptr);
}

TEST_F(FuncNoArgsTest, FunctionIsStaticNotAutomatic) {
  const hldb::Function *const f = getCountNonconstBits();
  ASSERT_NE(f, nullptr);
  EXPECT_FALSE(f->getAutomatic()) << "13.4.2: default lifetime is static when 'automatic' is not specified";
}

TEST_F(FuncNoArgsTest, FunctionHasNoFormalArguments) {
  const hldb::Function *const f = getCountNonconstBits();
  ASSERT_NE(f, nullptr);
  EXPECT_TRUE(f->getIODecls() == nullptr || f->getIODecls()->empty())
      << "'function integer count_nonconst_bits;' declares no port list at all";
}

TEST_F(FuncNoArgsTest, FunctionReturnsInteger) {
  const hldb::Function *const f = getCountNonconstBits();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getReturn(), nullptr);
  EXPECT_NE(f->getReturn()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "'function integer count_nonconst_bits' should resolve its return typespec to IntegerTypespec";
}

TEST_F(FuncNoArgsTest, FunctionHasLocalIntegerVariableI) {
  const hldb::Function *const f = getCountNonconstBits();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getVariables(), nullptr);
  const hldb::Variable *const i = hldb::findByName<hldb::Variable>("i", f->getVariables());
  ASSERT_NE(i, nullptr) << "'integer i;' should be a local Variable in the function's own scope";
  ASSERT_NE(i->getTypespec(), nullptr);
  EXPECT_NE(i->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr);
}

TEST_F(FuncNoArgsTest, FunctionBodyIsBeginBlockWithAssignmentThenForStmt) {
  const hldb::Function *const f = getCountNonconstBits();
  ASSERT_NE(f, nullptr);
  const hldb::Begin *const body = f->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'begin ... end' should produce a Begin block";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::Assignment *const asgn = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(asgn, nullptr) << "'count_nonconst_bits = 0;' should be a plain Assignment";
  const hldb::RefObj *const lhs = asgn->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("count_nonconst_bits"))
      << "13.4.1: assigning to the function's own name is the return-by-name idiom";

  const hldb::ForStmt *const forStmt = any_cast<hldb::ForStmt>(body->getStmts()->at(1));
  ASSERT_NE(forStmt, nullptr) << "'for (...) ...;' should produce a ForStmt";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
