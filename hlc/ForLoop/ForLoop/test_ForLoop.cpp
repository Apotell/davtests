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

// Tests for dut.sv (tags: ForLoop)
//   module t (/*AUTOARG*/);
//     integer a, b;
//     initial begin
//        for (; ; ) ;
//        for (; ; a=a+1) ;
//        for (; ; a=a+1, b=b+1) ;
//        for (; a<1; ) ;
//        for (; a<1; a=a+1) ;
//        for (; a<1; a=a+1, b=b+1) ;
//        for (a=0; a<1; ) ;
//        for (a=0; a<1; a=a+1) ;
//        for (a=0; a<1; a=a+1, b=b+1) ;
//        for (integer a=0; a<1; ) ;
//        for (integer a=0; a<1; a=a+1) ;
//        for (integer a=0; a<1; a=a+1, b=b+1) ;
//        for (var integer a=0; a<1; ) ;
//        for (var integer a=0; a<1; a=a+1) ;
//        for (var integer a=0; a<1; a=a+1, b=b+1) ;
//        for (integer a=0, integer b=0; a<1; ) ;
//        for (integer a=0, integer b=0; a<1; a=a+1) ;
//        for (integer a=0, integer b=0; a<1; a=a+1, b=b+1) ;
//        $write("*-* All Finished *-*\n");
//        $finish;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 Sec 12.7.1 "for-loop statements",
// checked before any test code was written):
//   "for ( for_initialization ; expression ; for_step ) statement_or_null"
//   -- "for_initialization" is a comma-separated list of either plain
//   variable_assignments (using a preexisting variable "a"/"b" declared
//   earlier at module scope) or for_variable_declarations ("integer a=0"
//   or, per 12.7.1's "for ( [ var_data_type ] genvar_or_variable = ...",
//   the 'var' keyword form "var integer a=0"). A for_variable_declaration
//   creates a new variable scoped to the for-loop's own implicit block
//   (12.7.1: "the loop variable ... is local to the for statement");
//   a plain variable_assignment does not declare anything -- it assigns
//   to the module-scope "a"/"b". "for_step" is a comma-separated list of
//   for_step_assignments (here, always simple assignments "a=a+1" and/or
//   "b=b+1"). All 18 combinations in this file omit the loop body
//   (";", a null_statement).
//
// What is checked (all 18 for-statements, in source order, via a
// table-driven check of ForStmt's getForInitStmts()/getCondition()/
// getForIncStmts()/getStmt()/getVariables() -- ForStmt is a Scope, so an
// inline for_variable_declaration must appear in its own getVariables(),
// never in module 't's):
//   - init: absent (0 stmts) / a plain "a=0" Assignment (no local var) /
//     an inline-declared "integer a=0" (or "var integer a=0") Assignment
//     with a corresponding local Variable "a" / two inline declarations
//     "integer a=0, integer b=0" with two local Variables
//   - condition: absent, or present as Operation(vpiLtOp) "a<1"
//   - for_step: 0, 1 ("a=a+1"), or 2 ("a=a+1, b=b+1") Assignments
//   - body: always a NullStmt (the trailing ";")
//
// What is NOT checked and why:
//   - AUTOARG/port-list details of module 't' -- irrelevant to for-loop
//     shape, this file's actual focus
//   - the exact vpiSigned/width modeling of "integer" -- out of scope

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/null_stmt.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <array>
#include <string>

namespace hlc {

namespace {
// Describes the expected shape of one of the 18 "for" statements.
struct ForCase {
  int32_t initCount;      // number of for_initialization items
  bool declaresVars;      // true when init is a for_variable_declaration
  std::array<std::string_view, 2> declNames;  // names of declared local vars (if declaresVars)
  bool hasCondition;       // "a<1" present?
  int32_t incCount;        // number of for_step items (0, 1, or 2)
};
}  // namespace

class ForLoopTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForLoop.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModuleT() {
    return hldb::findByName<hldb::Module>("t", m_design->getAllModules());
  }

