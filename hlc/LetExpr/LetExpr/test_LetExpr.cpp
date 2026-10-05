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

// Tests for dut.sv (tags: LetExpr), compiled with
// --disable-feature=letexprsubstitution so 'let' instances are kept as
// LetExpr nodes instead of being inlined.
//   module m(/*input clock*/);
//   logic a;
//   let p1(x) = $past(x);
//   let p2(x) = $past(x,,,@(posedge clock));
//   let s(x) = $sampled(x);
//   always_comb begin
//   a1: assert(p1(a));
//   a2: assert(p2(a));
//   a3: assert(s(a));
//   end
//   //a4: assert property(@(posedge clock) p1(a));
//   endmodule : m
//
// This is IEEE 1800-2023 11.12 example f) ("Sampled value functions in
// let"), with the 'clock' port commented out.
//
// What is checked (IEEE 1800-2023):
//   - 11.12: "It shall be an error if the clock is required, but cannot be
//     inferred in the instantiation context" -- the spec marks a1 as
//     "Illegal: no clock can be inferred" (always_comb gives no clock);
//     a2 carries an explicit clocking event and a3 uses $sampled, which
//     needs no clock (16.9.3), so neither is diagnosed
//   - 'clock' is never declared (the port is commented out): references in
//     p2's body are resolved from the let's declaration scope (11.12) and
//     must fail to bind
//   - module 'm' has exactly three LetDecls p1, p2, s, each with one formal
//     argument 'x', and each body is the expected system function call
//     whose first argument references the formal 'x'
//   - p2's body carries the explicit clocking event '@(posedge clock)' as
//     its last argument
//   - the always_comb (9.2.2.2) has a begin-end with three immediate
//     assertions (16.3) labeled a1, a2, a3
//   - with substitution disabled, each assertion expression is a LetExpr
//     bound to the corresponding LetDecl, with one argument referencing
//     variable 'a'
//
// What is NOT checked and why:
//   - the type HLC assigns to the untyped let formal 'x' (11.12 allows an
//     untyped formal; the object-model representation of "untyped" is not
//     specified by the standard).
//   - how the omitted 2nd/3rd arguments of '$past(x,,,@(...))' are
//     represented (empty slots vs. dropped) -- not specified by the
//     standard's object model.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/immediate_assert.h>
#include <hldb/let_decl.h>
#include <hldb/let_expr.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/seq_formal_decl.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LetExprTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LetExpr.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getM() { return hldb::findByDefName<hldb::Module>("m", m_design->getAllModules()); }

  static const hldb::LetDecl *findLet(std::string_view name) {
    const hldb::Module *const m = getM();
    if (m == nullptr || m->getLetDecls() == nullptr) return nullptr;
    for (const hldb::LetDecl *const l : *m->getLetDecls()) {
      if (l->getName() == name) return l;
    }
    return nullptr;
  }

  static const hldb::Begin *getAlwaysBody() {
    const hldb::Module *const m = getM();
    if (m == nullptr || m->getProcesses() == nullptr || m->getProcesses()->size() != 1) return nullptr;
    const hldb::Always *const al = any_cast<hldb::Always>(m->getProcesses()->at(0));
    if (al == nullptr) return nullptr;
    return al->getStmt<hldb::Begin>();
  }

  static const hldb::ImmediateAssert *findAssert(std::string_view label) {
    const hldb::Begin *const b = getAlwaysBody();
    if (b == nullptr || b->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const s : *b->getStmts()) {
      const hldb::ImmediateAssert *const ia = any_cast<hldb::ImmediateAssert>(s);
      if (ia != nullptr && ia->getLabel() == label) return ia;
    }
    return nullptr;
  }

  // 'let <name>(x) = <func>(x ...);'
  static void checkLet(std::string_view name, std::string_view func) {
    const hldb::LetDecl *const l = findLet(name);
    ASSERT_NE(l, nullptr) << "let '" << name << "' not found";
    ASSERT_NE(l->getSeqFormalDecls(), nullptr);
    ASSERT_EQ(l->getSeqFormalDecls()->size(), 1u);
    const hldb::SeqFormalDecl *const formal = l->getSeqFormalDecls()->at(0);
    EXPECT_EQ(formal->getName(), "x");
    const hldb::SysFuncCall *const call = l->getExpr<hldb::SysFuncCall>();
    ASSERT_NE(call, nullptr) << "let body must be a system function call";
    EXPECT_EQ(call->getName(), func);
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_FALSE(call->getArguments()->empty());
    const hldb::RefObj *const x = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->getName(), "x");
    EXPECT_EQ(x->getActual(), formal) << "body reference 'x' must bind to the let formal";
  }

  // 'aN: assert(<let>(a));' kept as a LetExpr
  static void checkAssertLetExpr(std::string_view label, std::string_view letName) {
    const hldb::ImmediateAssert *const ia = findAssert(label);
    ASSERT_NE(ia, nullptr) << "assertion '" << label << "' not found";
    const hldb::LetExpr *const le = ia->getExpr<hldb::LetExpr>();
    ASSERT_NE(le, nullptr) << "with letexprsubstitution disabled the expression must be a LetExpr";
    EXPECT_EQ(le->getLetDecl(), findLet(letName));
    ASSERT_NE(le->getArguments(), nullptr);
    ASSERT_EQ(le->getArguments()->size(), 1u);
    const hldb::RefObj *const a = any_cast<hldb::RefObj>(le->getArguments()->at(0));
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->getName(), "a");
    ASSERT_NE(a->getActual(), nullptr);
    EXPECT_EQ(a->getActual()->getAnyType(), hldb::AnyType::Variable);
  }
};

