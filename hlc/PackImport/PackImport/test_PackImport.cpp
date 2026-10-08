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

// Validates the HLDB model built for tests/PackImport/dut.sv:
//
//   package ibex_pkg;
//   typedef enum logic [6:0] {
//     OPCODE_LOAD     = 7'h03,
//     OPCODE_LUI      = 7'h37
//   } opcode_e;
//   endpackage // ibex_pkg
//
//   package ibex_tracer_pkg;
//   import ibex_pkg::*;
//   parameter logic [31:0] INSN_LUI = { 25'h?, {OPCODE_LUI  }};
//   endpackage
//
//   module top(input clk_i, output logic [31:0] o1);
//   import ibex_pkg::*;
//   endmodule // top
//
// The point of the fixture is a package parameter whose value concatenates a
// literal of '?' digits with an enum name reached through a wildcard import
// of another package (IEEE 1800-2023 26.3). In a number '?' is an alternative
// for 'z' (5.7.1), so 25'h? is 25 high-impedance bits. The regression this
// file exists to catch is HLC failing to bind the imported enum name inside
// the parameter value, or mis-evaluating the 4-state concatenation.
//
// What is checked, and why:
//   Package ibex_pkg
//     - exactly 1 Typedef, 'opcode_e', whose alias is an EnumTypespec (6.19)
//       with base type logic [6:0] and exactly 2 names in source order:
//       OPCODE_LOAD with the value Constant "7'h03" and OPCODE_LUI with
//       "7'h37"
//   Package ibex_tracer_pkg
//     - records exactly 1 import, the wildcard import of ibex_pkg: an
//       ImportTypespec named "ibex_pkg" whose item is "*"
//     - exactly 1 parameter, INSN_LUI, of type logic [31:0]; a local
//       parameter, because in a package the keyword 'parameter' is a synonym
//       for 'localparam' (6.20.4)
//     - unreduced, its value is Operation vpiConcatOp (11.4.12) with exactly
//       2 operands: the Constant "25'h?", then a nested concatenation with
//       the single operand RefObj 'OPCODE_LUI', bound through the wildcard
//       import to ibex_pkg's EnumConst
//     - a parameter value is a constant expression (6.20.2), so on an
//       elaborated design it is reduced to 32 bits: 25 'z' bits followed by
//       7'h37 = 0110111
//   Module top (23.2)
//     - exactly 2 ports, clk_i (input) then o1 (output), with port indexes 0
//       and 1 (37.14 detail 9). Following 23.2.2.3, clk_i writes no data
//       type and is an input, so it is a net of the default net type wire
//       (22.8) and of the default data type logic; o1 writes the explicit
//       data type logic [31:0] and is an output, so it is a variable. Each is
//       its port's low connection
//     - the module records exactly 1 import, the wildcard import of ibex_pkg
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: INSN_LUI is checked in both modes: unreduced on
// the package definition in Design::getAllPackages(), and reduced, only when
// the design is elaborated, on the elaborated package in
// Design::getTopPackages().
//
// KNOWN COMPILER BUG (port index not set), not a defect in this test: HLC
// leaves vpiPortIndex at 0 for every port, although 37.14 detail 9 says the
// port index gives the port order. TopPortIndexesFollowDeclarationOrder is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - The radix HLC uses to write the reduced 4-state value is a tool
//     convention; the value is compared bit by bit, so it is read only from a
//     binary form.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; packages are looked up
//     by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The '// ibex_pkg' and '// top' comments are not end labels (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/import_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class PackImportTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackImport.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::EnumConst *getOpcodeLui() {
    const hldb::Package *const pkg = getPkg("ibex_pkg");
    if (pkg == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("opcode_e", pkg->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr || et->getEnum() == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>("OPCODE_LUI", et->getEnum()->getEnumConsts());
  }

  // The ParamAssign in 'pkg' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Package *pkg, std::string_view name) {
    if (pkg == nullptr || pkg->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *pkg->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  // The imports 'scope' records.
  static std::vector<const hldb::ImportTypespec *> getImports(const hldb::Instance *scope) {
    std::vector<const hldb::ImportTypespec *> imports;
    if (scope == nullptr || scope->getTypespecs() == nullptr) return imports;
    for (const hldb::Typespec *const ts : *scope->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
    return imports;
  }

  // Verifies 'scope' records exactly one import, the wildcard import of
  // ibex_pkg.
  static void ExpectOnlyWildcardImportOfIbexPkg(const hldb::Instance *scope, std::string_view what) {
    ASSERT_NE(scope, nullptr) << what << " not found";
    const std::vector<const hldb::ImportTypespec *> imports = getImports(scope);
    ASSERT_EQ(imports.size(), 1u) << what << " writes exactly one import";
    EXPECT_EQ(imports[0]->getName(), "ibex_pkg") << "26.3: " << what << " imports from ibex_pkg";
    const hldb::Constant *const item = imports[0]->getItem();
    ASSERT_NE(item, nullptr) << "the import names what it imports";
    EXPECT_EQ(item->getDecompile(), "*") << "26.3: 'import ibex_pkg::*;' is a wildcard import";
  }

  // The bits of a binary Constant, lowercased, with '?' read as 'z' (5.7.1);
  // empty if the Constant is not written in binary.
  static std::string binaryBits(const hldb::Constant *c) {
    const std::string_view text = c->getDecompile();
    const std::string_view::size_type tick = text.find('\'');
    if (tick == std::string_view::npos || tick + 1 >= text.size()) return "";
    std::string_view::size_type pos = tick + 1;
    if ((text[pos] == 's') || (text[pos] == 'S')) ++pos;
    if ((pos >= text.size()) || ((text[pos] != 'b') && (text[pos] != 'B'))) return "";
    std::string bits;
    for (const char ch : text.substr(pos + 1)) {
      if (ch == '_') continue;
      const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      bits.push_back((lower == '?') ? 'z' : lower);
    }
    return bits;
  }
};

// ---------------------------------------------------------------------------
// package ibex_pkg; typedef enum logic [6:0] { ... } opcode_e; endpackage
// ---------------------------------------------------------------------------

TEST_F(PackImportTest, OpcodeEIsLogic6To0EnumOfTwoOpcodes) {
  const hldb::Package *const pkg = getPkg("ibex_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'ibex_pkg' not found";
  EXPECT_EQ(pkg->getEndLabel(), "") << "'// ibex_pkg' after endpackage is a comment, not a label";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'opcode_e' is ibex_pkg's only typedef";
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("opcode_e", pkg->getTypedefs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'opcode_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "the base type is 'logic [6:0]'";
  ASSERT_NE(base->getRanges(), nullptr);
  ASSERT_EQ(base->getRanges()->size(), 1u);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);
  const char *const names[] = {"OPCODE_LOAD", "OPCODE_LUI"};
  const char *const values[] = {"7'h03", "7'h37"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), names[i]) << "enum name " << i << ", in source order";
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getDecompile(), values[i]);
  }
}

