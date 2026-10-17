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

// Validates the HLDB model built for tests/ParamConcat/dut.sv:
//
//   module ibex_csr #(
//       parameter bit [1:0] ResetValue = '0) ();
//
//   endmodule;
//
//   module top();
//
//
//     typedef struct packed {
//       logic mie;
//       logic mpie;
//     } status_t;
//
//     localparam status_t MSTATUS_RST_VAL = '{mpie:  1'b1,
//                                             mie: 1'b0};
//     ibex_csr #(
//       .ResetValue ({MSTATUS_RST_VAL})
//     ) u_mstatus_csr();
//   endmodule
//
// The point of the fixture is a packed-struct local parameter whose
// assignment pattern names the members out of declaration order, passed
// inside a concatenation to a bit-vector parameter of an instance (IEEE
// 1800-2023 10.9.2, 23.10.2.2). The regression this file exists to catch is
// HLC placing the pattern's values by written position instead of by member
// name.
//
// Values: "The first member specified is the most significant" (7.2.1), so
// MSTATUS_RST_VAL = {mie, mpie} = {1'b0, 1'b1} = 2'b01 = 1, and
// u_mstatus_csr's ResetValue = {MSTATUS_RST_VAL} = 1. Placing the values by
// written position would give 2'b10 = 2 instead.
//
// What is checked, and why:
//   Module ibex_csr
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - exactly 1 parameter, ResetValue, in the parameter port list so
//       overridable (6.20.1), a bit vector with the single range [1:0], whose
//       default is the Constant "'0"
//   Module top
//     - '()' declares one null port (37.14 detail 10)
//     - exactly 1 Typedef, status_t, a packed struct (7.2.1) with exactly 2
//       members in source order, mie then mpie
//     - exactly 1 parameter, MSTATUS_RST_VAL, a localparam of type status_t,
//       whose unreduced value is Operation vpiAssignmentPatternOp (10.9.2)
//       with the items mpie: 1'b1 and mie: 1'b0 in written order
//     - exactly 1 instance, u_mstatus_csr, of ibex_csr, overriding
//       ResetValue by name (23.10.2.2) with Operation vpiConcatOp (11.4.12)
//       over RefObj 'MSTATUS_RST_VAL', bound to top's Parameter
//   Elaboration
//     - ibex_csr is instantiated, so top is the only top-level instance
//       (23.3.1)
//     - top.MSTATUS_RST_VAL is reduced to 1, and so is
//       top.u_mstatus_csr.ResetValue
//   Diagnostics
//     - the ';' after the first 'endmodule' is a null
//       package_or_generate_item_declaration (A.1.11), which is a
//       package_item and so a description at compilation-unit scope
//       (A.1.2); the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the parameter values are checked only when the
// design is elaborated, on Design::getTopModules(); the unreduced pattern
// and override are asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - How HLC records the parameter override of an unelaborated instance is a
//     model convention; it is read from the instance's module typespec.
//   - Whether HLC keeps any object for the stray ';' is a tool convention;
//     only the absence of a diagnostic is asserted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {

class ParamConcatTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamConcat.hlc"}); }
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

  static const hldb::Typedef *getStatusT() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("status_t", top->getTypedefs());
  }

  static const hldb::TypespecMember *getMember(std::string_view name) {
    const hldb::Typedef *const td = getStatusT();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr || st->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>(name, st->getStruct()->getMembers());
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
};

