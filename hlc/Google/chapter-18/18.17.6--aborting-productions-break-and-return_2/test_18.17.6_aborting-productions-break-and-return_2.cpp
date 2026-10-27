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
// SV source under test: tests/Google/chapter-18/18.17.6--aborting-productions-break-and-return_2.sv
// (IEEE 1800-2023 18.17 randsequence, 18.17.6 "Aborting productions--break and return")
//
//   /*
//   :name: aborting_productions_break_and_return_2
//   :description: return statement test
//   :tags: 18.17.6
//   */
//
//   function int F();
//       int x;
//       int return_on = 1;
//       randsequence( main )
//         main : first secondi third;
//         first : { x = x + 20; };
//         second : { if(return_on == 1) return; x = x + 10; };
//         third : { x = x + 5;};
//       endsequence
//       return x;
//   endfunction
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is ILLEGAL (even though it has no :should_fail_because:
//      tag): 'main : first secondi third;' uses a production 'secondi', but
//      no production of that name exists (only 'second' is defined). An
//      rs_production_item names a production of the randsequence (Syntax
//      18-13), so the compiler shall report an error.
//      - coarse check: at least one fatal/syntax/error diagnostic exists.
//   2. Function 'F' at compilation-unit scope: not a class method, returns
//      int, no argument, and its only variables are the locals 'int x', 'int return_on = 1'.
//   3. The locals are not rand: vpiRandType == vpiNotRand (37.17 details
//      15/22: the value is one of vpiRand/vpiRandC/vpiNotRand).
//   4. F's body is exactly two statements in order: the randsequence
//      statement, then 'return x;' bound to the local x. The production code
//      blocks must NOT appear as plain statements of F.
//   5. 18.17: "All production identifiers are local to the scope" of the
//      randsequence -- main, first, second, third are not variables of F and not
//      compilation-unit subroutines.
//
// CHECKED (TEST_F below):
//   - items 1 to 5.
//
// NOT CHECKED (and why):
//   - The productions themselves (names, alternatives '|', weights ':=',
//     code blocks, case/repeat/rand join controls): IEEE 1800-2023 Clause 37 defines
//     NO VPI object for randsequence, and the hldb model has none either, so
//     there is no standard API to assert them against. Only what is
//     observable around the statement is checked (it is kept as exactly one
//     statement, its code blocks are not flattened into the enclosing body,
//     and production names do not leak into the enclosing scope).
//   - No dedicated diagnostic check for item 1: there is no ErrorDefinition
//     entry specific to an undefined production, so only the coarse error
//     check covers it.
//   - Note: 'return;' inside a production code block aborts only the current
//     production (18.17.6), not F -- not observable without a randsequence
//     object.
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
class AbortingProductionsBreakAndReturn2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.17.6--aborting-productions-break-and-return_2.hlc"}); }
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

// Check: production 'secondi' is used but never defined, so the file is illegal.
TEST_F(AbortingProductionsBreakAndReturn2Test, CompilerRejectsUndefinedProductionSecondi) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "main uses production 'secondi', but only 'second' is defined: an rs_production_item names a "
         "production of the randsequence (IEEE 1800-2023 Sec 18.17, Syntax 18-13)";
}

// Check: compilation-unit function 'F' -- int return, no arguments, locals x, return_on.
TEST_F(AbortingProductionsBreakAndReturn2Test, FunctionFSignatureAndLocalVariables) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr) << "function 'F' declared at compilation-unit scope not found";
  EXPECT_FALSE(f->getMethod()) << "F is not a class method";
  EXPECT_EQ(typespecKind(f->getReturn()), hldb::AnyType::IntTypespec) << "F returns int";
  EXPECT_TRUE((f->getIODecls() == nullptr) || f->getIODecls()->empty()) << "F() takes no argument";
  ASSERT_NE(f->getVariables(), nullptr);
  ASSERT_EQ(f->getVariables()->size(), 2u) << "F's only variables are its locals: x, return_on";
  {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>("x", f->getVariables());
    ASSERT_NE(var, nullptr) << "local 'x' not found";
    EXPECT_EQ(typespecKind(var->getTypespec()), hldb::AnyType::IntTypespec) << "x is 'int'";
  }
  {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>("return_on", f->getVariables());
    ASSERT_NE(var, nullptr) << "local 'return_on' not found";
    EXPECT_EQ(typespecKind(var->getTypespec()), hldb::AnyType::IntTypespec) << "return_on is 'int'";
    const hldb::Any *const init = (var->getExpr() != nullptr) ? var->getExpr() : var->getValue();
    const hldb::Constant *const value = any_cast<hldb::Constant>(init);
    ASSERT_NE(value, nullptr) << "return_on is initialized to the literal 1";
    EXPECT_EQ(value->getDecompile(), "1");
  }
}

// Check: the function-local variables are not rand (37.17 details 15/22).
TEST_F(AbortingProductionsBreakAndReturn2Test, LocalVariablesRandTypeIsNotRand) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  for (std::string_view name : {"x", "return_on"}) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, f->getVariables());
    ASSERT_NE(var, nullptr) << "local '" << name << "' not found";
    EXPECT_EQ(var->getRandType(), vpiNotRand)
        << "IEEE 1800-2023 Sec 37.17 details 15/22: vpiRandType is one of vpiRand, vpiRandC or vpiNotRand; "
           "the plain local '"
        << name << "' is vpiNotRand";
  }
}

// Check: F's body is "randsequence ... ; return x;" -- two statements, in that order.
TEST_F(AbortingProductionsBreakAndReturn2Test, FunctionFBodyIsRandsequenceThenReturn) {
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
TEST_F(AbortingProductionsBreakAndReturn2Test, ProductionNamesAreLocalToTheRandsequence) {
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
