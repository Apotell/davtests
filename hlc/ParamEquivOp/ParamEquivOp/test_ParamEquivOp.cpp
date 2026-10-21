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

// Validates the HLDB model built for tests/ParamEquivOp/dut.sv:
//
//   module GOOD();
//   endmodule
//
//   module top();
//   parameter p_edma_irq_read_clear     = 1'b0;
//   parameter p_edma_tx_pkt_buffer      = 1'b1;
//   parameter p_edma_rx_pkt_buffer      = 1'b1;
//   parameter p_edma_queues             = 32'd1;
//   parameter p_edma_tsu                = 1'b1;
//   parameter p_edma_axi                = 1'b1;
//   parameter p_edma_has_pcs            = 1'b1;
//   parameter p_edma_ext_fifo_interface = 1'b0;
//   parameter p_has_dma = (p_edma_ext_fifo_interface == 1'b0);
//   parameter p_int_exists = { 2'b11, (p_edma_tsu == 1), 2'b11,
//                              (p_edma_tsu == 1), 8'b11111111,
//                              (p_edma_has_pcs == 1), (p_edma_has_pcs == 1),
//                              4'b1111, (p_has_dma == 1), 1'b1,
//                              (p_edma_has_pcs == 0), 1'b0, 1'b1,
//                              (p_has_dma == 1), 2'b11, (p_has_dma == 1),
//                              (p_has_dma == 1), 2'b11 };
//   if (p_int_exists == 32'b11111111111111111111110011111111) begin
//     GOOD good();
//   end
//   endmodule
//
// The point of the fixture is a parameter built as a concatenation
// (11.4.12) that mixes sized literals with equality results. 11.4.5 says of
// the equality operators that "the result shall be 1'b0 if the comparison
// fails and 1'b1 if it succeeds", so each '(x == y)' item contributes exactly
// one bit and the concatenation is 32 bits wide. The regression this file
// exists to catch is HLC giving an equality result the width of an integer
// (or of its operands) inside a concatenation: the concatenation would then
// be wider than 32 bits, the generate condition would compare unequal, and
// 'good' would never be instantiated.
//
// Values: p_edma_tsu = 1 and p_edma_has_pcs = 1, so '(p_edma_tsu == 1)' and
// '(p_edma_has_pcs == 1)' are 1 and '(p_edma_has_pcs == 0)' is 0.
// p_has_dma = (1'b0 == 1'b0) = 1, so every '(p_has_dma == 1)' is 1. Read
// from bit 31 down, p_int_exists is twenty-two 1s, then 0 (bit 9, the
// '(p_edma_has_pcs == 0)' item), then 0 (bit 8, the reserved 1'b0), then
// eight 1s: 32'hFFFFFCFF. That is the literal the generate condition tests,
// so the 'if' selects its block.
//
// What is checked, and why:
//   Module GOOD and module top
//     - each '()' declares one null port (37.14 detail 10, "module M();")
//   Module top
//     - exactly 10 parameters, all overridable: top has no
//       parameter_port_list, so 6.20.1 does not make them local
//     - the eight flag parameters hold the sized literals written
//     - p_has_dma's unreduced value is Operation vpiEqOp (11.4.5) over the
//       RefObj p_edma_ext_fifo_interface, bound to top's Parameter, and the
//       Constant 1'b0
//     - p_int_exists's unreduced value is Operation vpiConcatOp (11.4.12)
//       with 18 operands, in source order: the 9 literals keep their text and
//       the 9 equality items are vpiEqOp over the named parameter and the
//       literal written
//     - the only generate construct is an 'if' (27.5) whose condition is
//       vpiEqOp over the RefObj p_int_exists and the 32-bit literal, and
//       whose block holds the instance 'good' of GOOD
//   Elaboration
//     - p_has_dma is reduced to 1
//     - p_int_exists is reduced to 32'hFFFFFCFF, a 32-bit value (11.4.5,
//       11.4.12)
//     - the 'if' block is instantiated: genblk1 (27.6) has one generate
//       scope, and it holds the instance 'good' of GOOD
//     - top is the only top-level instance; GOOD is instantiated, so it is
//       not top-level (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values and the generate scope are
// checked only when the design is elaborated, on Design::getTopModules();
// the unreduced values and the generate condition are asserted on the
// definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Whether the parentheses around each equality are kept as a node is a
//     model convention; only the operation inside them is asserted.
//   - The radix HLC picks for a reduced value is a tool convention, so every
//     integer form is accepted.
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
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

