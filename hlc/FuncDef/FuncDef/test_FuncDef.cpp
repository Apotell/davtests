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

// Tests for FuncDef.hlc (tests/FuncDef/dut.sv):
//   module mulAddRecFNToRaw_preMul ();
//     function integer clog2;
//       input integer a;
//       begin
//         a = a - 1;
//         for (clog2 = 0; a > 0; clog2 = clog2 + 1) a = a>>1;
//       end
//     endfunction
//     countLeadingZeros#(clog2(prodWidth + 4) - 1)
//       countLeadingZeros_notCDom(notCDom_reduced2AbsSigSum, notCDom_normDistReduced2);
//   endmodule
//
// This is a basic (non-ANSI, Verilog-2001-style) function definition: IEEE
// 1800-2023 13.4 "Functions". "function integer clog2; input integer a; ...
// endfunction" declares a function whose return type is "integer" (13.4.1:
// "the data type of the return value ... shall be integer" is the default
// when unspecified, and here it is specified explicitly), with a single
// formal argument "a" declared via a separate "input integer a;" (13.3
// non-ANSI style), and no "automatic" keyword, so per 13.4.2 the function
// has static lifetime by default in this (module, not class/interface)
// context. The function body assigns to the function's own name ("clog2")
// to return a value, per 13.4.1's "return by assignment to the function
// name" rule -- there is no explicit "return" statement here.
//
// What is checked:
//   - the module and its single function "clog2" exist
//   - the function is NOT automatic (static, default lifetime)
//   - the function's return typespec resolves to IntegerTypespec
//   - the function has exactly 1 formal IODecl "a", direction vpiInput,
//     typespec IntegerTypespec
//   - the function body is a Begin block with 2 statements: a plain
//     Assignment ("a = a - 1;") followed by a ForStmt
//   - the ForStmt's condition is a binary "a > 0" Operation (vpiGtOp)
//
// What is NOT checked and why: the module instantiation of the unresolved
// "countLeadingZeros" module and the undeclared identifier "prodWidth" used
// in its parameter expression are unrelated to the function-definition
// construct under test here and are left unchecked (no .log file was
// consulted to decide this).

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
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncDefTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncDef.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("mulAddRecFNToRaw_preMul", m_design->getAllModules());
  }

  static const hldb::Function *getClog2() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("clog2", top->getTaskFuncs());
  }
};

TEST_F(FuncDefTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(FuncDefTest, ModuleHasExactlyOneFunctionClog2) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
  EXPECT_NE(getClog2(), nullptr);
}

TEST_F(FuncDefTest, Clog2IsStaticNotAutomatic) {
  const hldb::Function *const clog2 = getClog2();
  ASSERT_NE(clog2, nullptr);
  EXPECT_FALSE(clog2->getAutomatic()) << "13.4.2: default lifetime is static when 'automatic' is not specified";
}

TEST_F(FuncDefTest, Clog2ReturnsInteger) {
  const hldb::Function *const clog2 = getClog2();
  ASSERT_NE(clog2, nullptr);
  ASSERT_NE(clog2->getReturn(), nullptr);
  EXPECT_NE(clog2->getReturn()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "'function integer clog2' should resolve its return typespec to IntegerTypespec";
}

TEST_F(FuncDefTest, Clog2HasOneInputIODeclA) {
  const hldb::Function *const clog2 = getClog2();
  ASSERT_NE(clog2, nullptr);
  ASSERT_NE(clog2->getIODecls(), nullptr);
  ASSERT_EQ(clog2->getIODecls()->size(), 1u);
  const hldb::IODecl *const a = clog2->getIODecls()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), std::string_view("a"));
  EXPECT_EQ(a->getDirection(), vpiInput);
  ASSERT_NE(a->getTypespec(), nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "'input integer a' should resolve to IntegerTypespec";
}

TEST_F(FuncDefTest, Clog2BodyIsBeginBlockWithAssignmentThenForStmt) {
  const hldb::Function *const clog2 = getClog2();
  ASSERT_NE(clog2, nullptr);
  const hldb::Begin *const body = clog2->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'begin ... end' should produce a Begin block";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::Assignment *const firstAssign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(firstAssign, nullptr) << "'a = a - 1;' should be a plain Assignment";
  EXPECT_TRUE(firstAssign->getBlocking());
  const hldb::RefObj *const lhs = firstAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("a"));

  const hldb::ForStmt *const forStmt = any_cast<hldb::ForStmt>(body->getStmts()->at(1));
  ASSERT_NE(forStmt, nullptr) << "the 'for (...)' loop should produce a ForStmt";
}

TEST_F(FuncDefTest, ForStmtConditionIsGreaterThanOperation) {
  const hldb::Function *const clog2 = getClog2();
  ASSERT_NE(clog2, nullptr);
  const hldb::Begin *const body = clog2->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);
  const hldb::ForStmt *const forStmt = any_cast<hldb::ForStmt>(body->getStmts()->at(1));
  ASSERT_NE(forStmt, nullptr);
  const hldb::Operation *const cond = forStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'a > 0' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiGtOp);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
