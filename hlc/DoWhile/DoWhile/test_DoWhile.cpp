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

// Tests for dut.sv (tags: DoWhile)
//   package toto;
//     function void uvm_default_factory::print ();
//       do begin
//         wait (m_sync[i].m_state >= UVM_PHASE_SYNCING);
//         qs.push_back("  Type Name\n");
//       end while(m_type_names.next(key));
//     endfunction
//   endpackage
//
// This file is a deliberately malformed fragment (checked before any test
// code was written, IEEE 1800-2023):
//   - 8.24 "Out-of-block declarations": "function void
//     uvm_default_factory::print ()" defines a class method body outside
//     its class ("class_scope method_name"), but "uvm_default_factory" is
//     never declared as a class anywhere in this file/package -- the
//     class_scope cannot resolve.
//   - 6.3/23.6: "m_sync", "i", "UVM_PHASE_SYNCING", "qs", "m_type_names",
//     and "key" are all used without ever being declared, in this package
//     or an import; every one of these must fail to bind.
//   - 13.4(a): "A function shall not have any nonblocking assignments,
//     event controls, or a delay control of any kind" -- the "wait (...)"
//     inside "print"'s body is an illegal timing-control statement in a
//     function.
//   - 12.7.5: despite all of the above being illegal, the do...while
//     LOOP ITSELF is syntactically well-formed ("do statement_or_null
//     while ( expression )"), and this is the one construct this file is
//     actually exercising: confirming the do-while shape (DoWhile object,
//     condition tested via the trailing "while(...)", body is the
//     preceding "begin...end") is still built even though the enclosing
//     function/class-scope/identifiers around it are all malformed.
//
// What is checked:
//   - package 'toto' exists with exactly 1 TaskFunc: Function "print"
//   - the illegal "wait" inside a function is diagnosed:
//     COMP_ILLEGAL_TIMING_CONTROL_IN_FUNCTION at 7:5 (13.4(a))
//   - the undeclared identifiers fail to bind: COMP_FAILED_TO_BIND for
//     "m_state", "qs", "m_type_names", "key" (6.3)
//   - Function "print"'s body (its single function_statement_or_null) is
//     directly a DoWhile (AnyType::DoWhile) -- no enclosing Begin needed,
//     since the do-while is the function's only statement
//   - the do-while's body ("begin ... end") is an explicit Begin (source
//     has begin-end) with exactly 2 statements; the first is a WaitStmt
//     whose condition is Operation(vpiGeOp) comparing a path expression
//     rooted at "m_sync" against "UVM_PHASE_SYNCING"
//   - the do-while's own condition ("m_type_names.next(key)") is present
//     (non-null) as the trailing "while(...)" test, evaluated after the
//     body per 12.7.5 -- same field layout as 12.7.5--dowhile.sv
//
// What is NOT checked and why:
//   - the exact HLDB shape recovered for "m_type_names.next(key)" (the
//     do-while condition) and for "qs.push_back(...)" (the second body
//     statement) beyond bare existence: both identifiers fail to bind, so
//     whatever node HLC builds for them is error-recovery behavior for
//     malformed input, not something IEEE 1800 specifies a "correct"
//     shape for. (Narrow exception, per this repo's test-writing guide:
//     hlc/DoWhile/DoWhile/DoWhile.log was read only to confirm this
//     specific recovered shape exists at all -- e.g. that the condition
//     resolves to a RefObj with 2 path elements rather than being dropped
//     entirely -- not to derive value-based expectations from it.)
//   - whether "uvm_default_factory::print" should itself produce a
//     dedicated "class not found" diagnostic for the unresolved
//     class_scope (8.24): HLC currently attaches an UnsupportedTypespec
//     for "uvm_default_factory" with no error of its own. This looks like
//     a possible gap, but it is a class_scope/out-of-block-method concern
//     orthogonal to the do-while construct under test here, so it is
//     flagged in this comment rather than asserted on with GTEST_SKIP.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/do_while.h>
#include <hldb/function.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>
#include <hldb/wait_stmt.h>

namespace hlc {

class DoWhileTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DoWhile.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getToto() {
    return hldb::findByName<hldb::Package>("toto", m_design->getAllPackages());
  }

