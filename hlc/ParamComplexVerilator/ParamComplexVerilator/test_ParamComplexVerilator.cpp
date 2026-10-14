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

// Validates the HLDB model built for tests/ParamComplexVerilator/dut.sv:
//
//   package pkg;
//     typedef struct packed {
//       logic a;
//       logic [2:0] b;
//     } complex_t;
//   endpackage
//
//   module top ();
//      localparam pkg::complex_t init_val = '{a: 1'b0, b: '1};
//      dut #(
//          .size ($bits(pkg::complex_t)),
//          .init ({init_val})
//        ) asgn0();
//   endmodule
//
//   module dut #(
//       parameter int size = 0,
//       parameter bit [size-1:0] init = '0
//     ) ();
//
//   endmodule // dut
//
// The point of the fixture is a struct-typed local parameter passed, inside a
// concatenation, to a parameter whose width is itself a parameter set from
// $bits of the same struct (IEEE 1800-2023 20.6.2, 23.10.2.2). The regression
// this file exists to catch is HLC losing the packed value or the width on
// the way down.
//
// Values: complex_t is a packed struct of 1 + 3 bits, so $bits(complex_t) is
// 4. In init_val, a is 1'b0 and b is '1, all ones of 3 bits. "The first
// member specified is the most significant" (7.2.1), so init_val = {a, b} =
// 4'b0111 = 7. In asgn0, size = 4 and init = {init_val} = 7.
//
// What is checked, and why:
//   Package pkg
//     - exactly 1 Typedef, complex_t, a packed struct (7.2.1) with exactly 2
//       members in source order: a, of logic with no range, and b, of logic
//       [2:0]
//   Module top
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - exactly 1 parameter, init_val, a localparam of type pkg::complex_t
//       (26.3), whose unreduced value is Operation vpiAssignmentPatternOp
//       (10.9.2) with the items a: 1'b0 and b: '1 in written order
//     - exactly 1 instance, asgn0, of dut, overriding by name (23.10.2.2)
//       size with the system function call $bits (20.6.2) and init with
//       Operation vpiConcatOp (11.4.12) over RefObj 'init_val', bound to top's
//       Parameter
//   Module dut
//     - '()' declares one null port (37.14 detail 10)
//     - exactly 2 parameters, size and init, both in the parameter port list
//       so overridable (6.20.1); size is a signed int (6.11) defaulting to 0;
//       init is a bit vector whose single range [size-1:0] has the left bound
//       Operation vpiSubOp over RefObj 'size', bound to dut's Parameter, and
//       Constant "1", and whose default is the Constant "'0"
//   Elaboration
//     - dut is instantiated, so top is the only top-level instance (23.3.1)
//     - top.init_val is reduced to 7
//     - top.asgn0.size is reduced to 4 and top.asgn0.init to 7
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the parameter values are checked only when the
// design is elaborated, on Design::getTopModules(); the unreduced values and
// overrides are asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG ($bits of a package-scoped type), not a defect in this
// test: HLC models the argument of '$bits(pkg::complex_t)' as a RefObj path
// whose elements bind, through vpiActual, to the Typedef complex_t, and its
// own model check then rejects both bindings with two [ERR:DB2038]
// "'vpiActual' property value on object is invalid" errors on a legal file.
// NoFatalSyntaxOrErrorDiagnostics is expected to fail until HLC is fixed; it
// is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - How HLC records the data type argument of $bits, and the parameter
//     override of an unelaborated instance, are model conventions; the
//     override is read from the instance's module typespec.
//   - The default of init, '0 with size = 0, is never used, since asgn0
//     overrides both parameters and dut is not a top-level module.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention.
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
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
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
#include <hldb/sys_func_call.h>
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

class ParamComplexVerilatorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamComplexVerilator.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages()); }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Typedef *getComplexT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("complex_t", pkg->getTypedefs());
  }

  static const hldb::TypespecMember *getMember(std::string_view name) {
    const hldb::Typedef *const td = getComplexT();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr || st->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>(name, st->getStruct()->getMembers());
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

  // Whether 'rts' names complex_t, directly or as the last element of a
  // package-scoped path.
  static bool namesComplexT(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getComplexT();
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr) return false;
    const hldb::Typespec *actual = rts->getActual();
    if ((actual == nullptr) && (rts->getPathElems() != nullptr) && !rts->getPathElems()->empty()) {
      const hldb::RefTypespec *const last = any_cast<hldb::RefTypespec>(rts->getPathElems()->back());
      if (last != nullptr) actual = last->getActual();
    }
    if (actual == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
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

  static const hldb::RefInstance *getAsgn0() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr || top->getRefInstances() == nullptr || top->getRefInstances()->size() != 1u) return nullptr;
    return top->getRefInstances()->at(0);
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
// package pkg;
// ---------------------------------------------------------------------------

TEST_F(ParamComplexVerilatorTest, ComplexTIsPackedStructOfAAndB) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u);
  const hldb::Typedef *const td = getComplexT();
  ASSERT_NE(td, nullptr) << "typedef 'complex_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'complex_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 2u);
  const char *const names[] = {"a", "b"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TypespecMember *const m = st->getStruct()->getMembers()->at(i);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), names[i]) << "member " << i << ", in source order";
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "'" << names[i] << "' is declared 'logic'";
    if (i == 0) {
      EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << "'a' has no range";
    } else {
      ASSERT_NE(lt->getRanges(), nullptr);
      ASSERT_EQ(lt->getRanges()->size(), 1u);
      const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
      ASSERT_NE(left, nullptr);
      EXPECT_EQ(left->getDecompile(), "2") << "'b' is declared 'logic [2:0]'";
      const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
      ASSERT_NE(right, nullptr);
      EXPECT_EQ(right->getDecompile(), "0");
    }
  }
}

