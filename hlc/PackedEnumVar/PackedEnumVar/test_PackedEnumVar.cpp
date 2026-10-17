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

// Validates the HLDB model built for tests/PackedEnumVar/dut.sv:
//
//   module top;
//      typedef enum logic [2:0] {
//         A = 3'b000,
//         B = 3'b111
//      } sp2v_e;
//      int          o;
//      function automatic sp2v_e [1:0] get_BA();
//         sp2v_e [1:0] out;
//         out[0] = B;
//         out[1] = A;
//         return out;
//      endfunction
//      assign o = int'(get_BA());
//   endmodule
//
// The point of the fixture is a packed array of an enumerated type used as a
// function's return type and local variable (IEEE 1800-2023 7.4.1), whose
// elements are written one at a time, and a cast of the function's result to
// int (6.24.1). The regression this file exists to catch is HLC losing the
// packed dimension of the enum-typed return or local, or mis-binding the
// element writes.
//
// What is checked, and why:
//   Module top (23.2)
//     - 'module top;' writes no port list, so the module has no port
//     - exactly 1 typedef and exactly 1 module variable, o, a signed int
//       (6.11). The function's local 'out' is not a module variable
//   typedef enum logic [2:0] { A = 3'b000, B = 3'b111 } sp2v_e; (6.19)
//     - its alias is an EnumTypespec whose base type is a LogicTypespec with
//       the single packed range [2:0], with exactly 2 names in source order:
//       A with the value Constant "3'b000" and B with "3'b111"
//   function automatic sp2v_e [1:0] get_BA(); (13.4)
//     - an automatic Function (13.4.2) with no formals whose return type is
//       a packed array with the single packed dimension [1:0] of sp2v_e
//     - exactly 1 local variable, out, of the same packed array type
//     - its executable statements are, in order: 2 blocking Assignments and
//       a ReturnStmt. 'out[0] = B' and 'out[1] = A' each write a bit-select
//       (11.5.1) whose prefix is bound to out, with the index Constant "0" /
//       "1", and read RefObj B / A bound to the EnumConst of that name;
//       'return out' returns a RefObj bound to out (13.4.1)
//   assign o = int'(get_BA()); (10.3, 6.24.1)
//     - exactly 1 ContAssign; LHS bound to the variable o; RHS Operation
//       vpiCastOp whose type is int and whose single operand is a call of
//       get_BA bound to the module's Function, with no argument
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the RHS of a continuous assignment is not a
// constant-expression context, so the standard does not require HLC to fold
// the call; only the instance tree is checked under getElaborated().
//
// What is NOT checked, and why:
//   - The value o holds (get_BA() is {A, B} = 6'b000111, so o is 7) only
//     exists while simulation runs, since the call is not required to be
//     folded. Permanently out of scope.
//   - Which scope owns 'out' (the function itself or a Begin wrapping its
//     body) is a tool convention, so both are searched; bare declarations in
//     a statement list are skipped.
//   - How a RefTypespec refers to sp2v_e: it may resolve to the
//     TypedefTypespec or to the EnumTypespec it aliases. Both are that type
//     (6.18), so either is accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tf_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

#include <string_view>
#include <vector>

namespace hlc {

class PackedEnumVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedEnumVar.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Typedef *getSp2vE() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("sp2v_e", top->getTypedefs());
  }

  static const hldb::EnumConst *getEnumConst(std::string_view name) {
    const hldb::Typedef *const td = getSp2vE();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr || et->getEnum() == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>(name, et->getEnum()->getEnumConsts());
  }

  static const hldb::Function *getGetBA() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("get_BA", top->getTaskFuncs());
  }

  // The function's local 'out', owned by the function or by a Begin wrapping
  // its body (a tool convention, so both are searched).
  static const hldb::Variable *getOut() {
    const hldb::Function *const fn = getGetBA();
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>("out", fn->getVariables())) return v;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("out", body->getVariables());
  }

  // The function body's statements with bare Variable declarations removed.
  static std::vector<const hldb::Any *> getExecutableStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Function *const fn = getGetBA();
    if (fn == nullptr) return stmts;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr) return stmts;
    for (const hldb::Any *const stmt : *body->getStmts()) {
      if (any_cast<hldb::Variable>(stmt) == nullptr) stmts.emplace_back(stmt);
    }
    return stmts;
  }

  // Verifies 'type' is a packed array [1:0] of sp2v_e.
  static void ExpectPackedArrayOfSp2vE(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::ArrayTypespec *const at = type->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(at, nullptr) << what << " is declared 'sp2v_e [1:0]'";
    EXPECT_TRUE(at->getPacked()) << "7.4.1: '[1:0]' follows the type, so it is packed";
    const hldb::Range *const r = at->getRange();
    ASSERT_NE(r, nullptr);
    const hldb::Constant *const l = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const rr = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(rr, nullptr);
    EXPECT_EQ(l->getDecompile(), "1");
    EXPECT_EQ(rr->getDecompile(), "0");
    const hldb::Typedef *const td = getSp2vE();
    ASSERT_NE(td, nullptr);
    ASSERT_NE(td->getAlias(), nullptr);
    ASSERT_NE(at->getElemTypespec(), nullptr);
    const hldb::Typespec *const actual = at->getElemTypespec()->getActual();
    ASSERT_NE(actual, nullptr) << "the element type must resolve";
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    EXPECT_TRUE(((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
                (actual == td->getAlias()->getActual()))
        << "7.4.1: the element type of " << what << " is sp2v_e";
  }

  // Verifies 'stmt' is 'out[<index>] = <name>;'.
  static void ExpectElementWrite(const hldb::Any *stmt, std::string_view index, std::string_view name) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
    ASSERT_NE(assign, nullptr) << "'out[" << index << "] = " << name << ";' should be an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' is a blocking assignment";
    const hldb::BitSelect *const sel = assign->getLhs<hldb::BitSelect>();
    ASSERT_NE(sel, nullptr) << "11.5.1: 'out[" << index << "]' selects one element";
    const hldb::Constant *const idx = sel->getIndex<hldb::Constant>();
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(idx->getDecompile(), index);
    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "out");
    ASSERT_NE(getOut(), nullptr);
    EXPECT_EQ(prefix->getActual(), getOut()) << "'out' is the function's local";
    const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getName(), name);
    ASSERT_NE(getEnumConst(name), nullptr);
    EXPECT_EQ(rhs->getActual(), getEnumConst(name)) << "'" << name << "' is a name of sp2v_e";
  }
};

