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

// ---------------------------------------------------------------------------
// SV source under test: tests/Google/chapter-18/18.16--random-weighted-case-randcase_0.sv
// (IEEE 1800-2023 18.16 "Random weighted case--randcase")
//
//   /*
//   :name: random_weighted_case_randcase_0
//   :description: randcase test
//   :tags: 18.16
//   */
//
//   function int F();
//       int a;
//       randcase
//           0 : a = 5;
//           1 : a = 10;
//       endcase
//       return a;
//   endfunction
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error.
//   2. Function 'F' is declared at compilation-unit scope: not a class
//      method, returns int, takes no argument, declares exactly one local
//      variable 'int a' (not rand).
//   3. F's body is exactly two statements in order: the randcase, then
//      'return a;' whose expression binds to the local variable a.
//   4. The randcase (Syntax 18-12): no case expression, exactly two items in
//      source order:
//        [0] weight 0 : "a = 5;"   (lhs bound to a, rhs literal 5)
//        [1] weight 1 : "a = 10;"  (lhs bound to a, rhs literal 10)
//      The weights are "non-negative integral values" (18.16).
//
//  LOCAL RAND TYPE: the local 'a' has vpiRandType == vpiNotRand
//   (37.17 details 15/22: the value is one of vpiRand/vpiRandC/vpiNotRand).
//
// CHECKED (TEST_F below):
//   - items 1 to 4.
//
// NOT CHECKED (and why):
//   - Run time: "If a branch specifies a zero weight, then that branch is
//     not taken", so F() always returns 10. F is never called in this file
//     (see randcase_1 for the run-time variant).
//   - Whether the case statement is marked as a randcase (vs a plain case):
//     IEEE 37.72 defines no randcase property, so it is not asserted.
//   - F's lifetime: 6.21 gives static as the default for subroutines in a
//     module/interface/package/program; compilation-unit scope is not listed
//     there, so no lifetime is asserted.
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/tf_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace {
// Top-level statements of a subroutine body (a multi-statement body may be wrapped in begin...end).
std::vector<const hldb::Any *> topStmts(const hldb::Any *body) {
  std::vector<const hldb::Any *> stmts;
  if (const hldb::Begin *const begin = any_cast<hldb::Begin>(body)) {
    if (begin->getStmts() != nullptr) {
      for (const hldb::Any *const stmt : *begin->getStmts()) stmts.emplace_back(stmt);
    }
  } else if (body != nullptr) {
    stmts.emplace_back(body);
  }
  return stmts;
}

// Collects, in source order, every statement of type T reachable from 'node' (nested blocks included).
template <typename T>
void collectStmts(const hldb::Any *node, std::vector<const T *> *out) {
  if (node == nullptr) return;
  if (const T *const match = any_cast<T>(node)) out->emplace_back(match);
  if (const hldb::Begin *const begin = any_cast<hldb::Begin>(node)) {
    if (const hldb::AnyCollection *const stmts = begin->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectStmts(stmt, out);
    }
  } else if (const hldb::CaseStmt *const cs = any_cast<hldb::CaseStmt>(node)) {
    if (const hldb::CaseItemCollection *const items = cs->getCaseItems()) {
      for (const hldb::CaseItem *const item : *items) collectStmts(item->getStmt(), out);
    }
  } else if (const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(node)) {
    collectStmts(ifElse->getStmt(), out);
    collectStmts(ifElse->getElseStmt(), out);
  } else if (const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(node)) {
    collectStmts(ifStmt->getStmt(), out);
  }
}

template <typename T>
std::vector<const T *> collectStmts(const hldb::Any *node) {
  std::vector<const T *> found;
  collectStmts(node, &found);
  return found;
}

hldb::AnyType typespecKind(const hldb::RefTypespec *ref) {
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

// Checks that 'expr' is a reference named like 'var' that binds to 'var'.
void expectRefTo(const hldb::Any *expr, const hldb::Variable *var) {
  ASSERT_NE(expr, nullptr);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
  ASSERT_NE(ref, nullptr) << "operand should be a reference to '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "reference '" << ref->getName() << "' should bind to the declared variable";
}

// Checks that 'item' is the randcase branch "<weight> : <var> = <value>;" with a literal weight.
void expectLiteralWeightBranch(const hldb::CaseItem *item, std::string_view weight, const hldb::Variable *var,
                               std::string_view value) {
  ASSERT_NE(item, nullptr);
  ASSERT_NE(item->getExprs(), nullptr);
  ASSERT_EQ(item->getExprs()->size(), 1u) << "a randcase item has exactly one weight expression (Syntax 18-12)";
  const hldb::Constant *const w = any_cast<hldb::Constant>(item->getExprs()->at(0));
  ASSERT_NE(w, nullptr) << "the weight is the literal " << weight;
  EXPECT_EQ(w->getDecompile(), weight);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(item->getStmt());
  ASSERT_NE(assign, nullptr) << "the branch statement is '" << var->getName() << " = " << value << ";'";
  expectRefTo(assign->getLhs(), var);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), value);
}
}  // namespace