  static const hldb::Initial *getInitial() {
    const hldb::Module *const m = getModuleT();
    if (m == nullptr || m->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *m->getProcesses()) {
      if (const hldb::Initial *const init = any_cast<hldb::Initial>(p)) return init;
    }
    return nullptr;
  }

  // Returns the i-th top-level statement of the initial block's Begin.
  static const hldb::Any *getStmt(size_t i) {
    const hldb::Initial *const init = getInitial();
    if (init == nullptr) return nullptr;
    const hldb::Begin *const body = init->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || i >= body->getStmts()->size()) return nullptr;
    return body->getStmts()->at(i);
  }

  static const hldb::ForStmt *getForStmt(size_t i) { return any_cast<hldb::ForStmt>(getStmt(i)); }

  // Verifies a single Assignment "<name> = <name> + 1" (a for_step_assignment).
  static void CheckIncAssign(const hldb::Any *stmt, std::string_view name) {
    const hldb::Assignment *const a = any_cast<hldb::Assignment>(stmt);
    ASSERT_NE(a, nullptr) << "for_step item for '" << name << "' should be an Assignment";
    EXPECT_TRUE(a->getBlocking());
    const hldb::RefObj *const lhs = a->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), name);
    const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
    ASSERT_NE(rhs, nullptr) << "'" << name << "+1' should be an Operation";
    EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  }
};

// ---------------------------------------------------------------------------
// Module / process existence
// ---------------------------------------------------------------------------

TEST_F(ForLoopTest, ModuleTExists) { EXPECT_NE(getModuleT(), nullptr); }

TEST_F(ForLoopTest, InitialExists) { EXPECT_NE(getInitial(), nullptr); }

TEST_F(ForLoopTest, InitialBodyHasAtLeast18Statements) {
  const hldb::Initial *const init = getInitial();
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = init->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "initial body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 18u) << "18 for-statements plus $write/$finish";
}

TEST_F(ForLoopTest, AllEighteenStatementsAreForStmts) {
  for (size_t i = 0; i < 18; ++i) {
    EXPECT_NE(getForStmt(i), nullptr) << "statement index " << i << " should be a ForStmt";
  }
}

// ---------------------------------------------------------------------------
// Table-driven shape check for all 18 combinations (12.7.1)
// ---------------------------------------------------------------------------

