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

// Validates the HLDB model built for tests/ParamMultiConcat/dut.sv:
//
//   module dut
//   #(
//      parameter int Width
//   ) (
//      input [Width-1:0] wmask_i
//   );
//   endmodule
//
//   module top;
//      parameter int Width = 16;
//      parameter int EccWidth = 3;
//   parameter int BBB = {Width+EccWidth{1'b1}};
//      dut #(
//         .Width(Width + EccWidth)
//      ) u_dut (
//         .wmask_i({Width + EccWidth{1'b1}})
//      );
//   endmodule // top
//
// The point of the fixture is a replication (11.4.12.1) whose multiplier is
// a parameter expression, used both as a parameter value and as an input
// port connection, together with a parameter port that has no default
// (legal in a parameter_port_list, Syntax 6-6 footnote 22) and is overridden
// by the only instance. The regression this file exists to catch is HLC
// parsing '{Width+EccWidth{1'b1}}' as anything other than a replication with
// multiplier 'Width + EccWidth', or failing to give dut's port its width from
// the override.
//
// Values: Width + EccWidth = 16 + 3 = 19, so the replication is nineteen 1s,
// 19'h7FFFF. BBB = 524287. In u_dut, Width = 19, so wmask_i is [18:0].
//
// What is checked, and why:
//   Module dut
//     - exactly 1 parameter, Width, overridable (6.20.4), of type int, with
//       no default (Syntax 6-6 footnote 22)
//     - exactly 1 port, wmask_i, an input (23.2.2.2) of logic [Width-1:0]
//       whose left bound is Operation vpiSubOp over the RefObj Width, bound
//       to dut's Parameter, and the Constant 1
//   Module top
//     - has no port
//     - exactly 3 parameters, Width, EccWidth and BBB, all overridable (top
//       has no parameter port list, 6.20.1), of type int; Width and EccWidth
//       hold 16 and 3
//     - BBB's unreduced value is Operation vpiMultiConcatOp (11.4.12.1)
//       whose multiplier is vpiAddOp over the RefObjs Width and EccWidth,
//       bound to top's Parameters, and whose replicated item is 1'b1
//     - u_dut instantiates dut, overrides Width with 'Width + EccWidth', and
//       connects wmask_i to the same replication
//   Elaboration
//     - top's BBB is reduced to 524287
//     - u_dut's Width is 19 and its port wmask_i is [18:0]
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values and the elaborated port
// width are checked only when the design is elaborated, on
// Design::getTopModules(); the unreduced values, bindings, override and
// connection are asserted on the definitions in every run.
//
// What is NOT checked, and why:
//   - Whether the replicated item 1'b1 is held directly or inside an inner
//     concatenation node is a model convention; both forms are accepted.
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
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamMultiConcatTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamMultiConcat.hlc"}); }
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

  static const hldb::RefInstance *getUDut() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_dut", top->getRefInstances());
  }

  // Verifies 'any' is 'Width + EccWidth' over top's own parameters.
  static void ExpectWidthPlusEccWidth(const hldb::Any *any) {
    const hldb::Operation *const add = any_cast<hldb::Operation>(any);
    ASSERT_NE(add, nullptr) << "'Width + EccWidth' is an Operation";
    EXPECT_EQ(add->getOpType(), vpiAddOp) << "11.4.3: an addition";
    ASSERT_NE(add->getOperands(), nullptr);
    ASSERT_EQ(add->getOperands()->size(), 2u);
    const std::string_view names[] = {"Width", "EccWidth"};
    for (size_t i = 0; i < 2; ++i) {
      const hldb::RefObj *const ref = any_cast<hldb::RefObj>(add->getOperands()->at(i));
      ASSERT_NE(ref, nullptr) << "operand " << i << " is the name '" << names[i] << "'";
      EXPECT_EQ(ref->getName(), names[i]);
      ASSERT_NE(getParam(getModule("top"), names[i]), nullptr);
      EXPECT_EQ(ref->getActual(), getParam(getModule("top"), names[i]))
          << "'" << names[i] << "' is top's own parameter";
    }
  }

  // Verifies 'any' is the replication '{Width + EccWidth{1'b1}}'.
  static void ExpectReplication(const hldb::Any *any) {
    const hldb::Operation *const rep = any_cast<hldb::Operation>(any);
    ASSERT_NE(rep, nullptr) << "the replication is an Operation";
    EXPECT_EQ(rep->getOpType(), vpiMultiConcatOp) << "11.4.12.1: a multiplier before a concatenation";
    ASSERT_NE(rep->getOperands(), nullptr);
    ASSERT_EQ(rep->getOperands()->size(), 2u) << "the multiplier and the replicated item";
    ExpectWidthPlusEccWidth(rep->getOperands()->at(0));
    const hldb::Any *item = rep->getOperands()->at(1);
    if (const hldb::Operation *const inner = any_cast<hldb::Operation>(item)) {
      ASSERT_EQ(inner->getOpType(), vpiConcatOp) << "the replicated item may be an inner concatenation";
      ASSERT_NE(inner->getOperands(), nullptr);
      ASSERT_EQ(inner->getOperands()->size(), 1u);
      item = inner->getOperands()->at(0);
    }
    const hldb::Constant *const one = any_cast<hldb::Constant>(item);
    ASSERT_NE(one, nullptr) << "the replicated item is the literal 1'b1";
    EXPECT_EQ(one->getDecompile(), "1'b1");
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

  // Verifies the reduced value of parameter 'name' in the elaborated scope
  // 'scope' is 'expected'.
  static void ExpectElabParam(const hldb::Scope *scope, std::string_view name, uint64_t expected,
                              std::string_view why) {
    ASSERT_NE(scope, nullptr);
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated scope";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }
};

// ---------------------------------------------------------------------------
// module dut
// ---------------------------------------------------------------------------

TEST_F(ParamMultiConcatTest, DutDeclaresIntWidthWithoutDefault) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr) << "module 'dut' not found";
  ASSERT_NE(dut->getParameters(), nullptr);
  EXPECT_EQ(dut->getParameters()->size(), 1u);
  const hldb::Parameter *const width = getParam(dut, "Width");
  ASSERT_NE(width, nullptr) << "parameter 'Width' not found";
  EXPECT_FALSE(width->getLocalParam()) << "6.20.4: 'Width' is a parameter port, not a localparam";
  ASSERT_NE(width->getTypespec(), nullptr);
  EXPECT_NE(width->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'Width' is declared 'int'";
  const hldb::ParamAssign *const pa = getParamAssign(dut, "Width");
  EXPECT_TRUE(pa == nullptr || pa->getRhs() == nullptr)
      << "Syntax 6-6 footnote 22: the default is omitted, which is legal in a parameter_port_list";
}

