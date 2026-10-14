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

// Validates the HLDB model built for tests/ParamElab/dut.sv:
//
//   module dut ();
//           test #(
//                   .Width(32),
//                   .DataBitsPerMask(8)
//           ) t ();
//   endmodule
//
//   module test #(
//   parameter  int Width           = 32,
//   parameter  int DataBitsPerMask = 2
//   ) ();
//           localparam int MaskWidth = Width / DataBitsPerMask;
//
//   generate
//      for (i = 0; i < MaskWidth; i++) begin
//         G1 u();
//      end
//   endgenerate
//
//   endmodule
//
// The point of the fixture is a local parameter computed from two overridden
// parameters and then used as a loop generate bound (IEEE 1800-2023
// 23.10.2.2, 27.4). With Width = 32 and DataBitsPerMask = 8, MaskWidth in
// dut.t is 32 / 8 = 4. The regression this file exists to catch is HLC
// computing MaskWidth from the defaults (32 / 2 = 16) instead of the
// overrides.
//
// The loop index i is never declared, and 27.4 says "The loop index shall be
// declared in a genvar declaration prior to its use in a loop generate
// scheme". The file is therefore illegal, and HLC must report it.
//
// What is checked, and why:
//   Module dut
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - exactly 1 instance, t, of test, overriding by name (23.10.2.2) Width
//       with the Constant "32" and DataBitsPerMask with the Constant "8"
//   Module test
//     - '()' declares one null port (37.14 detail 10)
//     - exactly 3 parameters: Width and DataBitsPerMask, in the parameter
//       port list so overridable (6.20.1), defaulting to 32 and 2; and
//       MaskWidth, a localparam whose unreduced value is Operation vpiDivOp
//       (11.4.3) over RefObj 'Width' and RefObj 'DataBitsPerMask', each bound
//       to test's Parameter
//     - exactly 1 generate item, the generate region (27.3) written with
//       'generate ... endgenerate', holding a loop generate (GenFor) whose
//       condition is Operation vpiLtOp (11.4.4) over RefObj 'i' and RefObj
//       'MaskWidth', bound to test's Parameter, and whose block holds the
//       instance 'u'
//   Elaboration
//     - test is instantiated, so dut is the only top-level instance (23.3.1)
//     - dut.t has Width = 32, DataBitsPerMask = 8 and MaskWidth = 4
//   Diagnostics
//     - no syntax or fatal diagnostic, since 'for (i = 0; ...)' matches the
//       genvar_initialization grammar '[ genvar ] genvar_identifier =
//       constant_expression' (A.4.2)
//     - an error-severity diagnostic naming the undeclared loop index i
//       (27.4); which of HLC's undeclared-name error types carries it is a
//       tool convention, so any of them is accepted
//
// Reduction and elaboration: the reduced parameter values are checked only
// when the design is elaborated, on Design::getTopModules(); the unreduced
// expressions are asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (undeclared genvar unreported), not a defect in this
// test: HLC accepts the loop index i with no genvar declaration and reports
// nothing for it; it models i as a Variable of the GenFor. The only error
// HLC emits is for the undefined module G1. UndeclaredLoopIndexIsAnError is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - The generate blocks the loop would create. The loop generate scheme is
//     illegal (27.4), so the standard defines no expansion for it;
//     MaskWidth = 4 is the static evidence of the loop bound.
//   - The instance 'u' of G1, a module declared nowhere. The standard does
//     not say how an instance of an undefined module is reported or
//     represented, so only its presence in the loop block is asserted. For
//     the same reason, the total error count is not asserted: an error for
//     G1 would satisfy it without saying anything about i.
//   - How HLC records the parameter override of an unelaborated instance is a
//     model convention; it is read from the instance's module typespec.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_region.h>
#include <hldb/int_typespec.h>
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
#include <string>
#include <string_view>

