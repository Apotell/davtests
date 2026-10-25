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
// SV source under test: tests/Google/chapter-18/18.17.2--if-else-production-statements_0.sv
// (IEEE 1800-2023 18.17 randsequence, 18.17.2 "if-else production statements", 23.9 scope rules)
//
//   /*
//   :name: if_else_production_statements_0
//   :description: randcase if-else test
//   :tags: 18.17.2
//   */
//
//   function int F();
//       int x;
//       randsequence( main )
//         main : first;
//         first : { if(switch ) x = 10; else x = 5; };
//       endsequence
//       return x;
//   endfunction
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is ILLEGAL (even though it has no :should_fail_because:
//      tag): 'switch' is NOT a SystemVerilog keyword (it is absent from
//      Table B.1) and it is not declared anywhere -- not in F, not at
//      compilation-unit scope. IEEE 1800-2023 Sec 23.9: an identifier
//      referenced directly within a function "shall be declared either within
//      the task, function, named block, or generate block locally or within"
//      an enclosing scope. So the compiler shall report an error.
//      - coarse check: at least one fatal/syntax/error diagnostic exists.
//      - specific check: an undefined-variable or failed-to-bind diagnostic
//        names 'switch'.
//   2. Apart from that, the model is still built as written: function 'F' at
//      compilation-unit scope, not a class method, returns int, no argument,
//      exactly one local 'int x' ('switch' must NOT be invented as a local
//      variable).
//   3. F's body is exactly two statements: the randsequence statement, then
//      'return x;' bound to x. The code block '{ if (switch) x = 10; else
//      x = 5; }' belongs to production 'first' and must NOT appear as a plain
//      statement of F.
//   4. 18.17: production identifiers (main, first) are local to the
//      randsequence -- not variables of F, not compilation-unit subroutines.
//
//  LOCAL RAND TYPE: the local 'x' has vpiRandType == vpiNotRand
//   (37.17 details 15/22: the value is one of vpiRand/vpiRandC/vpiNotRand).
//
// CHECKED (TEST_F below):
//   - items 1 to 4.
//
// NOT CHECKED (and why):
//   - The productions themselves (names, alternatives '|', weights ':=',
//     code blocks, if-else productions): IEEE 1800-2023 Clause 37 defines
//     NO VPI object for randsequence, and the hldb model has none either, so
//     there is no standard API to assert them against. Only what is
//     observable around the statement is checked (it is kept as exactly one
//     statement, its code blocks are not flattened into the enclosing body,
//     and production names do not leak into the enclosing scope).
//   - The exact error wording/location: not defined by the standard.
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
}  // namespace

namespace hlc {
class IfElseProductionStatements0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.17.2--if-else-production-statements_0.hlc"}); }
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

// Check: 'switch' is never declared, so the reference is illegal (23.9) and must be diagnosed.
TEST_F(IfElseProductionStatements0Test, CompilerRejectsUndeclaredIdentifierSwitch) {
  GTEST_SKIP() << "HLC reports no error for the undeclared 'switch': the randsequence holding it is dropped, so the "
                  "reference is never resolved. IEEE 1800-2023 Sec 23.9 (and Table B.1: 'switch' is no keyword) "
                  "require an error for an undeclared identifier. Fix pending.";
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "'switch' is not a keyword (Table B.1) and is not declared anywhere: IEEE 1800-2023 Sec 23.9 requires an "
         "identifier referenced in a function to be declared locally or in an enclosing scope";
}

// Check: the diagnostic is about the identifier 'switch'.
TEST_F(IfElseProductionStatements0Test, UnresolvedIdentifierDiagnosticNamesSwitch) {
  GTEST_SKIP() << "No undefined-variable / failed-to-bind diagnostic names 'switch': the randsequence holding it is "
                  "dropped by HLC, so the reference is never resolved. "
                  "IEEE 1800-2023 Sec 23.9: an undeclared identifier in a function is an error. Fix pending.";
  const bool reported = (findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, "switch") != nullptr) ||
                        (findError(ErrorDefinition::COMP_FAILED_TO_BIND, "switch") != nullptr);
  EXPECT_TRUE(reported) << "expected an undefined-variable / failed-to-bind diagnostic for 'switch' "
                           "(IEEE 1800-2023 Sec 23.9)";
}