TEST_F(PackedEnumVarTest, TopHasNoPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty())
      << "'module top;' writes no port list, so it declares no port";
}

TEST_F(PackedEnumVarTest, TopDeclaresOneTypedefAndIntVariableO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 1u) << "'sp2v_e' is the only typedef";
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 1u) << "'o' is the only module variable; 'out' is local to get_BA";
  const hldb::Variable *const o = top->getVariables()->at(0);
  ASSERT_NE(o, nullptr);
  EXPECT_EQ(o->getName(), "o");
  ASSERT_NE(o->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = o->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'o' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
}

TEST_F(PackedEnumVarTest, Sp2vEIsLogic2To0EnumWithAAndB) {
  const hldb::Typedef *const td = getSp2vE();
  ASSERT_NE(td, nullptr) << "typedef 'sp2v_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'sp2v_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "the base type is 'logic [2:0]'";
  ASSERT_NE(base->getRanges(), nullptr);
  ASSERT_EQ(base->getRanges()->size(), 1u);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);
  const char *const names[] = {"A", "B"};
  const char *const values[] = {"3'b000", "3'b111"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), names[i]) << "enum name " << i << ", in source order";
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getDecompile(), values[i]);
  }
}

TEST_F(PackedEnumVarTest, GetBAIsAutomaticReturningPackedArrayOfSp2vE) {
  const hldb::Function *const fn = getGetBA();
  ASSERT_NE(fn, nullptr) << "function 'get_BA' not found";
  EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: declared 'function automatic'";
  EXPECT_TRUE(fn->getIODecls() == nullptr || fn->getIODecls()->empty()) << "'get_BA()' declares no formal";
  ExpectPackedArrayOfSp2vE(fn->getReturn(), "the return type");
}

TEST_F(PackedEnumVarTest, LocalOutIsPackedArrayOfSp2vE) {
  const hldb::Variable *const out = getOut();
  ASSERT_NE(out, nullptr) << "local variable 'out' not found";
  ExpectPackedArrayOfSp2vE(out->getTypespec(), "'out'");
}

TEST_F(PackedEnumVarTest, BodyWritesBothElementsThenReturnsOut) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_EQ(stmts.size(), 3u) << "two element writes, then the return";
  ExpectElementWrite(stmts[0], "0", "B");
  ExpectElementWrite(stmts[1], "1", "A");
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[2]);
  ASSERT_NE(ret, nullptr) << "13.4.1: the last statement is 'return out;'";
  const hldb::RefObj *const value = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "out");
  EXPECT_EQ(value->getActual(), getOut());
}

TEST_F(PackedEnumVarTest, ContAssignCastsGetBAToInt) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("o", top->getVariables()));
  const hldb::Operation *const cast = ca->getRhs<hldb::Operation>();
  ASSERT_NE(cast, nullptr) << "'int'(...)' is an Operation";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1: a cast";
  ASSERT_NE(cast->getTypespec(), nullptr) << "the cast names its target type";
  EXPECT_NE(cast->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "6.24.1: the cast is to 'int'";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(cast->getOperands()->at(0));
  ASSERT_NE(call, nullptr) << "the cast operand is the call 'get_BA()'";
  EXPECT_EQ(call->getName(), "get_BA");
  ASSERT_NE(getGetBA(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getGetBA()) << "the call binds to the module's function";
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "'get_BA()' writes no argument";
}

TEST_F(PackedEnumVarTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

TEST_F(PackedEnumVarTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
