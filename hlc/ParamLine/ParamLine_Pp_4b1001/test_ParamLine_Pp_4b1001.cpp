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

// Validates the HLDB model built for tests/ParamLine/dut.sv compiled with the
// command-line option -Pp=4'b1001 (ParamLine_Pp_4b1001.hlc), which overrides the
// parameter p of the top-level module top:
//
//   module top();
//       parameter p = 1'b0;
//      if (p == "BLAH") begin
//         GOOD_BLAH good();
//      end
//      if (p == 4'b1001) begin
//         GOOD_BIN1 good();
//      end
//      if (p == 4'b1011) begin
//         GOOD_BIN2 good();
//      end
//      if (p == 'hFF) begin
//         GOOD_HEXA good();
//      end
//      if (p == -12) begin
//         GOOD_MINUS12 good();
//      end
//      if (p == 12) begin
//         GOOD_12 good();
//      end
//      if (p == 12.12) begin
//         GOOD_REAL12 good();
//      end
//      if (p == 0) begin
//         GOOD_0 good();
//      end
//      if (p == "STR.WITH.DOT") begin
//         GOOD_STR_WITH_DOT good();
//      end
//      if (p == "-STR") begin
//         GOOD_MINUS_STR good();
//      end
//   endmodule
//
// The fixture is shared by ten variants, each of which sets p from the
// command line to a value that one of the ten 'if' constructs tests. The
// point is overriding a top-level parameter with values of different kinds
// (string, real, sized and unsized based, negative), and the regression this
// file exists to catch is HLC applying the value with the wrong type or size,
// so that the wrong conditional generate block (27.5) is selected, or none.
// p has no type or range, so 6.20.2 gives it "the type and range of the final
// value assigned to the parameter, after any value overrides have been
// applied". None of the GOOD_* modules is defined in the file.
//
// The command line is not part of IEEE 1800. This file assumes HLC reads the
// option value as the SystemVerilog literal it spells, with double quotes
// making a string literal (5.9).
//
// Value: 4'b1001, a 4-bit unsigned binary literal (5.7.1, 11.8.1) worth 9.
// Only 'p == 4'b1001' holds: 4'b1011 is 11, 'hFF is 255, and -12 compares
// as the unsigned 32'hFFFFFFF4. Only the 2nd construct is selected:
// genblk2, with GOOD_BIN1.
//
// What is checked, and why:
//   - '()' declares one null port (37.14 detail 10, "module M();")
//   - p is overridable (top has no parameter port list, 6.20.1), and the
//     definition keeps its default 1'b0
//   - top holds exactly 10 generate constructs, all 'if' (27.5), in source
//     order; each condition is vpiEqOp (11.4.5) over the RefObj p, bound to
//     top's Parameter, and the literal written
//   - the string, binary, hexadecimal and real literals have the constant
//     types vpiStringConst, vpiBinaryConst, vpiHexConst and vpiRealConst
//     (5.9, 5.7.1, 5.7.2); -12 is the unary minus of 12 (11.4.3)
//   - each block holds exactly one item, the instance 'good' of its GOOD_*
//     module
//   - 27.5: "The selected generate block, if any, is instantiated into the
//     model", and only genblk2 is selected, so no error names the GOOD_*
//     module of any of the other 9 blocks
//   - when elaborated, exactly one generate scope exists, and it is
//     genblk2 (27.6: an unnamed block is named "genblk<n>" after the number
//     of its generate construct)
//   - when elaborated, top is the only top-level instance (23.3.1)
//   - there is no fatal and no syntax diagnostic
//
// Reduction and elaboration: the selected generate scope exists only after
// elaboration, so it is checked only when the design is elaborated, on
// Design::getTopModules(); the definition is asserted in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port. TopHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (errors for modules in unselected generate blocks), not
// a defect in this test: HLC reports an "Unsupported typespec" error for the
// undefined GOOD_* module of every one of the 10 blocks, including the 9
// that the value of p leaves unselected, although 27.5 instantiates at most
// one block into the model. UnselectedBlocksReportNoError is expected to fail
// until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Whether HLC reports the undefined GOOD_BIN1 of the selected block. The
//     file never defines it, and this file does not pin how that is
//     reported.
//   - The reduced value of p in the elaborated top: the radix and form HLC
//     picks for it is a tool convention. OverrideSelectsGenblk2 is the
//     static evidence of the value.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <iterator>
#include <string>
#include <string_view>

