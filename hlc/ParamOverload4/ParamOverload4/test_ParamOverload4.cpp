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

// Validates the HLDB model built for tests/ParamOverload4/dut.sv:
//
//   module ibex_pmp #(
//       parameter int unsigned PMPGranularity = 0,
//       parameter int unsigned PMPNumChan     = 2,
//       parameter int unsigned PMPNumRegions  = 4
//   ) (
//   );
//     for (genvar r = 0; r < PMPNumRegions; r++) begin : g_addr_exp
//       for (genvar b = PMPGranularity+2; b < 34; b++) begin : g_bitmask
//         if (b == PMPGranularity+2) begin : g_bit0
//         end else begin : g_others
//         end
//       end
//     end
//   endmodule
//
// (The comments in the fixture are elided above.)
//
// The point of the fixture is two nested loop generates (27.4) whose bounds
// come from parameters, with a conditional generate (27.5) inside that
// compares the inner genvar with a parameter expression. The regression this
// file exists to catch is HLC computing the wrong number of iterations (for
// example by evaluating 'PMPGranularity+2' once with the wrong value), or
// selecting the wrong branch of the 'if'.
//
// Values: PMPNumRegions = 4, so r runs over 0 to 3. PMPGranularity = 0, so b
// starts at 2 and runs to 33: 32 iterations. 'b == 2' holds only for b = 2,
// so each g_bitmask has exactly one g_bit0 scope and 31 g_others scopes.
//
// What is checked, and why:
//   - the '( )' port list declares one null port (37.14 detail 10, "module
//     M();")
//   - exactly 3 parameters, PMPGranularity, PMPNumChan and PMPNumRegions,
//     all overridable parameter ports (6.20.4), all int unsigned, defaulting
//     to 0, 2 and 4
//   - the only generate construct is the loop g_addr_exp, whose condition
//     is vpiLtOp over the RefObjs r and PMPNumRegions, bound to the module's
//     Parameter
//   - its block holds the loop g_bitmask, whose condition is vpiLtOp over b
//     and the Constant 34
//   - g_bitmask's block holds an if-else generate (27.5) whose condition is
//     vpiEqOp over b and 'PMPGranularity+2', with the blocks g_bit0 and
//     g_others
//   - when elaborated: g_addr_exp has 4 scopes, each holding g_bitmask with
//     32 scopes, and across those scopes g_bit0 is instantiated once and
//     g_others 31 times
//   - when elaborated, ibex_pmp is the only top-level instance (23.3.1)
//   - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the generate scopes exist only after
// elaboration, so they are checked only when the design is elaborated, on
// Design::getTopModules(); the conditions are asserted on the definition in
// every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port. ModuleHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - How HLC names the generate scope array of the if-else inside each
//     g_bitmask scope beyond the block names is a model convention; the
//     scopes are found by the block names g_bit0 and g_others (27.6 applies
//     only to unnamed blocks).
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ParamOverload4Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamOverload4.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByDefName<hldb::Module>("ibex_pmp", m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Module *const m = getModule();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, m->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::GenFor *getOuterLoop() {
    const hldb::Module *const m = getModule();
    if (m == nullptr || m->getGenStmts() == nullptr || m->getGenStmts()->size() != 1u) return nullptr;
    return any_cast<hldb::GenFor>(m->getGenStmts()->at(0));
  }

  // The only item of the block of loop 'loop', of type T.
  template <typename T>
  static const T *getOnlyItem(const hldb::GenFor *loop) {
    if (loop == nullptr) return nullptr;
    const hldb::GenBlock *const block = loop->getStmt<hldb::GenBlock>();
    if (block == nullptr || block->getStmts() == nullptr || block->getStmts()->size() != 1u) return nullptr;
    return any_cast<T>(block->getStmts()->at(0));
  }

  // The number of generate scopes in 'scope' named 'name'.
  static size_t countScopes(const hldb::GenScope *scope, std::string_view name) {
    size_t count = 0;
    if (scope == nullptr || scope->getGenScopeArrays() == nullptr) return count;
    for (const hldb::GenScopeArray *const gsa : *scope->getGenScopeArrays()) {
      if ((gsa->getName() == name) && (gsa->getGenScopes() != nullptr)) count += gsa->getGenScopes()->size();
    }
    return count;
  }
};

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamOverload4Test, ModuleHasOneNullPort) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr) << "module 'ibex_pmp' not found";
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '( )' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '( )' declares exactly one null port";
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
}

TEST_F(ParamOverload4Test, DeclaresThreeOverridableIntUnsignedParameters) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 3u);
  const std::string_view names[] = {"PMPGranularity", "PMPNumChan", "PMPNumRegions"};
  const std::string_view values[] = {"0", "2", "4"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Parameter *const p = getParam(names[i]);
    ASSERT_NE(p, nullptr) << "parameter '" << names[i] << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.4: '" << names[i] << "' is a parameter port";
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << "6.11: '" << names[i] << "' is an int";
    EXPECT_FALSE(it->getSigned()) << "6.11.3: '" << names[i] << "' is 'int unsigned'";
    const hldb::ParamAssign *const pa = getParamAssign(m, names[i]);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
    EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), values[i]) << names[i];
  }
}