namespace hlc {

// One item of the p_int_exists concatenation: either a literal, or an
// equality of the parameter 'm_param' with the literal 'm_literal'.
struct ConcatItem {
  bool m_isEquality;
  std::string_view m_param;
  std::string_view m_literal;
};

// The 18 items of p_int_exists, in source order.
constexpr ConcatItem kIntExistsItems[] = {
    {false, "", "2'b11"},       {true, "p_edma_tsu", "1"},     {false, "", "2'b11"},          {true, "p_edma_tsu", "1"},
    {false, "", "8'b11111111"}, {true, "p_edma_has_pcs", "1"}, {true, "p_edma_has_pcs", "1"}, {false, "", "4'b1111"},
    {true, "p_has_dma", "1"},   {false, "", "1'b1"},           {true, "p_edma_has_pcs", "0"}, {false, "", "1'b0"},
    {false, "", "1'b1"},        {true, "p_has_dma", "1"},      {false, "", "2'b11"},          {true, "p_has_dma", "1"},
    {true, "p_has_dma", "1"},   {false, "", "2'b11"},
};

class ParamEquivOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamEquivOp.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
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

  static const hldb::GenIf *getGenIf() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr || top->getGenStmts() == nullptr || top->getGenStmts()->size() != 1u) return nullptr;
    return any_cast<hldb::GenIf>(top->getGenStmts()->at(0));
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if ((ch != '_') && (ch != ' ')) text.push_back(ch);
    }
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, *value, base);
    return (res.ec == std::errc()) && (res.ptr == last);
  }

  // The reduced value of parameter 'name' in the elaborated top.
  static const hldb::Constant *getElabValue(std::string_view name) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    const hldb::ParamAssign *const pa = getParamAssign(top, name);
    if (pa == nullptr) return nullptr;
    return pa->getRhs<hldb::Constant>();
  }

  // Verifies the parameter 'name' of top holds the literal 'text'.
  static void ExpectLiteral(std::string_view name, std::string_view text) {
    const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "'";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "'" << name << "' is assigned a literal";
    EXPECT_EQ(c->getDecompile(), text) << name;
  }

  // Verifies 'op' is '<param> == <literal>' with <param> bound to top's
  // Parameter of that name.
  static void ExpectEquality(const hldb::Any *any, std::string_view param, std::string_view literal) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(any);
    ASSERT_NE(op, nullptr) << "'(" << param << " == " << literal << ")' is an Operation";
    EXPECT_EQ(op->getOpType(), vpiEqOp) << "11.4.5: '==' is logical equality";
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(op->getOperands()->at(0));
    ASSERT_NE(ref, nullptr) << "the left operand of '== " << literal << "' names " << param;
    EXPECT_EQ(ref->getName(), param);
    ASSERT_NE(getParam(getModule("top"), param), nullptr);
    EXPECT_EQ(ref->getActual(), getParam(getModule("top"), param)) << "'" << param << "' is top's own parameter";
    const hldb::Constant *const c = any_cast<hldb::Constant>(op->getOperands()->at(1));
    ASSERT_NE(c, nullptr) << "the right operand of '" << param << " ==' is a literal";
    EXPECT_EQ(c->getDecompile(), literal);
  }
};

// ---------------------------------------------------------------------------
// module GOOD(); and module top();
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamEquivOpTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"GOOD", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << name << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: " << name << "'s '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamEquivOpTest, TopDeclaresTenOverridableParameters) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getParameters(), nullptr);
  EXPECT_EQ(top->getParameters()->size(), 10u);
  for (std::string_view name :
       {"p_edma_irq_read_clear", "p_edma_tx_pkt_buffer", "p_edma_rx_pkt_buffer", "p_edma_queues", "p_edma_tsu",
        "p_edma_axi", "p_edma_has_pcs", "p_edma_ext_fifo_interface", "p_has_dma", "p_int_exists"}) {
    const hldb::Parameter *const p = getParam(top, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: top has no parameter port list, so '" << name << "' is overridable";
  }
}

TEST_F(ParamEquivOpTest, FlagParametersHoldTheirLiterals) {
  ExpectLiteral("p_edma_irq_read_clear", "1'b0");
  ExpectLiteral("p_edma_tx_pkt_buffer", "1'b1");
  ExpectLiteral("p_edma_rx_pkt_buffer", "1'b1");
  ExpectLiteral("p_edma_queues", "32'd1");
  ExpectLiteral("p_edma_tsu", "1'b1");
  ExpectLiteral("p_edma_axi", "1'b1");
  ExpectLiteral("p_edma_has_pcs", "1'b1");
  ExpectLiteral("p_edma_ext_fifo_interface", "1'b0");
}