TEST_F(ParamMultiConcatTest, DutPortWmaskIsInputOfWidthMinusOneToZero) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getPorts(), nullptr);
  ASSERT_EQ(dut->getPorts()->size(), 1u);
  const hldb::Port *const port = dut->getPorts()->at(0);
  EXPECT_EQ(port->getName(), "wmask_i");
  EXPECT_EQ(port->getDirection(), vpiInput) << "23.2.2.2: 'input [Width-1:0] wmask_i'";
  ASSERT_NE(port->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = port->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "23.2.2.3: an implicit data type is logic";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "unreduced, the left bound is 'Width-1'";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const width = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(width, nullptr);
  EXPECT_EQ(width->getName(), "Width");
  EXPECT_EQ(width->getActual(), getParam(dut, "Width")) << "'Width' is dut's own parameter";
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamMultiConcatTest, TopHasNoPort) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty())
      << "A.1.2: 'module top;' writes no list_of_port_declarations";
}

TEST_F(ParamMultiConcatTest, TopDeclaresThreeOverridableIntParameters) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  EXPECT_EQ(top->getParameters()->size(), 3u);
  for (std::string_view name : {"Width", "EccWidth", "BBB"}) {
    const hldb::Parameter *const p = getParam(top, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: top has no parameter port list, so '" << name << "' is overridable";
    ASSERT_NE(p->getTypespec(), nullptr);
    EXPECT_NE(p->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'" << name << "' is declared 'int'";
  }
}

TEST_F(ParamMultiConcatTest, WidthAndEccWidthHoldSixteenAndThree) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const width = getParamAssign(top, "Width");
  ASSERT_NE(width, nullptr);
  ASSERT_NE(width->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(width->getRhs<hldb::Constant>()->getDecompile(), "16");
  const hldb::ParamAssign *const ecc = getParamAssign(top, "EccWidth");
  ASSERT_NE(ecc, nullptr);
  ASSERT_NE(ecc->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(ecc->getRhs<hldb::Constant>()->getDecompile(), "3");
}

TEST_F(ParamMultiConcatTest, BbbIsReplicationOfOneBitOne) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), "BBB");
  ASSERT_NE(pa, nullptr);
  ExpectReplication(pa->getRhs());
}

TEST_F(ParamMultiConcatTest, UDutOverridesWidthWithSum) {
  const hldb::RefInstance *const inst = getUDut();
  ASSERT_NE(inst, nullptr) << "instance 'u_dut' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("dut"));
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "Width");
  ExpectWidthPlusEccWidth(mt->getParamAssigns()->at(0)->getRhs());
}

TEST_F(ParamMultiConcatTest, UDutConnectsWmaskToReplication) {
  const hldb::RefInstance *const inst = getUDut();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 1u) << "u_dut connects one port";
  const hldb::NamedArgument *const conn = inst->getArguments()->at(0);
  ASSERT_NE(conn, nullptr);
  const hldb::RefObj *const formal = conn->getLowConn<hldb::RefObj>();
  ASSERT_NE(formal, nullptr) << "23.3.2.2: the connection names its port";
  EXPECT_EQ(formal->getName(), "wmask_i");
  ExpectReplication(conn->getHighConn());
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamMultiConcatTest, BbbReducesToNineteenOnes) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ExpectElabParam(top, "BBB", 0x7FFFFu, "11.4.12.1: {19{1'b1}} = 19'h7FFFF = 524287");
  }
}

TEST_F(ParamMultiConcatTest, UDutWidthIsNineteen) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::Module *const dut = hldb::findByName<hldb::Module>("u_dut", top->getModules());
    ASSERT_NE(dut, nullptr) << "the instance u_dut is elaborated";
    ExpectElabParam(dut, "Width", 19u, "23.10.2.2: '.Width(Width + EccWidth)' = 16 + 3");
  }
}

TEST_F(ParamMultiConcatTest, UDutWmaskIsEighteenToZero) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::Module *const dut = hldb::findByName<hldb::Module>("u_dut", top->getModules());
    ASSERT_NE(dut, nullptr);
    ASSERT_NE(dut->getPorts(), nullptr);
    ASSERT_EQ(dut->getPorts()->size(), 1u);
    ASSERT_NE(dut->getPorts()->at(0)->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = dut->getPorts()->at(0)->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << "the left bound should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(left, &v)) << "unparsable constant '" << left->getDecompile() << "'";
    EXPECT_EQ(v, 18u) << "Width - 1 = 19 - 1";
  }
}

TEST_F(ParamMultiConcatTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: dut is instantiated by top";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamMultiConcatTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog; dut's Width is overridden by its only instance";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