namespace hlc {

// One 'if (p == <literal>)' construct of the fixture.
struct GoodBlock {
  std::string_view m_literal;
  int m_constType;
  std::string_view m_module;
};

// The ten constructs, in source order. m_constType is 0 where the literal's
// constant type is not asserted: -12 is an operation, and the VPI constant
// type of a plain decimal is left to the tool.
constexpr GoodBlock kBlocks[] = {
    {"\"BLAH\"", vpiStringConst, "GOOD_BLAH"},
    {"4'b1001", vpiBinaryConst, "GOOD_BIN1"},
    {"4'b1011", vpiBinaryConst, "GOOD_BIN2"},
    {"'hFF", vpiHexConst, "GOOD_HEXA"},
    {"-12", 0, "GOOD_MINUS12"},
    {"12", 0, "GOOD_12"},
    {"12.12", vpiRealConst, "GOOD_REAL12"},
    {"0", 0, "GOOD_0"},
    {"\"STR.WITH.DOT\"", vpiStringConst, "GOOD_STR_WITH_DOT"},
    {"\"-STR\"", vpiStringConst, "GOOD_MINUS_STR"},
};

// The number (27.6) of the generate construct that -Pp=4'b1001 selects.
constexpr size_t kSelected = 2;

class ParamLinePp4b1001Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamLine_Pp_4b1001.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Whether a diagnostic of error severity names 'symbol'. Which error
  // identifier HLC picks for it is a tool convention, so any is accepted.
  bool reportsErrorNaming(std::string_view symbol) {
    for (const ErrorDefinition::ErrorMap::value_type &entry : ErrorDefinition::getErrorInfoMap()) {
      if ((entry.second.m_severity == ErrorDefinition::ERROR) && (findError(entry.first, symbol) != nullptr)) {
        return true;
      }
    }
    return false;
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getP() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("p", top->getParameters());
  }

  // The 'index'-th generate construct of top, or nullptr unless top holds
  // exactly 10 generate constructs.
  static const hldb::GenIf *getGenIf(size_t index) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getGenStmts() == nullptr) return nullptr;
    if (top->getGenStmts()->size() != std::size(kBlocks)) return nullptr;
    return any_cast<hldb::GenIf>(top->getGenStmts()->at(index));
  }

  // The literal operand of construct 'index'.
  static const hldb::Any *getLiteral(size_t index) {
    const hldb::GenIf *const genIf = getGenIf(index);
    if (genIf == nullptr) return nullptr;
    const hldb::Operation *const eq = genIf->getCondition<hldb::Operation>();
    if (eq == nullptr || eq->getOperands() == nullptr || eq->getOperands()->size() != 2u) return nullptr;
    return eq->getOperands()->at(1);
  }

  // Verifies 'any' is the literal 'text': a Constant with that text, or, for
  // -12, the unary minus of the Constant 12 (11.4.3).
  static void ExpectLiteral(const hldb::Any *any, std::string_view text) {
    ASSERT_NE(any, nullptr) << "the literal " << text;
    if (const hldb::Constant *const c = any_cast<hldb::Constant>(any)) {
      EXPECT_EQ(c->getDecompile(), text);
      return;
    }
    const hldb::Operation *const minus = any_cast<hldb::Operation>(any);
    ASSERT_NE(minus, nullptr) << "the literal " << text << " is a Constant or a unary minus";
    ASSERT_EQ(text, "-12") << "only -12 is written with a sign";
    EXPECT_EQ(minus->getOpType(), vpiMinusOp) << "11.4.3: '-12' is the unary minus of 12";
    ASSERT_NE(minus->getOperands(), nullptr);
    ASSERT_EQ(minus->getOperands()->size(), 1u);
    const hldb::Constant *const twelve = any_cast<hldb::Constant>(minus->getOperands()->at(0));
    ASSERT_NE(twelve, nullptr);
    EXPECT_EQ(twelve->getDecompile(), "12");
  }
};

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamLinePp4b1001Test, TopHasOneNullPort) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(top->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  EXPECT_EQ(top->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(top->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
}

TEST_F(ParamLinePp4b1001Test, PIsOverridableWithDefaultOneBitZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  EXPECT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getP();
  ASSERT_NE(p, nullptr) << "parameter 'p' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: top has no parameter port list, so 'p' is overridable";
  ASSERT_NE(top->getParamAssigns(), nullptr);
  ASSERT_EQ(top->getParamAssigns()->size(), 1u);
  const hldb::Constant *const def = top->getParamAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getDecompile(), "1'b0") << "the definition keeps the default written in the source";
}

TEST_F(ParamLinePp4b1001Test, TenGenerateIfsTestPInSourceOrder) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  ASSERT_EQ(top->getGenStmts()->size(), std::size(kBlocks)) << "27.5: the module writes 10 'if' constructs";
  for (size_t i = 0; i < std::size(kBlocks); ++i) {
    SCOPED_TRACE(testing::Message() << "construct " << (i + 1));
    const hldb::GenIf *const genIf = getGenIf(i);
    ASSERT_NE(genIf, nullptr) << "27.5: an 'if' generate construct";
    const hldb::Operation *const eq = genIf->getCondition<hldb::Operation>();
    ASSERT_NE(eq, nullptr);
    EXPECT_EQ(eq->getOpType(), vpiEqOp) << "11.4.5";
    ASSERT_NE(eq->getOperands(), nullptr);
    ASSERT_EQ(eq->getOperands()->size(), 2u);
    const hldb::RefObj *const p = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getName(), "p");
    ASSERT_NE(getP(), nullptr);
    EXPECT_EQ(p->getActual(), getP()) << "'p' is top's own parameter";
    ExpectLiteral(eq->getOperands()->at(1), kBlocks[i].m_literal);
  }
}