TEST_F(ParamEquivOpTest, HasDmaIsEqualityOfExtFifoInterfaceWithZero) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), "p_has_dma");
  ASSERT_NE(pa, nullptr);
  ExpectEquality(pa->getRhs(), "p_edma_ext_fifo_interface", "1'b0");
}

TEST_F(ParamEquivOpTest, IntExistsIsEighteenItemConcatenation) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), "p_int_exists");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const concat = pa->getRhs<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "unreduced, p_int_exists is an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12: braces with a list of items are a concatenation";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), std::size(kIntExistsItems)) << "the concatenation writes 18 items";
  for (size_t i = 0; i < std::size(kIntExistsItems); ++i) {
    const ConcatItem &item = kIntExistsItems[i];
    SCOPED_TRACE(testing::Message() << "concatenation item " << i);
    if (item.m_isEquality) {
      ExpectEquality(concat->getOperands()->at(i), item.m_param, item.m_literal);
    } else {
      const hldb::Constant *const c = any_cast<hldb::Constant>(concat->getOperands()->at(i));
      ASSERT_NE(c, nullptr) << "item " << i << " is the literal " << item.m_literal;
      EXPECT_EQ(c->getDecompile(), item.m_literal);
    }
  }
}

TEST_F(ParamEquivOpTest, GenerateIfComparesIntExistsWithLiteral) {
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr) << "27.5: top's only generate construct is an 'if'";
  const hldb::Operation *const cond = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp) << "11.4.5";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "p_int_exists");
  ASSERT_NE(getParam(getModule("top"), "p_int_exists"), nullptr);
  EXPECT_EQ(ref->getActual(), getParam(getModule("top"), "p_int_exists"));
  const hldb::Constant *const literal = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->getDecompile(), "32'b11111111111111111111110011111111");
}

TEST_F(ParamEquivOpTest, GenerateIfBlockHoldsGood) {
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr);
  const hldb::GenBlock *const block = genIf->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr) << "the 'if' writes a 'begin ... end' block";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 1u) << "the block holds one item";
  const hldb::RefInstance *const good = any_cast<hldb::RefInstance>(block->getStmts()->at(0));
  ASSERT_NE(good, nullptr) << "the item is the instance 'good'";
  EXPECT_EQ(good->getName(), "good");
  ASSERT_NE(good->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = good->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("GOOD")) << "'good' instantiates GOOD";
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(ParamEquivOpTest, HasDmaReducesToOne) {
  if (m_design->getElaborated()) {
    const hldb::Constant *const c = getElabValue("p_has_dma");
    ASSERT_NE(c, nullptr) << "6.20.2: p_has_dma should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 1u) << "11.4.5: (1'b0 == 1'b0) is 1'b1";
  }
}

TEST_F(ParamEquivOpTest, IntExistsReducesToThirtyTwoBitFFFFFCFF) {
  if (m_design->getElaborated()) {
    const hldb::Constant *const c = getElabValue("p_int_exists");
    ASSERT_NE(c, nullptr) << "6.20.2: p_int_exists should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 0xFFFFFCFFu) << "11.4.5, 11.4.12: twenty-two 1s, 0, 0, eight 1s";
    EXPECT_EQ(c->getSize(), 32) << "11.4.5: each equality item is 1 bit, so the concatenation is 32 bits";
  }
}

TEST_F(ParamEquivOpTest, GenerateBlockIsInstantiatedWithGood) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::GenScopeArray *const gsa = hldb::findByName<hldb::GenScopeArray>("genblk1", top->getGenScopeArrays());
    ASSERT_NE(gsa, nullptr) << "27.6: the unnamed 'if' block is genblk1, and the condition holds";
    ASSERT_NE(gsa->getGenScopes(), nullptr);
    ASSERT_EQ(gsa->getGenScopes()->size(), 1u);
    const hldb::GenScope *const scope = gsa->getGenScopes()->at(0);
    const hldb::Module *const good = hldb::findByName<hldb::Module>("good", scope->getModules());
    ASSERT_NE(good, nullptr) << "genblk1 holds the instance 'good'";
    EXPECT_EQ(good->getDefName(), "GOOD");
  }
}

TEST_F(ParamEquivOpTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: GOOD is instantiated, so it is not top-level";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamEquivOpTest, NoFatalSyntaxOrErrorDiagnostics) {
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