// ---------------------------------------------------------------------------
// modules top and dut
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamComplexVerilatorTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"top", "dut"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: the empty port list of " << name << " is a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamComplexVerilatorTest, InitValIsLocalComplexTPattern) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(top, "init_val");
  ASSERT_NE(p, nullptr) << "parameter 'init_val' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: declared 'localparam'";
  EXPECT_TRUE(namesComplexT(p->getTypespec())) << "26.3: 'pkg::complex_t' names the package's complex_t";
  const hldb::ParamAssign *const pa = getParamAssign(top, "init_val");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a structure assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u);
  const char *const keys[] = {"a", "b"};
  const char *const values[] = {"1'b0", "'1"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i << " is a 'member: value' item";
    const hldb::RefObj *const tag = item->getTag<hldb::RefObj>();
    ASSERT_NE(tag, nullptr) << "10.9.2: item " << i << " is keyed by a member name";
    EXPECT_EQ(tag->getName(), keys[i]);
    ASSERT_NE(getMember(keys[i]), nullptr);
    EXPECT_EQ(tag->getActual(), getMember(keys[i]))
        << "10.9.2: item " << i << " is keyed by the member " << keys[i] << " of complex_t";
    const hldb::Constant *const value = item->getPattern<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "item " << i;
    EXPECT_EQ(value->getDecompile(), values[i]) << "item " << i;
  }
}

TEST_F(ParamComplexVerilatorTest, Asgn0OverridesSizeWithBitsOfComplexT) {
  const hldb::RefInstance *const inst = getAsgn0();
  ASSERT_NE(inst, nullptr) << "top holds exactly one instance";
  EXPECT_EQ(inst->getName(), "asgn0");
  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(getOverride(inst, "size"));
  ASSERT_NE(bits, nullptr) << "23.10.2.2: '.size ($bits(pkg::complex_t))' overrides with a system function call";
  EXPECT_EQ(bits->getName(), "$bits") << "20.6.2";
  ASSERT_NE(bits->getArguments(), nullptr);
  EXPECT_EQ(bits->getArguments()->size(), 1u) << "20.6.2: $bits takes one argument";
}

TEST_F(ParamComplexVerilatorTest, Asgn0OverridesInitWithConcatOfInitVal) {
  const hldb::RefInstance *const inst = getAsgn0();
  ASSERT_NE(inst, nullptr);
  const hldb::Operation *const concat = any_cast<hldb::Operation>(getOverride(inst, "init"));
  ASSERT_NE(concat, nullptr) << "23.10.2.2: '.init ({init_val})' overrides with a concatenation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(concat->getOperands()->at(0));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "init_val");
  EXPECT_EQ(ref->getActual(), getParam(getModule("top"), "init_val")) << "'init_val' is top's localparam";
}

TEST_F(ParamComplexVerilatorTest, DutDeclaresIntSizeAndBitVectorInit) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr) << "module 'dut' not found";
  ASSERT_NE(dut->getParameters(), nullptr);
  ASSERT_EQ(dut->getParameters()->size(), 2u);
  const hldb::Parameter *const size = getParam(dut, "size");
  ASSERT_NE(size, nullptr);
  EXPECT_FALSE(size->getLocalParam()) << "6.20.1: 'size' is in the parameter port list";
  ASSERT_NE(size->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = size->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'size' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11, Table 6-8: 'int' is a 32-bit signed integer";
  const hldb::ParamAssign *const sizeAssign = getParamAssign(dut, "size");
  ASSERT_NE(sizeAssign, nullptr);
  ASSERT_NE(sizeAssign->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(sizeAssign->getRhs<hldb::Constant>()->getDecompile(), "0");

  const hldb::Parameter *const init = getParam(dut, "init");
  ASSERT_NE(init, nullptr);
  EXPECT_FALSE(init->getLocalParam()) << "6.20.1: 'init' is in the parameter port list";
  ASSERT_NE(init->getTypespec(), nullptr);
  const hldb::BitTypespec *const bt = init->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr) << "'init' is declared 'bit [size-1:0]'";
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  const hldb::Operation *const left = bt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "unreduced, the left bound 'size-1' is an expression";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const sizeRef = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(sizeRef, nullptr);
  EXPECT_EQ(sizeRef->getName(), "size");
  EXPECT_EQ(sizeRef->getActual(), size) << "6.20.1: a parameter can depend on earlier parameters";
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = bt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
  const hldb::ParamAssign *const initAssign = getParamAssign(dut, "init");
  ASSERT_NE(initAssign, nullptr);
  ASSERT_NE(initAssign->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(initAssign->getRhs<hldb::Constant>()->getDecompile(), "'0");
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamComplexVerilatorTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: dut is instantiated";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamComplexVerilatorTest, InitValReducesToSeven) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    ExpectElabParam(top, "init_val", 7, "7.2.1: a (0) is the most significant bit and b ('1) the low 3 bits: 4'b0111");
  }
}

TEST_F(ParamComplexVerilatorTest, Asgn0ReceivesSizeFourAndInitSeven) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::Module *const asgn0 = hldb::findByName<hldb::Module>("asgn0", top->getModules());
    ASSERT_NE(asgn0, nullptr) << "the instance asgn0 is elaborated";
    ExpectElabParam(asgn0, "size", 4, "20.6.2: $bits(complex_t) = 1 + 3");
    ExpectElabParam(asgn0, "init", 7, "23.10.2.2: '{init_val}' = 4'b0111");
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG ($bits of a
// package-scoped type) in the file header.
TEST_F(ParamComplexVerilatorTest, NoFatalSyntaxOrErrorDiagnostics) {
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