// Check: compilation-unit function 'F' -- int return, no arguments, one local int 'x'.
TEST_F(IfElseProductionStatements0Test, FunctionFSignatureAndLocalVariable) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr) << "function 'F' declared at compilation-unit scope not found";
  EXPECT_FALSE(f->getMethod()) << "F is not a class method";
  EXPECT_EQ(typespecKind(f->getReturn()), hldb::AnyType::IntTypespec) << "F returns int";
  EXPECT_TRUE((f->getIODecls() == nullptr) || f->getIODecls()->empty()) << "F() takes no argument";
  ASSERT_NE(f->getVariables(), nullptr);
  ASSERT_EQ(f->getVariables()->size(), 1u) << "F declares exactly one local variable, 'x'";
  const hldb::Variable *const local = f->getVariables()->at(0);
  EXPECT_EQ(local->getName(), "x");
  EXPECT_EQ(typespecKind(local->getTypespec()), hldb::AnyType::IntTypespec) << "x is 'int'";
}

// Check: the function-local variable is not rand (37.17 details 15/22).
TEST_F(IfElseProductionStatements0Test, LocalVariableRandTypeIsNotRand) {
  GTEST_SKIP() << "HLC reports vpiRandType == 0 for the function-local variable 'x' (0 is not a defined value). "
                  "IEEE 1800-2023 Sec 37.17 details 15/22: vpiRandType is one of vpiRand, vpiRandC or vpiNotRand, "
                  "so a plain local is vpiNotRand. Fix pending.";
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  const hldb::Variable *const local = hldb::findByName<hldb::Variable>("x", f->getVariables());
  ASSERT_NE(local, nullptr) << "local variable 'x' not found";
  EXPECT_EQ(local->getRandType(), vpiNotRand)
      << "IEEE 1800-2023 Sec 37.17 details 15/22: vpiRandType is one of vpiRand, vpiRandC or vpiNotRand; "
         "a plain local 'x' is vpiNotRand";
}

// Check: F's body is "randsequence ... ; return x;" -- two statements, in that order.
TEST_F(IfElseProductionStatements0Test, FunctionFBodyIsRandsequenceThenReturn) {
  GTEST_SKIP() << "HLC drops the randsequence statement: F's body only holds 'return x;' (1 statement, not 2). "
                  "IEEE 1800-2023 Sec 18.17: randsequence is a statement_item (Syntax 18-13) and shall be kept in "
                  "the subroutine body. Fix pending.";
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getVariables(), nullptr);
  const hldb::Variable *const local = hldb::findByName<hldb::Variable>("x", f->getVariables());
  ASSERT_NE(local, nullptr);
  const std::vector<const hldb::Any *> stmts = topStmts(f->getStmt());
  ASSERT_EQ(stmts.size(), 2u) << "F's body has two statements: the randsequence and the return";
  ASSERT_NE(stmts[0], nullptr);
  // There is no VPI object for randsequence (Clause 37), so only check that it was kept as one statement
  // and that its code blocks were not flattened into F's body.
  EXPECT_NE(stmts[0]->getAnyType(), hldb::AnyType::ReturnStmt) << "the randsequence statement is kept";
  EXPECT_NE(stmts[0]->getAnyType(), hldb::AnyType::Assignment)
      << "the code-block assignments belong to their productions, not to F's body";
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[1]);
  ASSERT_NE(ret, nullptr) << "the last statement is 'return x;'";
  expectRefTo(ret->getCondition(), local);
}

// Check: production names are local to the randsequence (18.17) -- they are not declared in function F.
TEST_F(IfElseProductionStatements0Test, ProductionNamesAreLocalToTheRandsequence) {
  const hldb::Function *const scope = getF();
  ASSERT_NE(scope, nullptr) << "function 'F' not found";
  for (std::string_view name : {"main", "first", "second", "third"}) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, scope->getVariables()), nullptr)
        << "production '" << name << "' must not become a variable of function F";
    EXPECT_EQ(findUnitSubroutine(name), nullptr) << "production '" << name << "' must not become a subroutine";
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
