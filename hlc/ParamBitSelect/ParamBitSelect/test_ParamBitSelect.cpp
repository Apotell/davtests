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

// Validates the HLDB model built for tests/ParamBitSelect/dut.sv:
//
//   module GOOD();
//   endmodule
//
//   module dut();
//   parameter test = 4'b0011;
//   for (genvar k = 0; k < 4; k++) begin
//     if (test[k]) begin
//       MOD m ();
//       if (k == 0 || k == 1) begin
//          GOOD good();
//       end else begin
//          BAD bad();
//       end
//     end
//   end
//   endmodule // dut
//
// The point of the fixture is a generate condition that selects one bit of a
// parameter by the genvar of an enclosing loop generate (IEEE 1800-2023
// 27.4, 27.5). test = 4'b0011, so the if-generate is selected for k = 0 and
// k = 1 only, and in those iterations the nested if-generate selects GOOD.
// The design checks itself: BAD is declared nowhere and must never be
// instantiated. The regression this file exists to catch is HLC evaluating
// test[k] with the wrong bit order or the wrong genvar value.
//
// What is checked, and why:
//   Module GOOD
//     - '()' declares one null port (37.14 detail 10, "module M();") and the
//       body is empty
//   Module dut
//     - '()' declares one null port (37.14 detail 10)
//     - exactly 1 parameter, test, of the body with no parameter port list,
//       so it is not local (6.20.1); its value is the Constant "4'b0011"
//     - exactly 1 generate construct, a loop generate (GenFor, 27.4) whose
//       condition is Operation vpiLtOp over RefObj 'k' and Constant "4"
//     - its block holds an if-generate whose condition is a bit-select
//       (11.5.1) with the prefix bound to the Parameter test and the index
//       RefObj 'k'
//   Elaboration (27.4, 27.5, 27.6)
//     - GOOD appears in a module instantiation statement, so it is not a
//       top-level module even though that statement is inside a generate
//       block (23.3.1); dut is the only top-level instance
//     - the loop block is unnamed and the loop is the first generate
//       construct in dut, so it is named "genblk1" (27.6), with exactly 4
//       instances, k = 0 to 3
//     - bit k of 4'b0011 is 1 for k = 0 and k = 1 and 0 for k = 2 and k = 3,
//       so iterations 0 and 1 each hold the if-generate's block and
//       iterations 2 and 3 hold none
//     - in iterations 0 and 1, 'k == 0 || k == 1' holds, so the nested
//       if-generate selects the block instantiating 'good' of GOOD
//     - no instance named 'bad' exists anywhere in the elaborated design
//   Diagnostics
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: the generate conditions are evaluated during
// elaboration (27.5), so the instance tree is checked only under
// getElaborated(); the unreduced conditions are asserted on the module
// definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models a '()' port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The instance 'm' of MOD, a module declared nowhere. The standard does
//     not say how an instance of an undefined module is reported or
//     represented, so neither is asserted, and the error count is not
//     asserted either.
//   - How HLC records the genvar k in the unelaborated loop, and how it names
//     the nested unnamed generate blocks beyond the loop's own genblk1, are
//     model conventions; nested blocks are searched by content.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ParamBitSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamBitSelect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::GenFor *getLoop() {
    const hldb::Module *const dut = getModule("dut");
    if (dut == nullptr || dut->getGenStmts() == nullptr || dut->getGenStmts()->empty()) return nullptr;
    return any_cast<hldb::GenFor>(dut->getGenStmts()->at(0));
  }

  // The first GenIf found in 'stmt' or in a Begin it holds.
  static const hldb::GenIf *findGenIf(const hldb::Any *stmt) {
    if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(stmt)) return genIf;
    const hldb::Begin *const block = any_cast<hldb::Begin>(stmt);
    if (block == nullptr || block->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const item : *block->getStmts()) {
      if (const hldb::GenIf *const found = findGenIf(item)) return found;
    }
    return nullptr;
  }

  // The number of generate scopes directly in 'scope'.
  static size_t countGenScopes(const hldb::GenScope *scope) {
    size_t count = 0;
    if (scope == nullptr || scope->getGenScopeArrays() == nullptr) return count;
    for (const hldb::GenScopeArray *const gsa : *scope->getGenScopeArrays()) {
      if (gsa->getGenScopes() != nullptr) count += gsa->getGenScopes()->size();
    }
    return count;
  }

  // Whether an instance named 'name' exists in 'scope' or below it.
  static bool hasInstance(const hldb::GenScope *scope, std::string_view name) {
    if (scope == nullptr) return false;
    if (hldb::findByName<hldb::Module>(name, scope->getModules()) != nullptr) return true;
    if (scope->getGenScopeArrays() == nullptr) return false;
    for (const hldb::GenScopeArray *const gsa : *scope->getGenScopeArrays()) {
      if (gsa->getGenScopes() == nullptr) continue;
      for (const hldb::GenScope *const inner : *gsa->getGenScopes()) {
        if (hasInstance(inner, name)) return true;
      }
    }
    return false;
  }

  static const hldb::GenScopeArray *getElabLoop() {
    const hldb::Module *const dut = hldb::findByDefName<hldb::Module>("dut", m_design->getTopModules());
    if (dut == nullptr) return nullptr;
    return hldb::findByName<hldb::GenScopeArray>("genblk1", dut->getGenScopeArrays());
  }
};

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamBitSelectTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"GOOD", "dut"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' of " << name << " declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamBitSelectTest, GoodBodyIsEmpty) {
  const hldb::Module *const good = getModule("GOOD");
  ASSERT_NE(good, nullptr);
  EXPECT_TRUE(good->getNets() == nullptr || good->getNets()->empty());
  EXPECT_TRUE(good->getVariables() == nullptr || good->getVariables()->empty());
  EXPECT_TRUE(good->getGenStmts() == nullptr || good->getGenStmts()->empty());
}

