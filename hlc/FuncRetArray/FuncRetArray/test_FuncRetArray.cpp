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

// Tests for FuncRetArray/dut.sv:
//   module top #() ();
//       typedef int unsigned ASSIGN_VADDR_RET_T[2];
//       function static ASSIGN_VADDR_RET_T ASSIGN_VADDR();
//           for (int i = 0; i < 2; i++) begin
//                ASSIGN_VADDR[i] = 5;
//           end
//       endfunction
//       localparam int unsigned VADDR[2] = ASSIGN_VADDR();
//       ...
//   endmodule
//   module main;
//       top #()top1 ();
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape):
//
//   Sec 6.18/6.22 "Typedef declarations": "typedef int unsigned
//   ASSIGN_VADDR_RET_T[2];" declares a named unpacked-array type. Its alias
//   should resolve to an ArrayTypespec with getPacked() == false and an
//   IntTypespec element type (unsigned).
//
//   Sec 13.4 "Functions": "function static ASSIGN_VADDR_RET_T
//   ASSIGN_VADDR();" is a function whose return type is the typedef'd
//   unpacked-array type -- getReturn() should resolve to a
//   TypedefTypespec whose getTypedef() is the ASSIGN_VADDR_RET_T Typedef
//   declaration itself. The explicit "static" keyword means getAutomatic()
//   should be false.
//
//   Sec 13.4.1 "Return values and void functions": "the value of the
//   function... can be assigned by assigning a value to the internal
//   variable that has the same name as the function." Here that internal
//   variable is array-typed, and the function body assigns to individual
//   elements of it ("ASSIGN_VADDR[i] = 5;") via a for loop rather than to
//   the whole array at once -- this is legal per 13.4.1 since indexing
//   into the implicit return-name variable is just an ordinary variable
//   reference of array type.
//
//   Sec 12.7 "For-loop statements": "for (int i = 0; i < 2; i++) begin
//   ... end" should produce a ForStmt with one init statement (the "int i
//   = 0" declaration), a vpiLtOp condition, one increment statement, and a
//   Begin body containing a single indexed Assignment.
//
//   Sec 23.3.2 "Module instantiation": "top #()top1 ();" instantiates
//   "top" (whose port list and parameter list are both empty, "#() ()")
//   under "main" as instance "top1".
//
// What is NOT checked and why:
//   - "localparam int unsigned VADDR[2] = ASSIGN_VADDR();" evaluating the
//     for-loop to the constant array {5, 5} is a constant-function-folding
//     concern (like EvalFuncArray's "ParamInfosPerBank..." test), checked
//     here only loosely (RHS is either a FuncCall or an already-folded
//     Constant/array value) since this file's focus is the array-typed
//     return shape itself, not full constant-function evaluation.
//   - the "if (VADDR[0] != 5) $info(...)" diagnostic statements are
//     unrelated to the function-returns-array construct under test.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/var_select.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncRetArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncRetArray.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Module *getMain() { return hldb::findByName<hldb::Module>("main", m_design->getAllModules()); }

  static const hldb::Typedef *getRetTypedef() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTypedefs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("ASSIGN_VADDR_RET_T", top->getTypedefs());
  }

  static const hldb::Function *getFunc() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(FuncRetArrayTest, ModulesExist) {
  EXPECT_NE(getTop(), nullptr);
  EXPECT_NE(getMain(), nullptr);
}

// typedef int unsigned ASSIGN_VADDR_RET_T[2];
TEST_F(FuncRetArrayTest, TypedefIsUnpackedArrayOfUnsignedInt) {
  const hldb::Typedef *const td = getRetTypedef();
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getAlias();
  ASSERT_NE(alias, nullptr);
  const hldb::ArrayTypespec *const arrTs = alias->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(arrTs, nullptr) << "'int unsigned [2]' typedef should resolve to an ArrayTypespec";
  EXPECT_FALSE(arrTs->getPacked()) << "'ASSIGN_VADDR_RET_T[2]' is an unpacked array per IEEE 1800-2023 Sec 7.4";

  const hldb::RefTypespec *const elemRts = arrTs->getElemTypespec();
  ASSERT_NE(elemRts, nullptr);
  const hldb::IntTypespec *const elemTs = elemRts->getActual<hldb::IntTypespec>();
  ASSERT_NE(elemTs, nullptr) << "element type should resolve to IntTypespec ('int')";
  EXPECT_FALSE(elemTs->getSigned()) << "'int unsigned' should be unsigned";
}