namespace hlc {

// The error types HLC uses for a name with no declaration. Which one reports
// the loop index is a tool convention, so any of them, at error severity and
// naming 'i', is accepted.
constexpr ErrorDefinition::ErrorType kUndeclaredNameErrors[] = {ErrorDefinition::COMP_UNDEFINED_VARIABLE,
                                                                ErrorDefinition::ELAB_UNDEF_VARIABLE,
                                                                ErrorDefinition::COMP_FAILED_TO_BIND};

class ParamElabTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamElab.hlc"}); }
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

  // The override expression for 'param' that the unelaborated instance
  // 'inst' writes.
  static const hldb::Any *getOverride(const hldb::RefInstance *inst, std::string_view param) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr || mt->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *mt->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == param)) return pa->getRhs();
    }
    return nullptr;
  }

  // The loop generate of test. The 'generate ... endgenerate' keywords
  // enclose it in a generate region (27.3), so a GenRegion is unwrapped.
  static const hldb::GenFor *getLoop() {
    const hldb::Module *const test = getModule("test");
    if (test == nullptr || test->getGenStmts() == nullptr || test->getGenStmts()->empty()) return nullptr;
    const hldb::Any *stmt = test->getGenStmts()->at(0);
    if (const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt)) stmt = region->getStmt();
    return any_cast<hldb::GenFor>(stmt);
  }

  // The instance named 'name' in 'stmt' or in a Begin it holds.
  static const hldb::RefInstance *findInstance(const hldb::Any *stmt, std::string_view name) {
    if (const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(stmt)) {
      return (inst->getName() == name) ? inst : nullptr;
    }
    const hldb::Begin *const block = any_cast<hldb::Begin>(stmt);
    if (block == nullptr || block->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const item : *block->getStmts()) {
      if (const hldb::RefInstance *const found = findInstance(item, name)) return found;
    }
    return nullptr;
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
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated scope";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }

  // Verifies 'name' is an overridable signed int of 'test' defaulting to
  // 'value'.
  static void ExpectOverridableInt(std::string_view name, std::string_view value) {
    const hldb::Module *const test = getModule("test");
    ASSERT_NE(test, nullptr);
    const hldb::Parameter *const p = getParam(test, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: '" << name << "' is in the parameter port list";
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << "'" << name << "' is declared 'int'";
    EXPECT_TRUE(it->getSigned()) << "6.11, Table 6-8: 'int' is a 32-bit signed integer";
    const hldb::ParamAssign *const pa = getParamAssign(test, name);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
    EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), value);
  }
};

// ---------------------------------------------------------------------------
// modules dut and test
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamElabTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"dut", "test"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: the empty port list of " << name << " is a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamElabTest, DutOverridesWidthAndDataBitsPerMask) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr) << "module 'dut' not found";
  ASSERT_NE(dut->getRefInstances(), nullptr);
  ASSERT_EQ(dut->getRefInstances()->size(), 1u) << "dut holds exactly one instance";
  const hldb::RefInstance *const inst = dut->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "t");
  const char *const names[] = {"Width", "DataBitsPerMask"};
  const char *const values[] = {"32", "8"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Constant *const value = any_cast<hldb::Constant>(getOverride(inst, names[i]));
    ASSERT_NE(value, nullptr) << "23.10.2.2: '." << names[i] << "(" << values[i] << ")' overrides by name";
    EXPECT_EQ(value->getDecompile(), values[i]);
  }
}

TEST_F(ParamElabTest, TestDeclaresTwoOverridableIntsAndMaskWidth) {
  const hldb::Module *const test = getModule("test");
  ASSERT_NE(test, nullptr) << "module 'test' not found";
  ASSERT_NE(test->getParameters(), nullptr);
  ASSERT_EQ(test->getParameters()->size(), 3u);
  ExpectOverridableInt("Width", "32");
  ExpectOverridableInt("DataBitsPerMask", "2");
  const hldb::Parameter *const mask = getParam(test, "MaskWidth");
  ASSERT_NE(mask, nullptr) << "parameter 'MaskWidth' not found";
  EXPECT_TRUE(mask->getLocalParam()) << "6.20.4: declared 'localparam'";
}