TEST_F(ParamBitSelectTest, TestIsOverridableFourBitParameter) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParameters(), nullptr);
  ASSERT_EQ(dut->getParameters()->size(), 1u);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("test", dut->getParameters());
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: dut has no parameter port list, so a body 'parameter' is overridable";
  ASSERT_NE(dut->getParamAssigns(), nullptr);
  ASSERT_EQ(dut->getParamAssigns()->size(), 1u);
  const hldb::Constant *const value = dut->getParamAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "4'b0011");
}

TEST_F(ParamBitSelectTest, LoopGenerateRunsKBelowFour) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getGenStmts(), nullptr);
  ASSERT_EQ(dut->getGenStmts()->size(), 1u) << "the module body holds one generate construct";
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr) << "27.4: 'for (genvar k = 0; k < 4; k++)' is a loop generate construct";
  const hldb::Operation *const lt = loop->getCondition<hldb::Operation>();
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getOpType(), vpiLtOp);
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  const hldb::RefObj *const k = any_cast<hldb::RefObj>(lt->getOperands()->at(0));
  ASSERT_NE(k, nullptr);
  EXPECT_EQ(k->getName(), "k");
  const hldb::Constant *const four = any_cast<hldb::Constant>(lt->getOperands()->at(1));
  ASSERT_NE(four, nullptr);
  EXPECT_EQ(four->getDecompile(), "4");
}

TEST_F(ParamBitSelectTest, IfGenerateSelectsBitKOfTest) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr);
  const hldb::GenIf *const genIf = findGenIf(loop->getStmt());
  ASSERT_NE(genIf, nullptr) << "27.5: the loop block holds 'if (test[k]) begin ... end'";
  const hldb::BitSelect *const sel = genIf->getCondition<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr) << "11.5.1: 'test[k]' is a bit-select";
  const hldb::RefObj *const test = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(test->getName(), "test");
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  EXPECT_EQ(test->getActual(), hldb::findByName<hldb::Parameter>("test", dut->getParameters()))
      << "'test' is dut's parameter";
  const hldb::RefObj *const k = sel->getIndex<hldb::RefObj>();
  ASSERT_NE(k, nullptr);
  EXPECT_EQ(k->getName(), "k") << "the index is the loop's genvar";
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(ParamBitSelectTest, DutIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u)
        << "23.3.1: GOOD appears in an instantiation (inside a generate block), so it is not top-level";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "dut");
  }
}

TEST_F(ParamBitSelectTest, LoopIsInstantiatedFourTimesAsGenblk1) {
  if (m_design->getElaborated()) {
    const hldb::GenScopeArray *const loop = getElabLoop();
    ASSERT_NE(loop, nullptr) << "27.6: the unnamed loop block of dut's first generate construct is genblk1";
    ASSERT_NE(loop->getGenScopes(), nullptr);
    EXPECT_EQ(loop->getGenScopes()->size(), 4u) << "27.4: k runs over 0, 1, 2 and 3";
  }
}

TEST_F(ParamBitSelectTest, OnlyIterationsZeroAndOneHoldTheIfBlock) {
  if (m_design->getElaborated()) {
    const hldb::GenScopeArray *const loop = getElabLoop();
    ASSERT_NE(loop, nullptr);
    ASSERT_NE(loop->getGenScopes(), nullptr);
    ASSERT_EQ(loop->getGenScopes()->size(), 4u);
    for (size_t k = 0; k < 4; ++k) {
      EXPECT_EQ(countGenScopes(loop->getGenScopes()->at(k)), (k < 2) ? 1u : 0u)
          << "27.5: test[" << k << "] of 4'b0011 is " << ((k < 2) ? 1 : 0);
    }
  }
}

TEST_F(ParamBitSelectTest, GoodIsInstantiatedAndBadIsNot) {
  if (m_design->getElaborated()) {
    const hldb::GenScopeArray *const loop = getElabLoop();
    ASSERT_NE(loop, nullptr);
    ASSERT_NE(loop->getGenScopes(), nullptr);
    ASSERT_EQ(loop->getGenScopes()->size(), 4u);
    for (size_t k = 0; k < 4; ++k) {
      EXPECT_EQ(hasInstance(loop->getGenScopes()->at(k), "good"), k < 2)
          << "27.5: GOOD is instantiated exactly in the iterations where test[k] is 1 (k = " << k << ")";
      EXPECT_FALSE(hasInstance(loop->getGenScopes()->at(k), "bad"))
          << "27.5: 'k == 0 || k == 1' holds wherever the nested if is reached, so BAD is never selected (k = " << k
          << ")";
    }
  }
}

TEST_F(ParamBitSelectTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