TEST_F(ParamOverload4Test, OuterLoopRunsBelowPmpNumRegions) {
  const hldb::GenFor *const loop = getOuterLoop();
  ASSERT_NE(loop, nullptr) << "27.4: the module's only generate construct is a loop";
  const hldb::GenBlock *const block = loop->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), "g_addr_exp");
  const hldb::Operation *const lt = loop->getCondition<hldb::Operation>();
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getOpType(), vpiLtOp) << "11.4.4: 'r < PMPNumRegions'";
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  const hldb::RefObj *const r = any_cast<hldb::RefObj>(lt->getOperands()->at(0));
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getName(), "r");
  const hldb::RefObj *const bound = any_cast<hldb::RefObj>(lt->getOperands()->at(1));
  ASSERT_NE(bound, nullptr);
  EXPECT_EQ(bound->getName(), "PMPNumRegions");
  ASSERT_NE(getParam("PMPNumRegions"), nullptr);
  EXPECT_EQ(bound->getActual(), getParam("PMPNumRegions")) << "the module's own parameter";
}

TEST_F(ParamOverload4Test, InnerLoopRunsBelowThirtyFour) {
  const hldb::GenFor *const inner = getOnlyItem<hldb::GenFor>(getOuterLoop());
  ASSERT_NE(inner, nullptr) << "g_addr_exp holds one loop generate";
  const hldb::GenBlock *const block = inner->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), "g_bitmask");
  const hldb::Operation *const lt = inner->getCondition<hldb::Operation>();
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getOpType(), vpiLtOp) << "11.4.4: 'b < 34'";
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  const hldb::RefObj *const b = any_cast<hldb::RefObj>(lt->getOperands()->at(0));
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "b");
  const hldb::Constant *const limit = any_cast<hldb::Constant>(lt->getOperands()->at(1));
  ASSERT_NE(limit, nullptr);
  EXPECT_EQ(limit->getDecompile(), "34");
}

TEST_F(ParamOverload4Test, IfElseComparesBWithGranularityPlusTwo) {
  const hldb::GenIfElse *const ifElse = getOnlyItem<hldb::GenIfElse>(getOnlyItem<hldb::GenFor>(getOuterLoop()));
  ASSERT_NE(ifElse, nullptr) << "27.5: g_bitmask holds one if-else generate";
  const hldb::Operation *const eq = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp) << "11.4.5: 'b == PMPGranularity+2'";
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  const hldb::RefObj *const b = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "b");
  const hldb::Operation *const add = any_cast<hldb::Operation>(eq->getOperands()->at(1));
  ASSERT_NE(add, nullptr) << "'PMPGranularity+2' is an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::RefObj *const gran = any_cast<hldb::RefObj>(add->getOperands()->at(0));
  ASSERT_NE(gran, nullptr);
  EXPECT_EQ(gran->getName(), "PMPGranularity");
  EXPECT_EQ(gran->getActual(), getParam("PMPGranularity"));
  const hldb::Constant *const two = any_cast<hldb::Constant>(add->getOperands()->at(1));
  ASSERT_NE(two, nullptr);
  EXPECT_EQ(two->getDecompile(), "2");
  const hldb::GenBlock *const thenBlock = ifElse->getStmt<hldb::GenBlock>();
  ASSERT_NE(thenBlock, nullptr);
  EXPECT_EQ(thenBlock->getName(), "g_bit0");
  const hldb::GenBlock *const elseBlock = ifElse->getElseStmt<hldb::GenBlock>();
  ASSERT_NE(elseBlock, nullptr);
  EXPECT_EQ(elseBlock->getName(), "g_others");
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamOverload4Test, LoopsAreUnrolledFourByThirtyTwo) {
  if (m_design->getElaborated()) {
    const hldb::Module *const m = hldb::findByDefName<hldb::Module>("ibex_pmp", m_design->getTopModules());
    ASSERT_NE(m, nullptr) << "the elaborated ibex_pmp not found";
    const hldb::GenScopeArray *const outer =
        hldb::findByName<hldb::GenScopeArray>("g_addr_exp", m->getGenScopeArrays());
    ASSERT_NE(outer, nullptr) << "27.4: g_addr_exp is generated";
    ASSERT_NE(outer->getGenScopes(), nullptr);
    ASSERT_EQ(outer->getGenScopes()->size(), 4u) << "27.4: r runs over 0 to 3";
    for (size_t r = 0; r < 4; ++r) {
      EXPECT_EQ(countScopes(outer->getGenScopes()->at(r), "g_bitmask"), 32u)
          << "27.4: in g_addr_exp[" << r << "], b runs over 2 to 33";
    }
  }
}

TEST_F(ParamOverload4Test, OnlyFirstBitIsBit0) {
  if (m_design->getElaborated()) {
    const hldb::Module *const m = hldb::findByDefName<hldb::Module>("ibex_pmp", m_design->getTopModules());
    ASSERT_NE(m, nullptr);
    const hldb::GenScopeArray *const outer =
        hldb::findByName<hldb::GenScopeArray>("g_addr_exp", m->getGenScopeArrays());
    ASSERT_NE(outer, nullptr);
    ASSERT_NE(outer->getGenScopes(), nullptr);
    for (const hldb::GenScope *const region : *outer->getGenScopes()) {
      const hldb::GenScopeArray *const bitmask =
          hldb::findByName<hldb::GenScopeArray>("g_bitmask", region->getGenScopeArrays());
      ASSERT_NE(bitmask, nullptr);
      ASSERT_NE(bitmask->getGenScopes(), nullptr);
      size_t bit0 = 0;
      size_t others = 0;
      for (const hldb::GenScope *const bit : *bitmask->getGenScopes()) {
        bit0 += countScopes(bit, "g_bit0");
        others += countScopes(bit, "g_others");
      }
      EXPECT_EQ(bit0, 1u) << "27.5: 'b == 2' holds only for b = 2";
      EXPECT_EQ(others, 31u) << "27.5: the 31 other values of b take the else block";
    }
  }
}

TEST_F(ParamOverload4Test, IbexPmpIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: ibex_pmp is the only module";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "ibex_pmp");
  }
}

TEST_F(ParamOverload4Test, NoFatalSyntaxOrErrorDiagnostics) {
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