// ---------------------------------------------------------------------------
// package ibex_tracer_pkg; import ibex_pkg::*; parameter ... INSN_LUI = ...;
// ---------------------------------------------------------------------------

TEST_F(PackImportTest, TracerPkgImportsAllOfIbexPkg) {
  ExpectOnlyWildcardImportOfIbexPkg(getPkg("ibex_tracer_pkg"), "package ibex_tracer_pkg");
}

TEST_F(PackImportTest, InsnLuiIsLocalLogic31To0) {
  const hldb::Package *const pkg = getPkg("ibex_tracer_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'ibex_tracer_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u) << "'INSN_LUI' is the package's only parameter";
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("INSN_LUI", pkg->getParameters());
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'INSN_LUI' is declared 'logic [31:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "31");
}

TEST_F(PackImportTest, InsnLuiConcatenatesZBitsWithImportedOpcodeLui) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg("ibex_tracer_pkg"), "INSN_LUI");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'INSN_LUI'";
  const hldb::Operation *const concat = pa->getRhs<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12: a concatenation";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u) << "'25'h?' and '{OPCODE_LUI}'";
  const hldb::Constant *const zbits = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(zbits, nullptr);
  EXPECT_EQ(zbits->getDecompile(), "25'h?") << "5.7.1: '?' is an alternative for 'z' in a number";
  const hldb::Operation *const inner = any_cast<hldb::Operation>(concat->getOperands()->at(1));
  ASSERT_NE(inner, nullptr) << "'{OPCODE_LUI}' is a nested concatenation";
  EXPECT_EQ(inner->getOpType(), vpiConcatOp);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 1u);
  const hldb::RefObj *const lui = any_cast<hldb::RefObj>(inner->getOperands()->at(0));
  ASSERT_NE(lui, nullptr);
  EXPECT_EQ(lui->getName(), "OPCODE_LUI");
  ASSERT_NE(getOpcodeLui(), nullptr);
  EXPECT_EQ(lui->getActual(), getOpcodeLui()) << "26.3: OPCODE_LUI is imported from ibex_pkg by 'import ibex_pkg::*'";
}