TEST_F(ParamLinePp4b1001Test, LiteralsHaveTheirConstantTypes) {
  for (size_t i = 0; i < std::size(kBlocks); ++i) {
    if (kBlocks[i].m_constType == 0) continue;
    SCOPED_TRACE(testing::Message() << "construct " << (i + 1) << ", " << kBlocks[i].m_literal);
    const hldb::Constant *const c = any_cast<hldb::Constant>(getLiteral(i));
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getConstType(), kBlocks[i].m_constType) << "5.9, 5.7.1, 5.7.2";
  }
}

TEST_F(ParamLinePp4b1001Test, EachBlockInstantiatesItsGoodModule) {
  for (size_t i = 0; i < std::size(kBlocks); ++i) {
    SCOPED_TRACE(testing::Message() << "construct " << (i + 1));
    const hldb::GenIf *const genIf = getGenIf(i);
    ASSERT_NE(genIf, nullptr);
    const hldb::GenBlock *const block = genIf->getStmt<hldb::GenBlock>();
    ASSERT_NE(block, nullptr) << "the 'if' writes a 'begin ... end' block";
    ASSERT_NE(block->getStmts(), nullptr);
    ASSERT_EQ(block->getStmts()->size(), 1u) << "the block holds one item";
    const hldb::RefInstance *const good = any_cast<hldb::RefInstance>(block->getStmts()->at(0));
    ASSERT_NE(good, nullptr) << "the item is a module instance";
    EXPECT_EQ(good->getName(), "good");
    ASSERT_NE(good->getTypespec(), nullptr);
    EXPECT_EQ(good->getTypespec()->getName(), kBlocks[i].m_module);
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (errors for
// modules in unselected generate blocks) in the file header.
TEST_F(ParamLinePp4b1001Test, UnselectedBlocksReportNoError) {
  for (size_t i = 0; i < std::size(kBlocks); ++i) {
    if (i + 1 == kSelected) continue;
    EXPECT_FALSE(reportsErrorNaming(kBlocks[i].m_module))
        << "27.5: construct " << (i + 1) << " is not selected, so " << kBlocks[i].m_module
        << " is not instantiated into the model";
  }
}

TEST_F(ParamLinePp4b1001Test, OverrideSelectsGenblk2) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    size_t scopes = 0;
    std::string_view selected;
    if (top->getGenScopeArrays() != nullptr) {
      for (const hldb::GenScopeArray *const gsa : *top->getGenScopeArrays()) {
        if (gsa->getGenScopes() == nullptr || gsa->getGenScopes()->empty()) continue;
        scopes += gsa->getGenScopes()->size();
        selected = gsa->getName();
      }
    }
    EXPECT_EQ(scopes, 1u) << "27.5: exactly one of the 10 conditions holds";
    EXPECT_EQ(selected, "genblk" + std::to_string(kSelected)) << "27.6: the selected block is genblk2";
  }
}

TEST_F(ParamLinePp4b1001Test, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: top is the only module";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamLinePp4b1001Test, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file and the option value are syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