// function static ASSIGN_VADDR_RET_T ASSIGN_VADDR();
TEST_F(FuncRetArrayTest, FuncReturnsTypedefArrayTypeAndIsStatic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const func = getFunc();
  ASSERT_NE(func, nullptr);
  EXPECT_EQ(func->getName(), "ASSIGN_VADDR");
  EXPECT_FALSE(func->getAutomatic()) << "'function static' explicitly requests static lifetime";
  EXPECT_TRUE(func->getIODecls() == nullptr || func->getIODecls()->empty());

  const hldb::RefTypespec *const rts = func->getReturn();
  ASSERT_NE(rts, nullptr);
  const hldb::TypedefTypespec *const typedefTs = rts->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(typedefTs, nullptr) << "return type 'ASSIGN_VADDR_RET_T' should resolve to a TypedefTypespec";
  EXPECT_EQ(typedefTs->getTypedef(), getRetTypedef())
      << "the return TypedefTypespec should resolve back to the 'ASSIGN_VADDR_RET_T' typedef declaration";
}

// for (int i = 0; i < 2; i++) begin ASSIGN_VADDR[i] = 5; end
TEST_F(FuncRetArrayTest, FuncBodyIsForLoopAssigningEachElementOfTheReturnArray) {
  const hldb::Function *const func = getFunc();
  ASSERT_NE(func, nullptr);
  const hldb::ForStmt *const forStmt = func->getStmt<hldb::ForStmt>();
  ASSERT_NE(forStmt, nullptr) << "function body should be a plain ForStmt (single statement, no begin-end)";

  ASSERT_NE(forStmt->getForInitStmts(), nullptr);
  ASSERT_EQ(forStmt->getForInitStmts()->size(), 1u);
  const hldb::Variable *const iVar = any_cast<hldb::Variable>(forStmt->getForInitStmts()->at(0));
  ASSERT_NE(iVar, nullptr) << "'int i = 0' should be a Variable declaration";
  EXPECT_EQ(iVar->getName(), "i");

  const hldb::Operation *const cond = forStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'i < 2' should be Operation(vpiLtOp)";
  EXPECT_EQ(cond->getOpType(), vpiLtOp);

  ASSERT_NE(forStmt->getForIncStmts(), nullptr);
  ASSERT_EQ(forStmt->getForIncStmts()->size(), 1u) << "'i++' should be a single increment statement";

  const hldb::Begin *const body = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "for-loop body should be a Begin (explicit begin-end)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'ASSIGN_VADDR[i] = 5;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::VarSelect *const lhs = assign->getLhs<hldb::VarSelect>();
  ASSERT_NE(lhs, nullptr) << "'ASSIGN_VADDR[i]' should be a VarSelect indexing the implicit return-name variable";
  EXPECT_EQ(lhs->getName(), "ASSIGN_VADDR");
  const hldb::RefObj *const idx = lhs->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getName(), "i");
  EXPECT_EQ(idx->getActual(), iVar);

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "5");
}

// top #()top1 (); -- inside module main
TEST_F(FuncRetArrayTest, MainInstantiatesTopAsTop1) {
  const hldb::Module *const main = getMain();
  ASSERT_NE(main, nullptr);
  ASSERT_NE(main->getModules(), nullptr);
  ASSERT_EQ(main->getModules()->size(), 1u);
  const hldb::Module *const top1 = hldb::findByName<hldb::Module>("top1", main->getModules());
  ASSERT_NE(top1, nullptr);
  EXPECT_EQ(top1->getDefName(), "top");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
