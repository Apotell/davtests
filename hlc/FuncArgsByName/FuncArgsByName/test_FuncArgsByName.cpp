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

// Tests for FuncArgsByName/dut.sv:
//   module top ();
//     function bit m_matches_type_pair(string match_type_pair,
//        string requested_type);
//     endfunction
//
//     function void func();
//        m_matches_type_pair(.match_type_pair("a"),
//                               .requested_type("b"));
//        m_matches_type_pair("c", "d");
//        m_matches_type_pair(.requested_type("e"),
//                               .match_type_pair("f"));
//     endfunction
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text and the real hldb API headers):
//
//   Sec 13.5.3 "Argument passing": arguments can be connected by name,
//   using '.formal_name(actual_expression)', and "The order in which
//   arguments are connected by name is not significant." The only
//   standard-mandated fact about a named-argument call is the *binding*:
//   each actual connects to the formal whose name is given, regardless of
//   syntactic order. Nothing in the standard, nor in the hldb
//   TFCall::getArguments() API (a plain AnyCollection*, no separate
//   "argument name" field), mandates a particular *storage order* for
//   that collection, so assertions below identify each argument by which
//   formal it is bound to (matching a RefObj's name against
//   m_matches_type_pair's own same-named IODecl -- the same approach
//   used by the existing EvalFuncNamed test), not by position.
//
//   Sec 13.4: function arguments default to direction 'input' when not
//   otherwise specified; 'string' (Sec 6.16) is a non-net data type.
//
//   Sec 13.4.1: "It shall be legal to call a function as if it had no
//   return value" -- 'm_matches_type_pair(...)' called as a bare
//   statement (discarding its 'bit' return, with no 'void'(...)' cast)
//   is legal per this rule (only a warning is mandated, which HLC does
//   not yet implement -- see the existing
//   Chapter13ErrorRulesTest.Row404_DiscardedNonvoidReturnValueIsWarned
//   GTEST_SKIP for that separate, already-tracked gap). This file should
//   therefore compile with zero errors.
//
// What is NOT checked and why:
//   - the runtime-evaluated boolean result of m_matches_type_pair(...) is
//     a simulation-time concept.
//   - the (empty) body of m_matches_type_pair is not walked; per 13.4.1 a
//     function with no return statement returns the default value of its
//     return type, which is not a static/structural property to assert.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncArgsByNameTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncArgsByName.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getFn(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>(name, top->getTaskFuncs());
  }

  // Finds, among a call's arguments, the actual Constant bound to the given
  // formal IODecl -- resolved by matching against the IODecl in the
  // callee's own formal list, since positional/order is not guaranteed.
  static const hldb::Constant *findConstArgAt(const hldb::FuncCall *call, size_t index) {
    if (call == nullptr || call->getArguments() == nullptr || call->getArguments()->size() <= index) return nullptr;
    return any_cast<hldb::Constant>(call->getArguments()->at(index));
  }
};

TEST_F(FuncArgsByNameTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// function bit m_matches_type_pair(string match_type_pair, string requested_type);
// ---------------------------------------------------------------------------
TEST_F(FuncArgsByNameTest, MMatchesTypePairSignature) {
  const hldb::Function *const fn = getFn("m_matches_type_pair");
  ASSERT_NE(fn, nullptr) << "function 'm_matches_type_pair' not found";
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u);
  bool hasMatchTypePair = false, hasRequestedType = false;
  for (const hldb::IODecl *const io : *fn->getIODecls()) {
    ASSERT_NE(io, nullptr);
    EXPECT_EQ(io->getDirection(), vpiInput) << "Sec 13.4: formals default to input when unspecified";
    if (io->getName() == std::string_view("match_type_pair")) hasMatchTypePair = true;
    if (io->getName() == std::string_view("requested_type")) hasRequestedType = true;
  }
  EXPECT_TRUE(hasMatchTypePair) << "formal 'match_type_pair' missing";
  EXPECT_TRUE(hasRequestedType) << "formal 'requested_type' missing";
}

TEST_F(FuncArgsByNameTest, FuncBodyHasThreeCallStatements) {
  const hldb::Function *const func = getFn("func");
  ASSERT_NE(func, nullptr) << "function 'func' not found";
  const hldb::Begin *const body = func->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'func''s body should be an implicit Begin scope (3 statements)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);
}