  static const hldb::Function *getPrint() {
    const hldb::Package *const toto = getToto();
    if (toto == nullptr || toto->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("print", toto->getTaskFuncs());
  }

  static const hldb::DoWhile *getDoWhile() {
    const hldb::Function *const print = getPrint();
    if (print == nullptr) return nullptr;
    return print->getStmt<hldb::DoWhile>();
  }
};

// ===========================================================================
// package toto / function print
// ===========================================================================

TEST_F(DoWhileTest, PackageTotoExists) { EXPECT_NE(getToto(), nullptr); }

TEST_F(DoWhileTest, PackageTotoHasExactlyOneTaskFuncPrint) {
  const hldb::Package *const toto = getToto();
  ASSERT_NE(toto, nullptr);
  ASSERT_NE(toto->getTaskFuncs(), nullptr);
  EXPECT_EQ(toto->getTaskFuncs()->size(), 1u);
  const hldb::Function *const print = getPrint();
  ASSERT_NE(print, nullptr);
  EXPECT_EQ(print->getName(), "print");
}

// ===========================================================================
// 13.4(a): "wait (...)" is illegal timing control inside a function
// ===========================================================================

TEST_F(DoWhileTest, WaitInsideFunctionIsIllegalTimingControl) {
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_TIMING_CONTROL_IN_FUNCTION, "print", 7, 5), nullptr)
      << "13.4(a): a function shall not contain a timing-control statement such as 'wait'";
}

// ===========================================================================
// 6.3: undeclared identifiers must fail to bind
// ===========================================================================

TEST_F(DoWhileTest, UndeclaredIdentifiersFailToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "m_state"), nullptr) << "'m_state' is never declared";
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "qs"), nullptr) << "'qs' is never declared";
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "push_back"), nullptr) << "'push_back' is never declared";
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "m_type_names"), nullptr)
      << "'m_type_names' is never declared";
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "key"), nullptr) << "'key' is never declared";
}

// ===========================================================================
// print's body is directly a DoWhile (12.7.5): no enclosing Begin, since
// the do-while is the function's only statement
// ===========================================================================

TEST_F(DoWhileTest, PrintBodyIsDirectlyDoWhile) {
  const hldb::DoWhile *const dw = getDoWhile();
  ASSERT_NE(dw, nullptr) << "'print's single function_statement_or_null should resolve to DoWhile";
  EXPECT_EQ(dw->getAnyType(), hldb::AnyType::DoWhile);
}

// ---------------------------------------------------------------------------
// do begin
//   wait (m_sync[i].m_state >= UVM_PHASE_SYNCING);
//   qs.push_back("  Type Name\n");
// end while(...)
// ---------------------------------------------------------------------------
TEST_F(DoWhileTest, BodyIsBeginWithWaitFirst) {
  const hldb::DoWhile *const dw = getDoWhile();
  ASSERT_NE(dw, nullptr);
  const hldb::Begin *const body = dw->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "do-while body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::WaitStmt *const wait = any_cast<hldb::WaitStmt>(body->getStmts()->at(0));
  ASSERT_NE(wait, nullptr) << "'wait (m_sync[i].m_state >= UVM_PHASE_SYNCING);' should be a WaitStmt";
  const hldb::Operation *const cond = wait->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiGeOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "m_sync[i].m_state");
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "UVM_PHASE_SYNCING");

  // stmts[1] ("qs.push_back(...)") exists but its exact recovered shape is
  // error-recovery for the undeclared 'qs' -- see file-level comment above.
  EXPECT_NE(body->getStmts()->at(1), nullptr);
}

// ---------------------------------------------------------------------------
// 12.7.5: the do-while's own condition ("while(m_type_names.next(key))")
// is evaluated after the body, but the field itself must still be present
// ---------------------------------------------------------------------------
TEST_F(DoWhileTest, ConditionIsPresent) {
  const hldb::DoWhile *const dw = getDoWhile();
  ASSERT_NE(dw, nullptr);
  EXPECT_NE(dw->getCondition(), nullptr) << "12.7.5: the do-while's trailing 'while(...)' test must be captured";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