namespace hlc {
class RandomWeightedCaseRandcase0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.16--random-weighted-case-randcase_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // A subroutine declared at compilation-unit scope (design level or the $unit package).
  const hldb::TaskFunc *findUnitSubroutine(std::string_view name) const {
    if (const hldb::TaskFunc *const tf = hldb::findByName<hldb::TaskFunc>(name, m_design->getTaskFuncs())) return tf;
    if (m_design->getAllPackages() != nullptr) {
      for (const hldb::Package *const pkg : *m_design->getAllPackages()) {
        if (const hldb::TaskFunc *const tf = hldb::findByName<hldb::TaskFunc>(name, pkg->getTaskFuncs())) return tf;
      }
    }
    return nullptr;
  }
  const hldb::Function *getF() const { return any_cast<hldb::Function>(findUnitSubroutine("F")); }
};

// Check: legal file.
TEST_F(RandomWeightedCaseRandcase0Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "randcase inside a function is legal (IEEE 1800-2023 Sec 18.16)";
}

// Check: compilation-unit function 'F' -- int return, no arguments, one local int 'a'.
TEST_F(RandomWeightedCaseRandcase0Test, FunctionFSignatureAndLocalVariable) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr) << "function 'F' declared at compilation-unit scope not found";
  EXPECT_FALSE(f->getMethod()) << "F is not a class method";
  EXPECT_EQ(typespecKind(f->getReturn()), hldb::AnyType::IntTypespec) << "F returns int";
  EXPECT_TRUE((f->getIODecls() == nullptr) || f->getIODecls()->empty()) << "F() takes no argument";
  ASSERT_NE(f->getVariables(), nullptr);
  ASSERT_EQ(f->getVariables()->size(), 1u) << "F declares exactly one local variable, 'a'";
  const hldb::Variable *const local = f->getVariables()->at(0);
  EXPECT_EQ(local->getName(), "a");
  EXPECT_EQ(typespecKind(local->getTypespec()), hldb::AnyType::IntTypespec) << "a is 'int'";
}

// Check: the function-local variable is not rand (37.17 details 15/22).
TEST_F(RandomWeightedCaseRandcase0Test, LocalVariableRandTypeIsNotRand) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  const hldb::Variable *const local = hldb::findByName<hldb::Variable>("a", f->getVariables());
  ASSERT_NE(local, nullptr) << "local variable 'a' not found";
  EXPECT_EQ(local->getRandType(), vpiNotRand)
      << "IEEE 1800-2023 Sec 37.17 details 15/22: vpiRandType is one of vpiRand, vpiRandC or vpiNotRand; "
         "a plain local 'a' is vpiNotRand";
}

// Check: F's body is "randcase ... ; return a;" -- two statements, in that order.
TEST_F(RandomWeightedCaseRandcase0Test, FunctionFBodyIsRandcaseThenReturn) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getVariables(), nullptr);
  const hldb::Variable *const local = hldb::findByName<hldb::Variable>("a", f->getVariables());
  ASSERT_NE(local, nullptr);
  const std::vector<const hldb::Any *> stmts = topStmts(f->getStmt());
  ASSERT_EQ(stmts.size(), 2u) << "F's body has two statements: the randcase and the return";
  ASSERT_NE(stmts[0], nullptr);
  EXPECT_EQ(stmts[0]->getAnyType(), hldb::AnyType::CaseStmt) << "the first statement is the randcase";
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[1]);
  ASSERT_NE(ret, nullptr) << "the last statement is 'return a;'";
  expectRefTo(ret->getCondition(), local);
}

// Check: "randcase 0 : a = 5; 1 : a = 10; endcase".
TEST_F(RandomWeightedCaseRandcase0Test, RandcaseHasZeroAndOneWeightBranches) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", f->getVariables());
  ASSERT_NE(a, nullptr);
  const std::vector<const hldb::CaseStmt *> cases = collectStmts<hldb::CaseStmt>(f->getStmt());
  ASSERT_EQ(cases.size(), 1u) << "F contains exactly one randcase";
  EXPECT_EQ(cases[0]->getCondition(), nullptr) << "randcase has no case expression (Syntax 18-12)";
  ASSERT_NE(cases[0]->getCaseItems(), nullptr);
  ASSERT_EQ(cases[0]->getCaseItems()->size(), 2u);
  expectLiteralWeightBranch(cases[0]->getCaseItems()->at(0), "0", a, "5");
  expectLiteralWeightBranch(cases[0]->getCaseItems()->at(1), "1", a, "10");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