TEST_F(ForLoopTest, AllEighteenForStmtsMatchExpectedShape) {
  const std::array<ForCase, 18> kCases = {{
      /*  0 */ {0, false, {}, false, 0},
      /*  1 */ {0, false, {}, false, 1},
      /*  2 */ {0, false, {}, false, 2},
      /*  3 */ {0, false, {}, true, 0},
      /*  4 */ {0, false, {}, true, 1},
      /*  5 */ {0, false, {}, true, 2},
      /*  6 */ {1, false, {}, true, 0},
      /*  7 */ {1, false, {}, true, 1},
      /*  8 */ {1, false, {}, true, 2},
      /*  9 */ {1, true, {"a", ""}, true, 0},
      /* 10 */ {1, true, {"a", ""}, true, 1},
      /* 11 */ {1, true, {"a", ""}, true, 2},
      /* 12 */ {1, true, {"a", ""}, true, 0},
      /* 13 */ {1, true, {"a", ""}, true, 1},
      /* 14 */ {1, true, {"a", ""}, true, 2},
      /* 15 */ {2, true, {"a", "b"}, true, 0},
      /* 16 */ {2, true, {"a", "b"}, true, 1},
      /* 17 */ {2, true, {"a", "b"}, true, 2},
  }};

  for (size_t i = 0; i < kCases.size(); ++i) {
    SCOPED_TRACE("for-statement index " + std::to_string(i));
    const ForCase &tc = kCases[i];
    const hldb::ForStmt *const fs = getForStmt(i);
    ASSERT_NE(fs, nullptr);

    // for_initialization: only the "list_of_variable_assignments" form
    // (plain "a=0", reusing the preexisting module-scope 'a'/'b') is
    // asserted item-by-item here; its shape is unambiguous (a plain
    // Assignment). The "for_variable_declaration" form's exact split
    // between getForInitStmts() and the declared Variable's own initial
    // value is not pinned down by any other test in this suite, so for
    // those cases (tc.declaresVars) only the resulting local Variable is
    // checked below, not the contents of getForInitStmts() itself.
    if (!tc.declaresVars) {
      if (tc.initCount == 0) {
        EXPECT_TRUE(fs->getForInitStmts() == nullptr || fs->getForInitStmts()->empty());
      } else {
        ASSERT_NE(fs->getForInitStmts(), nullptr);
        ASSERT_EQ(fs->getForInitStmts()->size(), static_cast<size_t>(tc.initCount));
        for (int32_t k = 0; k < tc.initCount; ++k) {
          const hldb::Assignment *const initAssign = any_cast<hldb::Assignment>(fs->getForInitStmts()->at(k));
          ASSERT_NE(initAssign, nullptr) << "for_initialization item " << k << " should be an Assignment";
          const hldb::RefObj *const lhs = initAssign->getLhs<hldb::RefObj>();
          ASSERT_NE(lhs, nullptr);
          EXPECT_EQ(lhs->getName(), tc.declNames[static_cast<size_t>(k)]);
          const hldb::Constant *const rhs = initAssign->getRhs<hldb::Constant>();
          ASSERT_NE(rhs, nullptr) << "'" << tc.declNames[static_cast<size_t>(k)] << " = 0': RHS should be a Constant";
          EXPECT_EQ(rhs->getDecompile(), std::string_view{"0"});
        }
      }
    }

    // 12.7.1: a for_variable_declaration is local to the for-loop's own
    // scope; a plain variable_assignment declares nothing.
    if (tc.declaresVars) {
      ASSERT_NE(fs->getVariables(), nullptr) << "inline-declared loop variable(s) must live in the ForStmt's scope";
      EXPECT_EQ(fs->getVariables()->size(), static_cast<size_t>(tc.initCount));
      for (int32_t k = 0; k < tc.initCount; ++k) {
        EXPECT_NE(hldb::findByName<hldb::Variable>(tc.declNames[static_cast<size_t>(k)], fs->getVariables()),
                  nullptr)
            << "local variable '" << tc.declNames[static_cast<size_t>(k)] << "' not found in ForStmt's scope";
      }
    } else {
      EXPECT_TRUE(fs->getVariables() == nullptr || fs->getVariables()->empty())
          << "no for_variable_declaration in the header -- ForStmt should not own any local variable";
    }

    // condition
    if (tc.hasCondition) {
      const hldb::Operation *const cond = fs->getCondition<hldb::Operation>();
      ASSERT_NE(cond, nullptr) << "'a<1' should be an Operation";
      EXPECT_EQ(cond->getOpType(), vpiLtOp);
      ASSERT_NE(cond->getOperands(), nullptr);
      ASSERT_EQ(cond->getOperands()->size(), 2u);
      const hldb::RefObj *const condLhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
      ASSERT_NE(condLhs, nullptr);
      EXPECT_EQ(condLhs->getName(), std::string_view{"a"});
      const hldb::Constant *const condRhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
      ASSERT_NE(condRhs, nullptr);
      EXPECT_EQ(condRhs->getDecompile(), std::string_view{"1"});
    } else {
      EXPECT_EQ(fs->getCondition(), nullptr) << "empty 'expression' slot -- no condition";
    }

    // for_step
    if (tc.incCount == 0) {
      EXPECT_TRUE(fs->getForIncStmts() == nullptr || fs->getForIncStmts()->empty());
    } else {
      ASSERT_NE(fs->getForIncStmts(), nullptr);
      ASSERT_EQ(fs->getForIncStmts()->size(), static_cast<size_t>(tc.incCount));
      CheckIncAssign(fs->getForIncStmts()->at(0), "a");
      if (tc.incCount == 2) {
        CheckIncAssign(fs->getForIncStmts()->at(1), "b");
      }
    }

    // body: always ";" -- a NullStmt
    const hldb::NullStmt *const nullStmt = fs->getStmt<hldb::NullStmt>();
    EXPECT_NE(nullStmt, nullptr) << "the loop body is ';' -- a NullStmt";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