TEST_F(LetExprTest, SubstitutionIsDisabled) {
  ASSERT_NE(m_session, nullptr);
  EXPECT_FALSE(m_session->getLetExprSubstitution()) << "LetExpr.hlc passes --disable-feature=letexprsubstitution";
}

// ---------------------------------------------------------------------------
// Diagnostics -- 11.12, 16.9.3, 6.10
// ---------------------------------------------------------------------------

TEST_F(LetExprTest, A1NoInferredClockIsDiagnosed) {
  const bool atUse = findError(ErrorDefinition::COMP_NO_INFERRED_CLOCK, 10) != nullptr;
  const bool atBody = findError(ErrorDefinition::COMP_NO_INFERRED_CLOCK, 4) != nullptr;
  EXPECT_TRUE(atUse || atBody) << "11.12 example f): 'a1: assert(p1(a));' is illegal -- $past needs a clock and none "
                                  "can be inferred in always_comb";
}

TEST_F(LetExprTest, A2AndA3NeedNoInferredClock) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_NO_INFERRED_CLOCK, 11), nullptr) << "a2: clock given explicitly";
  EXPECT_EQ(findError(ErrorDefinition::COMP_NO_INFERRED_CLOCK, 6), nullptr) << "p2: clock given explicitly";
  EXPECT_EQ(findError(ErrorDefinition::COMP_NO_INFERRED_CLOCK, 12), nullptr) << "a3: $sampled uses no clock";
  EXPECT_EQ(findError(ErrorDefinition::COMP_NO_INFERRED_CLOCK, 7), nullptr) << "s: $sampled uses no clock";
}

TEST_F(LetExprTest, UndeclaredClockFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "clock", 6, 33), nullptr)
      << "'clock' is not declared (port commented out)";
}

// ---------------------------------------------------------------------------
// let declarations -- 11.12
// ---------------------------------------------------------------------------

TEST_F(LetExprTest, ModuleHasThreeLetDecls) {
  const hldb::Module *const m = getM();
  ASSERT_NE(m, nullptr) << "module 'm' not found";
  ASSERT_NE(m->getLetDecls(), nullptr);
  EXPECT_EQ(m->getLetDecls()->size(), 3u);
}

TEST_F(LetExprTest, P1IsPastOfX) { checkLet("p1", "$past"); }

TEST_F(LetExprTest, P2IsPastOfXWithExplicitClock) {
  checkLet("p2", "$past");
  const hldb::LetDecl *const l = findLet("p2");
  ASSERT_NE(l, nullptr);
  const hldb::SysFuncCall *const call = l->getExpr<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_GE(call->getArguments()->size(), 2u);
  const hldb::EventControl *const ev = any_cast<hldb::EventControl>(call->getArguments()->back());
  ASSERT_NE(ev, nullptr) << "last argument must be the clocking_event '@(posedge clock)'";
  const hldb::Operation *const pos = ev->getCondition<hldb::Operation>();
  ASSERT_NE(pos, nullptr);
  EXPECT_EQ(pos->getOpType(), vpiPosedgeOp);
  ASSERT_NE(pos->getOperands(), nullptr);
  ASSERT_EQ(pos->getOperands()->size(), 1u);
  const hldb::RefObj *const clk = any_cast<hldb::RefObj>(pos->getOperands()->at(0));
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getName(), "clock");
}

TEST_F(LetExprTest, SIsSampledOfX) {
  checkLet("s", "$sampled");
  const hldb::LetDecl *const l = findLet("s");
  ASSERT_NE(l, nullptr);
  const hldb::SysFuncCall *const call = l->getExpr<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 1u) << "16.9.3: $sampled takes exactly one argument";
}

// ---------------------------------------------------------------------------
// always_comb with immediate assertions -- 9.2.2.2, 16.3
// ---------------------------------------------------------------------------

TEST_F(LetExprTest, AlwaysCombHasThreeImmediateAsserts) {
  const hldb::Module *const m = getM();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_EQ(m->getProcesses()->size(), 1u);
  const hldb::Always *const al = any_cast<hldb::Always>(m->getProcesses()->at(0));
  ASSERT_NE(al, nullptr);
  EXPECT_EQ(al->getAlwaysType(), vpiAlwaysComb);
  const hldb::Begin *const b = getAlwaysBody();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  EXPECT_EQ(b->getStmts()->size(), 3u);
  for (std::string_view label : {"a1", "a2", "a3"}) {
    const hldb::ImmediateAssert *const ia = findAssert(label);
    ASSERT_NE(ia, nullptr) << label;
    EXPECT_FALSE(ia->getIsDeferred()) << label << ": simple immediate assertion";
  }
}

// ---------------------------------------------------------------------------
// LetExpr instances (substitution disabled)
// ---------------------------------------------------------------------------

TEST_F(LetExprTest, A1IsLetExprP1) { checkAssertLetExpr("a1", "p1"); }

TEST_F(LetExprTest, A2IsLetExprP2) { checkAssertLetExpr("a2", "p2"); }

TEST_F(LetExprTest, A3IsLetExprS) { checkAssertLetExpr("a3", "s"); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