TEST_F(ParamElabTest, MaskWidthIsWidthDividedByDataBitsPerMask) {
  const hldb::Module *const test = getModule("test");
  ASSERT_NE(test, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(test, "MaskWidth");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const div = pa->getRhs<hldb::Operation>();
  ASSERT_NE(div, nullptr) << "unreduced, 'Width / DataBitsPerMask' is an Operation";
  EXPECT_EQ(div->getOpType(), vpiDivOp) << "11.4.3: a division";
  ASSERT_NE(div->getOperands(), nullptr);
  ASSERT_EQ(div->getOperands()->size(), 2u);
  const char *const names[] = {"Width", "DataBitsPerMask"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(div->getOperands()->at(i));
    ASSERT_NE(ref, nullptr) << "operand " << i;
    EXPECT_EQ(ref->getName(), names[i]);
    EXPECT_EQ(ref->getActual(), getParam(test, names[i])) << "'" << names[i] << "' is test's parameter";
  }
}

TEST_F(ParamElabTest, LoopGenerateIsBoundedByMaskWidth) {
  const hldb::Module *const test = getModule("test");
  ASSERT_NE(test, nullptr);
  ASSERT_NE(test->getGenStmts(), nullptr);
  ASSERT_EQ(test->getGenStmts()->size(), 1u) << "the module body holds one generate construct";
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr) << "27.4: 'for (i = 0; i < MaskWidth; i++)' is a loop generate construct";
  const hldb::Operation *const lt = loop->getCondition<hldb::Operation>();
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getOpType(), vpiLtOp) << "11.4.4";
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  const hldb::RefObj *const i = any_cast<hldb::RefObj>(lt->getOperands()->at(0));
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), "i");
  const hldb::RefObj *const mask = any_cast<hldb::RefObj>(lt->getOperands()->at(1));
  ASSERT_NE(mask, nullptr);
  EXPECT_EQ(mask->getName(), "MaskWidth");
  EXPECT_EQ(mask->getActual(), getParam(test, "MaskWidth")) << "'MaskWidth' is test's localparam";
  EXPECT_NE(findInstance(loop->getStmt(), "u"), nullptr) << "the loop block holds the instance 'u'";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamElabTest, DutIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: test is instantiated";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "dut");
  }
}

TEST_F(ParamElabTest, MaskWidthIsComputedFromTheOverrides) {
  if (m_design->getElaborated()) {
    const hldb::Module *const dut = hldb::findByDefName<hldb::Module>("dut", m_design->getTopModules());
    ASSERT_NE(dut, nullptr) << "the elaborated dut not found";
    const hldb::Module *const t = hldb::findByName<hldb::Module>("t", dut->getModules());
    ASSERT_NE(t, nullptr) << "the instance t is elaborated";
    ExpectElabParam(t, "Width", 32, "23.10.2.2: '.Width(32)'");
    ExpectElabParam(t, "DataBitsPerMask", 8, "23.10.2.2: '.DataBitsPerMask(8)'");
    ExpectElabParam(t, "MaskWidth", 4, "11.4.3: 32 / 8 = 4, from the overrides and not the defaults");
  }
}

TEST_F(ParamElabTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "A.4.2: 'i = 0' matches genvar_initialization, where 'genvar' is optional";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (undeclared
// genvar unreported) in the file header.
TEST_F(ParamElabTest, UndeclaredLoopIndexIsAnError) {
  bool reported = false;
  for (ErrorDefinition::ErrorType type : kUndeclaredNameErrors) {
    const ErrorDefinition::ErrorMap::const_iterator info = ErrorDefinition::getErrorInfoMap().find(type);
    if ((info != ErrorDefinition::getErrorInfoMap().end()) && (info->second.m_severity == ErrorDefinition::ERROR) &&
        (findError(type, "i") != nullptr)) {
      reported = true;
    }
  }
  EXPECT_TRUE(reported)
      << "27.4: The loop index shall be declared in a genvar declaration prior to its use in a loop generate scheme";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