// ---------------------------------------------------------------------------
// module ibex_csr
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamConcatTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"ibex_csr", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: the empty port list of " << name << " is a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamConcatTest, IbexCsrDeclaresOverridableTwoBitResetValue) {
  const hldb::Module *const csr = getModule("ibex_csr");
  ASSERT_NE(csr, nullptr) << "module 'ibex_csr' not found";
  ASSERT_NE(csr->getParameters(), nullptr);
  ASSERT_EQ(csr->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(csr, "ResetValue");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: 'ResetValue' is in the parameter port list";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::BitTypespec *const bt = p->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr) << "'ResetValue' is declared 'bit [1:0]'";
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  const hldb::Constant *const left = bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "1");
  const hldb::Constant *const right = bt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
  const hldb::ParamAssign *const pa = getParamAssign(csr, "ResetValue");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "'0");
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamConcatTest, StatusTIsPackedStructOfMieThenMpie) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 1u);
  const hldb::Typedef *const td = getStatusT();
  ASSERT_NE(td, nullptr) << "typedef 'status_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'status_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 2u);
  const char *const names[] = {"mie", "mpie"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TypespecMember *const m = st->getStruct()->getMembers()->at(i);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), names[i]) << "member " << i << ", in source order";
    ASSERT_NE(m->getTypespec(), nullptr);
    EXPECT_NE(m->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'" << names[i] << "' is 'logic'";
  }
}

TEST_F(ParamConcatTest, MstatusRstValIsLocalPatternKeyedOutOfOrder) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(top, "MSTATUS_RST_VAL");
  ASSERT_NE(p, nullptr) << "parameter 'MSTATUS_RST_VAL' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: declared 'localparam'";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::Typedef *const td = getStatusT();
  ASSERT_NE(td, nullptr);
  const hldb::TypedefTypespec *const viaTypedef = p->getTypespec()->getActual<hldb::TypedefTypespec>();
  const bool isStatusT =
      ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
      ((td->getAlias() != nullptr) && (p->getTypespec()->getActual() == td->getAlias()->getActual()));
  EXPECT_TRUE(isStatusT) << "'MSTATUS_RST_VAL' is declared 'status_t'";
  const hldb::ParamAssign *const pa = getParamAssign(top, "MSTATUS_RST_VAL");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a structure assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u);
  const char *const keys[] = {"mpie", "mie"};
  const char *const values[] = {"1'b1", "1'b0"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i << " is a 'member: value' item";
    const hldb::RefObj *const tag = item->getTag<hldb::RefObj>();
    ASSERT_NE(tag, nullptr) << "10.9.2: item " << i << " is keyed by a member name";
    EXPECT_EQ(tag->getName(), keys[i]);
    ASSERT_NE(getMember(keys[i]), nullptr);
    EXPECT_EQ(tag->getActual(), getMember(keys[i]))
        << "10.9.2: item " << i << ", as written, is keyed by the member " << keys[i] << " of status_t";
    const hldb::Constant *const value = item->getPattern<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "item " << i;
    EXPECT_EQ(value->getDecompile(), values[i]) << "item " << i;
  }
}

TEST_F(ParamConcatTest, UMstatusCsrOverridesResetValueWithConcat) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u) << "top holds exactly one instance";
  const hldb::RefInstance *const inst = top->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "u_mstatus_csr");
  const hldb::Operation *const concat = any_cast<hldb::Operation>(getOverride(inst, "ResetValue"));
  ASSERT_NE(concat, nullptr) << "23.10.2.2: '.ResetValue ({MSTATUS_RST_VAL})' overrides with a concatenation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(concat->getOperands()->at(0));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "MSTATUS_RST_VAL");
  EXPECT_EQ(ref->getActual(), getParam(top, "MSTATUS_RST_VAL")) << "'MSTATUS_RST_VAL' is top's localparam";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamConcatTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: ibex_csr is instantiated";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamConcatTest, MstatusRstValReducesToOneByMemberName) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    ExpectElabParam(top, "MSTATUS_RST_VAL", 1,
                    "7.2.1, 10.9.2: mie (1'b0) is the most significant member and mpie (1'b1) the least: 2'b01");
  }
}

TEST_F(ParamConcatTest, UMstatusCsrReceivesResetValueOne) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::Module *const csr = hldb::findByName<hldb::Module>("u_mstatus_csr", top->getModules());
    ASSERT_NE(csr, nullptr) << "the instance u_mstatus_csr is elaborated";
    ExpectElabParam(csr, "ResetValue", 1, "23.10.2.2: '{MSTATUS_RST_VAL}' = 2'b01");
  }
}

TEST_F(ParamConcatTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "A.1.11: the ';' after 'endmodule' is a null package_or_generate_item_declaration";
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