TEST_F(PackImportTest, InsnLuiReducesToZBitsThenOpcodeLui) {
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const pa =
        getParamAssign(hldb::findByName<hldb::Package>("ibex_tracer_pkg", m_design->getTopPackages()), "INSN_LUI");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'INSN_LUI' in the elaborated package";
    const hldb::Constant *const value = pa->getRhs<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "6.20.2: a parameter value is reduced to a Constant";
    EXPECT_EQ(binaryBits(value), std::string(25, 'z') + "0110111")
        << "5.7.1, 11.4.12: 25 'z' bits, then 7'h37; the reduced value was '" << value->getDecompile() << "'";
  }
}

// ---------------------------------------------------------------------------
// module top(input clk_i, output logic [31:0] o1); import ibex_pkg::*;
// ---------------------------------------------------------------------------

TEST_F(PackImportTest, TopHasInputNetClkIAndOutputVariableO1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_EQ(top->getEndLabel(), "") << "'// top' after endmodule is a comment, not a label";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  const hldb::Port *const clk = top->getPorts()->at(0);
  const hldb::Port *const o1 = top->getPorts()->at(1);
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(o1, nullptr);
  EXPECT_EQ(clk->getName(), "clk_i");
  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(o1->getName(), "o1");
  EXPECT_EQ(o1->getDirection(), vpiOutput);
  const hldb::Net *const clkNet = hldb::findByName<hldb::Net>("clk_i", top->getNets());
  ASSERT_NE(clkNet, nullptr) << "23.2.2.3: an input port with no port kind is a net";
  EXPECT_EQ(clkNet->getNetType(), vpiWire) << "22.8: the default net type is wire";
  ASSERT_NE(clkNet->getTypespec(), nullptr);
  const hldb::LogicTypespec *const clkType = clkNet->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(clkType, nullptr) << "23.2.2.3: an omitted data type defaults to logic";
  EXPECT_TRUE(clkType->getRanges() == nullptr || clkType->getRanges()->empty()) << "'clk_i' is a single bit";
  const hldb::Variable *const o1Var = hldb::findByName<hldb::Variable>("o1", top->getVariables());
  ASSERT_NE(o1Var, nullptr) << "23.2.2.3: an output port with an explicit data type is a variable";
  ASSERT_NE(clk->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(clk->getLowConn<hldb::RefObj>()->getActual(), clkNet);
  ASSERT_NE(o1->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(o1->getLowConn<hldb::RefObj>()->getActual(), o1Var);
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (port index
// not set) in the file header.
TEST_F(PackImportTest, TopPortIndexesFollowDeclarationOrder) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 2u);
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_NE(top->getPorts()->at(i), nullptr);
    EXPECT_EQ(top->getPorts()->at(i)->getPortIndex(), static_cast<int32_t>(i))
        << "37.14 detail 9: vpiPortIndex gives the port order, and the first port has index 0";
  }
}

TEST_F(PackImportTest, TopImportsAllOfIbexPkg) { ExpectOnlyWildcardImportOfIbexPkg(getTop(), "module top"); }

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackImportTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

TEST_F(PackImportTest, NoFatalSyntaxOrErrorDiagnostics) {
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