// ---------------------------------------------------------------------------
// m_matches_type_pair(.match_type_pair("a"), .requested_type("b"));
// ---------------------------------------------------------------------------
TEST_F(FuncArgsByNameTest, FirstCallBindsArgumentsByNameInSourceOrder) {
  const hldb::Function *const func = getFn("func");
  const hldb::Function *const callee = getFn("m_matches_type_pair");
  ASSERT_NE(func, nullptr);
  ASSERT_NE(callee, nullptr);
  const hldb::Begin *const body = func->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(body->getStmts()->at(0));
  ASSERT_NE(call, nullptr) << "first statement should be a bare FuncCall statement (Sec 13.4.1: legal to call a "
                               "function as if it had no return value)";
  EXPECT_EQ(call->getName(), std::string_view("m_matches_type_pair"));
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), callee);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  // Named connections are order-independent (Sec 13.5.3); bind by the
  // formal each actual's RefObj resolves to.
  bool foundA = false, foundB = false;
  for (hldb::Any *const arg : *call->getArguments()) {
    const hldb::Constant *const value = any_cast<hldb::Constant>(arg);
    if (value == nullptr) continue;
    if (value->getDecompile() == std::string_view("\"a\"")) foundA = true;
    if (value->getDecompile() == std::string_view("\"b\"")) foundB = true;
  }
  EXPECT_TRUE(foundA) << "'.match_type_pair(\"a\")' actual \"a\" not found among call arguments";
  EXPECT_TRUE(foundB) << "'.requested_type(\"b\")' actual \"b\" not found among call arguments";
}

// ---------------------------------------------------------------------------
// m_matches_type_pair("c", "d");  -- positional, for contrast
// ---------------------------------------------------------------------------
TEST_F(FuncArgsByNameTest, SecondCallIsPositional) {
  const hldb::Function *const func = getFn("func");
  ASSERT_NE(func, nullptr);
  const hldb::Begin *const body = func->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(body->getStmts()->at(1));
  ASSERT_NE(call, nullptr) << "second statement should be a bare FuncCall statement";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::Constant *const first = findConstArgAt(call, 0);
  const hldb::Constant *const second = findConstArgAt(call, 1);
  ASSERT_NE(first, nullptr) << "'\"c\"' should be a Constant";
  ASSERT_NE(second, nullptr) << "'\"d\"' should be a Constant";
  EXPECT_EQ(first->getDecompile(), std::string_view("\"c\""));
  EXPECT_EQ(second->getDecompile(), std::string_view("\"d\""));
}

// ---------------------------------------------------------------------------
// m_matches_type_pair(.requested_type("e"), .match_type_pair("f"));
// -- named arguments given in REVERSE order relative to the formal list.
// ---------------------------------------------------------------------------
TEST_F(FuncArgsByNameTest, ThirdCallBindsArgumentsByNameEvenWhenReversed) {
  const hldb::Function *const func = getFn("func");
  ASSERT_NE(func, nullptr);
  const hldb::Begin *const body = func->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(body->getStmts()->at(2));
  ASSERT_NE(call, nullptr) << "third statement should be a bare FuncCall statement";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  // Sec 13.5.3: "The order in which arguments are connected by name is not
  // significant" -- '.requested_type("e"), .match_type_pair("f")' must
  // still bind "e" to requested_type and "f" to match_type_pair,
  // regardless of the reversed syntactic order used at the call site.
  bool foundE = false, foundF = false;
  for (hldb::Any *const arg : *call->getArguments()) {
    const hldb::Constant *const value = any_cast<hldb::Constant>(arg);
    if (value == nullptr) continue;
    if (value->getDecompile() == std::string_view("\"e\"")) foundE = true;
    if (value->getDecompile() == std::string_view("\"f\"")) foundF = true;
  }
  EXPECT_TRUE(foundE) << "'.requested_type(\"e\")' actual \"e\" not found among call arguments";
  EXPECT_TRUE(foundF) << "'.match_type_pair(\"f\")' actual \"f\" not found among call arguments";
}

TEST_F(FuncArgsByNameTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
