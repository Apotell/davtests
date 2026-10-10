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

// Tests for FuncDefaultVal.hlc (tests/FuncDefaultVal/dut.sv):
//   module top();
//     bit[31:0] [31:0] gpr[2];
//     function string dasm( input tid=0);
//       dasm = (opcode[1:0] == 2'b11) ? dasm32(opcode, pc, tid) : dasm16(opcode, pc, tid);
//       if(regn) gpr[tid][regn] = regv;
//     endfunction
//   endmodule
//
// "input tid=0" is an ANSI-style function formal port (IEEE 1800-2023 13.3)
// carrying a default value ("=0"), the construct under test. Two rules
// apply:
//   - 13.3: a default value on a function/task port is only used when the
//     corresponding argument is omitted from a call; it does not change the
//     port's direction (explicit "input" here) or make it optional-looking
//     in the declaration itself -- it is recorded as the port's default
//     expression.
//   - 1800-2023 6.8/13.3: when a variable's data type is omitted (as here:
//     "input tid=0" gives no type before 'tid'), the default data type is
//     "logic" (a single unpacked/unranged scalar bit when no range is
//     given either).
//
// What is checked:
//   - module top and its single function "dasm" exist
//   - IODecl "tid": name, direction vpiInput, default-value expr present
//     and equal to Constant "0", typespec resolves to LogicTypespec, scalar
//   - "dasm" returns string -> StringTypespec
//   - "dasm" body is a Begin block of 2 statements: an Assignment to
//     "dasm" (the function's own name, IEEE 13.4.1 return-by-name idiom)
//     whose rhs is a ternary Operation (vpiConditionOp), followed by an
//     IfStmt
//
// What is NOT checked and why: the undeclared identifiers referenced in the
// body ("opcode", "pc", "regn", "regv", "dasm32", "dasm16") are unrelated to
// the default-value-on-a-function-argument construct under test and are
// left unchecked; no .log file was consulted to decide this file's shape.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/if_stmt.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncDefaultValTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncDefaultVal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getDasm() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("dasm", top->getTaskFuncs());
  }
};

TEST_F(FuncDefaultValTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(FuncDefaultValTest, ModuleHasFunctionDasm) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
  EXPECT_NE(getDasm(), nullptr);
}

TEST_F(FuncDefaultValTest, DasmReturnsString) {
  const hldb::Function *const dasm = getDasm();
  ASSERT_NE(dasm, nullptr);
  ASSERT_NE(dasm->getReturn(), nullptr);
  EXPECT_NE(dasm->getReturn()->getActual<hldb::StringTypespec>(), nullptr)
      << "'function string dasm(...)' should resolve its return typespec to StringTypespec";
}

TEST_F(FuncDefaultValTest, TidIsInputWithDefaultValueZero) {
  const hldb::Function *const dasm = getDasm();
  ASSERT_NE(dasm, nullptr);
  ASSERT_NE(dasm->getIODecls(), nullptr);
  ASSERT_EQ(dasm->getIODecls()->size(), 1u);
  const hldb::IODecl *const tid = dasm->getIODecls()->at(0);
  ASSERT_NE(tid, nullptr);
  EXPECT_EQ(tid->getName(), std::string_view("tid"));
  EXPECT_EQ(tid->getDirection(), vpiInput) << "13.3: the explicit 'input' keyword sets the port direction";

  ASSERT_NE(tid->getExpr(), nullptr) << "'=0' should record a default-value expression on the IODecl";
  const hldb::Constant *const defVal = tid->getExpr<hldb::Constant>();
  ASSERT_NE(defVal, nullptr) << "the default value expression should be a Constant";
  EXPECT_EQ(defVal->getDecompile(), std::string_view("0"));
}

TEST_F(FuncDefaultValTest, TidHasNoExplicitTypeSoDefaultsToScalarLogic) {
  const hldb::Function *const dasm = getDasm();
  ASSERT_NE(dasm, nullptr);
  ASSERT_NE(dasm->getIODecls(), nullptr);
  ASSERT_EQ(dasm->getIODecls()->size(), 1u);
  const hldb::IODecl *const tid = dasm->getIODecls()->at(0);
  ASSERT_NE(tid, nullptr);
  ASSERT_NE(tid->getTypespec(), nullptr) << "'input tid=0' should still resolve to a typespec (default logic)";
  const hldb::LogicTypespec *const lt = tid->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "1800-2023 6.8/13.3: an omitted data type defaults to 'logic'";
  EXPECT_TRUE(lt->getScalar()) << "no range given, so 'tid' should be a single-bit scalar logic";
}

TEST_F(FuncDefaultValTest, DasmBodyIsBeginBlockWithAssignmentThenIfStmt) {
  const hldb::Function *const dasm = getDasm();
  ASSERT_NE(dasm, nullptr);
  const hldb::Begin *const body = dasm->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "the function's two top-level statements should be grouped into a Begin block";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::Assignment *const asgn = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(asgn, nullptr) << "'dasm = (...) ? ... : ...;' should be a plain Assignment";
  const hldb::RefObj *const lhs = asgn->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("dasm"))
      << "13.4.1: assigning to the function's own name is the return-by-name idiom";
  const hldb::Operation *const rhs = asgn->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "the ternary rhs should be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiConditionOp);

  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(1));
  ASSERT_NE(ifStmt, nullptr) << "'if(regn) ...;' should be an IfStmt";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
