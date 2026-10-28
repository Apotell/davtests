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

// Validates the HLDB model built for tests/ParamRef/dut.sv:
//
//   package tl_main_pkg;
//      localparam int ADDR_SPACE_DEBUG_MEM = 1;
//   endpackage
//
//   package dm;
//      localparam int HaltAddress = 10;
//      localparam logic [31:0] ExceptionAddress = HaltAddress + 2;
//   endpackage : dm
//
//   module top_earlgrey;
//     import tl_main_pkg::*;
//
//     rv_core_ibex #(
//        .DmExceptionAddr(ADDR_SPACE_DEBUG_MEM + dm::ExceptionAddress)
//     ) u_rv_core_ibex();
//   endmodule
//
//   module rv_core_ibex;
//      parameter int unsigned DmExceptionAddr = 3;
//      ibex_core #(
//         .DmExceptionAddr(DmExceptionAddr)
//     ) u_core();
//   endmodule
//
//   module ibex_core;
//      parameter int unsigned DmExceptionAddr = 4;
//   endmodule // ibex_core
//
// The point of the fixture is how the names inside a parameter override are
// resolved. The override of u_rv_core_ibex adds a name made visible by a
// wildcard import (26.3) to a name written with the package scope resolution
// operator '::' (26.3), and the override of u_core passes rv_core_ibex's own
// parameter down to a parameter of the same name in ibex_core (23.10.2.2).
// 6.20.2 allows exactly this: "A value parameter (parameter, localparam, or
// specparam) can only be set to an expression of literals, value parameters
// or local parameters, genvars, enumerated names, or a constant function of
// these. Package references are allowed." The regression this file exists to
// catch is HLC binding a name to the wrong declaration -- in particular
// binding the RHS 'DmExceptionAddr' of '.DmExceptionAddr(DmExceptionAddr)' to
// ibex_core's parameter instead of rv_core_ibex's -- or letting a default win
// over an override.
//
// Values: dm::ExceptionAddress = HaltAddress + 2 = 10 + 2 = 12, a 32-bit
// unsigned logic. u_rv_core_ibex's DmExceptionAddr = ADDR_SPACE_DEBUG_MEM +
// dm::ExceptionAddress = 1 + 12 = 13, replacing the default 3 (23.10.3).
// u_core's DmExceptionAddr = u_rv_core_ibex's DmExceptionAddr = 13, replacing
// the default 4.
//
// What is checked, and why:
//   Package tl_main_pkg
//     - exactly 1 parameter, ADDR_SPACE_DEBUG_MEM, local (6.20.1, 6.20.4), of
//       the signed type int (6.11.3) with no explicitly defined range (37.28
//       detail 5), whose value is the unsized literal 1. 5.7.1: "Simple
//       decimal numbers without the size and the base format shall be
//       treated as signed integers", and an unsized number "shall be at least
//       32" bits
//   Package dm
//     - exactly 2 parameters, HaltAddress and ExceptionAddress, in that
//       order, both local
//     - HaltAddress is a signed int with the value 10, a signed literal of at
//       least 32 bits
//     - ExceptionAddress is an unsigned logic [31:0] (6.11.3) whose unreduced
//       value is Operation vpiAddOp over the RefObj HaltAddress, bound to
//       dm's own HaltAddress, and the literal 2, in that order (11.4.3)
//     - the end label is dm (26.2)
//     - the LHS of every ParamAssign is bound to the parameter it names
//   Module top_earlgrey
//     - records exactly 1 import, the wildcard import of tl_main_pkg (26.3)
//     - declares no parameter, and holds exactly 1 instance, u_rv_core_ibex
//       of rv_core_ibex
//     - u_rv_core_ibex writes exactly 1 override, by name (37.28
//       vpiConnByName), whose name is bound to rv_core_ibex's DmExceptionAddr
//       (23.10.2.2) and whose value is Operation vpiAddOp over
//       ADDR_SPACE_DEBUG_MEM, bound through the wildcard import to
//       tl_main_pkg's parameter, and dm::ExceptionAddress, bound to dm's
//       parameter, in that order
//   Module rv_core_ibex and module ibex_core
//     - each declares exactly 1 parameter, DmExceptionAddr, overridable
//       (neither module has a parameter port list, 6.20.1), of type
//       'int unsigned', so unsigned (6.11.3), with no explicitly defined
//       range, defaulting to the signed literals 3 and 4
//     - rv_core_ibex holds exactly 1 instance, u_core of ibex_core, whose
//       only override is by name, names ibex_core's DmExceptionAddr and
//       passes rv_core_ibex's own DmExceptionAddr. The value is written in
//       rv_core_ibex, and 26.3 says: "For a reference to an identifier other
//       than function or task call, the locally visible identifiers defined
//       at the point of the reference in the current scope shall be
//       searched."
//   All modules
//     - none has a port (A.1.2: module_ansi_header with the optional
//       list_of_port_declarations omitted), and no instance connects one
//       (23.3.2)
//   Elaboration
//     - dm::ExceptionAddress reduces to the 32-bit value 12
//     - u_rv_core_ibex's DmExceptionAddr is 13 and u_core's is 13, both 32
//       bits wide. 6.20.2: "A parameter with a type specification, but with
//       no range specification, shall be of the type specified."
//     - top_earlgrey is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values are checked only when the
// design is elaborated, on Design::getTopPackages() and
// Design::getTopModules(); the unreduced values and bindings are asserted on
// the definitions in every run.
//
// Model convention: 37.28 detail 4 says "vpiLhs from a param assign object
// shall return a handle to the overridden value parameter or type
// parameter." HLC holds the LHS as a RefObj naming the parameter and bound
// through getActual() to that Parameter. The tests follow the RefObj and
// assert the Parameter it is bound to.
//
// Model convention: a package-scoped name 'dm::ExceptionAddress' is one RefObj
// bound to the package item, whose path elements are a RefObj for the package
// prefix and a RefObj for the item. PackageScopedNameBindsThroughDm asserts
// what each element is bound to.
//
// What is NOT checked, and why:
//   - Which vpiConstType an unsized decimal literal such as 1 carries. Annex
//     K (vpi_user.h) defines both vpiDecConst ("decimal integer") and
//     vpiIntConst ("integer constant (1364-2001)") and the standard does not
//     say which one an unsized decimal literal uses. The literal text, its
//     signedness and its width are asserted instead.
//   - The exact width of an unsized literal. 5.7.1 only requires "at least
//     32", so a lower bound is asserted.
//   - Whether an unsized literal's type is int or integer. 5.7.1 calls it a
//     signed integer without naming the type, so either is accepted and its
//     signedness is asserted.
//   - The result type of the override 'ADDR_SPACE_DEBUG_MEM +
//     dm::ExceptionAddress', unsigned by 11.8.1 because dm::ExceptionAddress
//     is unsigned. The unreduced Operation is checked for its operator and
//     operands, and the value it reduces to, 13, is the same under either
//     signedness; OverriddenDmExceptionAddrIsThirteen carries the static
//     evidence of the reduction.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
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
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamRefTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamRef.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The name the LHS of 'pa' writes, whatever node holds it.
  static std::string_view getLhsName(const hldb::ParamAssign *pa) {
    if (const hldb::RefObj *const ref = pa->getLhs<hldb::RefObj>()) return ref->getName();
    if (const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>()) return p->getName();
    return {};
  }

  // The declaration the LHS of 'pa' is bound to.
  static const hldb::Any *getLhsActual(const hldb::ParamAssign *pa) {
    if (const hldb::RefObj *const ref = pa->getLhs<hldb::RefObj>()) return ref->getActual();
    return pa->getLhs();
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      if (getLhsName(pa) == name) return pa;
    }
    return nullptr;
  }

  static const hldb::RefInstance *getInstance(std::string_view module, std::string_view name) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, m->getRefInstances());
  }

  // The ModuleTypespec an unelaborated instance refers to.
  static const hldb::ModuleTypespec *getInstanceType(const hldb::RefInstance *inst) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    return inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  }

  // The overrides the unelaborated instance writes, in source order.
  static const std::vector<hldb::ParamAssign *> *getOverrides(const hldb::RefInstance *inst) {
    const hldb::ModuleTypespec *const mt = getInstanceType(inst);
    if (mt == nullptr) return nullptr;
    return mt->getParamAssigns();
  }

  // The elaborated instance at 'path' below the elaborated top_earlgrey.
  static const hldb::Module *getElabInstance(const std::vector<std::string_view> &path) {
    const hldb::Module *m = hldb::findByDefName<hldb::Module>("top_earlgrey", m_design->getTopModules());
    for (std::string_view name : path) {
      if (m == nullptr) return nullptr;
      m = hldb::findByName<hldb::Module>(name, m->getModules());
    }
    return m;
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

  // Verifies 'p' is declared 'int', signed or not as 'isSigned' says.
  static void ExpectIntType(const hldb::Parameter *p, bool isSigned, std::string_view why) {
    ASSERT_NE(p, nullptr);
    ASSERT_NE(p->getTypespec(), nullptr) << p->getName() << " has a declared type";
    const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << p->getName() << " is declared 'int'";
    EXPECT_EQ(it->getSigned(), isSigned) << why;
    EXPECT_TRUE(p->getRanges() == nullptr || p->getRanges()->empty())
        << "37.28 detail 5: " << p->getName() << " has no explicitly defined range";
  }

  // Verifies the ParamAssign of 'name' in 'scope' is bound to the parameter
  // and assigns it the unsized decimal literal 'text'.
  static void ExpectDefault(const hldb::Scope *scope, std::string_view name, std::string_view text) {
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "'";
    ASSERT_NE(getParam(scope, name), nullptr);
    EXPECT_EQ(getLhsActual(pa), getParam(scope, name)) << "37.28 detail 4: the LHS is '" << name << "' itself";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "the value is the literal " << text;
    EXPECT_EQ(c->getDecompile(), text);
    EXPECT_GE(c->getSize(), 32) << "5.7.1: an unsized number has at least 32 bits";
    ASSERT_NE(c->getTypespec(), nullptr) << "the literal " << text << " carries its type";
    const hldb::Typespec *const ts = c->getTypespec()->getActual();
    ASSERT_NE(ts, nullptr) << "the type of the literal " << text << " is resolved";
    bool isSigned = false;
    if (const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(ts)) {
      isSigned = it->getSigned();
    } else if (const hldb::IntegerTypespec *const integer = any_cast<hldb::IntegerTypespec>(ts)) {
      isSigned = integer->getSigned();
    } else {
      ADD_FAILURE() << "5.7.1: the literal " << text << " is an integer";
    }
    EXPECT_TRUE(isSigned) << "5.7.1: \"Simple decimal numbers without the size and the base format shall be "
                             "treated as signed integers\"";
  }

  // Verifies the parameter 'name' of the elaborated 'scope' reduces to the
  // 32-bit value 'expected'.
  static void ExpectElabValue(const hldb::Scope *scope, std::string_view name, uint64_t expected,
                              std::string_view why) {
    ASSERT_NE(scope, nullptr) << "the elaborated scope is missing";
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "'";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
    EXPECT_EQ(c->getSize(), 32) << "6.20.2: '" << name << "' keeps its declared 32-bit type";
  }
};

// ---------------------------------------------------------------------------
// package tl_main_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamRefTest, AddrSpaceDebugMemIsLocalSignedIntOne) {
  const hldb::Package *const pkg = getPkg("tl_main_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'tl_main_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(pkg, "ADDR_SPACE_DEBUG_MEM");
  ASSERT_NE(p, nullptr) << "parameter 'ADDR_SPACE_DEBUG_MEM' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1, 6.20.4: declared with 'localparam' in a package";
  ExpectIntType(p, true, "6.11.3: int defaults to signed");
  ExpectDefault(pkg, "ADDR_SPACE_DEBUG_MEM", "1");
}

// ---------------------------------------------------------------------------
// package dm
// ---------------------------------------------------------------------------

TEST_F(ParamRefTest, DmDeclaresHaltAddressThenExceptionAddress) {
  const hldb::Package *const pkg = getPkg("dm");
  ASSERT_NE(pkg, nullptr) << "package 'dm' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  ASSERT_EQ(pkg->getParameters()->size(), 2u);
  const std::string_view expected[] = {"HaltAddress", "ExceptionAddress"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Parameter *const p = any_cast<hldb::Parameter>(pkg->getParameters()->at(i));
    ASSERT_NE(p, nullptr) << "parameter " << i << " is a value parameter";
    EXPECT_EQ(p->getName(), expected[i]) << "position " << i << " follows the order of declaration";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: " << expected[i] << " is declared with 'localparam'";
  }
}

TEST_F(ParamRefTest, HaltAddressIsSignedIntTen) {
  const hldb::Package *const pkg = getPkg("dm");
  ASSERT_NE(pkg, nullptr);
  ExpectIntType(getParam(pkg, "HaltAddress"), true, "6.11.3: int defaults to signed");
  ExpectDefault(pkg, "HaltAddress", "10");
}

TEST_F(ParamRefTest, ExceptionAddressIsUnsignedLogic31To0) {
  const hldb::Parameter *const p = getParam(getPkg("dm"), "ExceptionAddress");
  ASSERT_NE(p, nullptr) << "parameter 'ExceptionAddress' not found";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "ExceptionAddress is declared 'logic [31:0]'";
  EXPECT_FALSE(lt->getSigned()) << "6.11.3: logic defaults to unsigned";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u) << "one packed dimension";
  const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "31");
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ParamRefTest, ExceptionAddressIsHaltAddressPlusTwo) {
  const hldb::Package *const pkg = getPkg("dm");
  ASSERT_NE(pkg, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "ExceptionAddress");
  ASSERT_NE(pa, nullptr);
  EXPECT_EQ(getLhsActual(pa), getParam(pkg, "ExceptionAddress"));
  const hldb::Operation *const add = pa->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "11.4.3: 'HaltAddress + 2'";
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::RefObj *const halt = any_cast<hldb::RefObj>(add->getOperands()->at(0));
  ASSERT_NE(halt, nullptr) << "operand 0 is the name 'HaltAddress'";
  EXPECT_EQ(halt->getName(), "HaltAddress");
  ASSERT_NE(getParam(pkg, "HaltAddress"), nullptr);
  EXPECT_EQ(halt->getActual(), getParam(pkg, "HaltAddress")) << "26.3: HaltAddress is declared earlier in dm";
  const hldb::Constant *const two = any_cast<hldb::Constant>(add->getOperands()->at(1));
  ASSERT_NE(two, nullptr) << "operand 1 is the literal 2";
  EXPECT_EQ(two->getDecompile(), "2");
}

TEST_F(ParamRefTest, DmEndLabelIsDm) {
  const hldb::Package *const pkg = getPkg("dm");
  ASSERT_NE(pkg, nullptr);
  EXPECT_EQ(pkg->getEndLabel(), "dm") << "26.2: 'endpackage : dm'";
}

// ---------------------------------------------------------------------------
// module top_earlgrey
// ---------------------------------------------------------------------------

TEST_F(ParamRefTest, TopEarlgreyImportsAllOfTlMainPkg) {
  const hldb::Module *const m = getModule("top_earlgrey");
  ASSERT_NE(m, nullptr) << "module 'top_earlgrey' not found";
  std::vector<const hldb::ImportTypespec *> imports;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "top_earlgrey writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "tl_main_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamRefTest, TopEarlgreyDeclaresNoParameter) {
  const hldb::Module *const m = getModule("top_earlgrey");
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getParameters() == nullptr || m->getParameters()->empty())
      << "26.3: the wildcard import only makes tl_main_pkg's names visible";
}

TEST_F(ParamRefTest, TopEarlgreyInstantiatesRvCoreIbex) {
  const hldb::Module *const m = getModule("top_earlgrey");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getRefInstances(), nullptr);
  EXPECT_EQ(m->getRefInstances()->size(), 1u);
  const hldb::ModuleTypespec *const mt = getInstanceType(getInstance("top_earlgrey", "u_rv_core_ibex"));
  ASSERT_NE(mt, nullptr) << "instance 'u_rv_core_ibex' not found";
  ASSERT_NE(getModule("rv_core_ibex"), nullptr);
  EXPECT_EQ(mt->getModule(), getModule("rv_core_ibex"));
}

TEST_F(ParamRefTest, URvCoreIbexOverridesDmExceptionAddrByName) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("top_earlgrey", "u_rv_core_ibex"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::ParamAssign *const pa = overrides->at(0);
  EXPECT_TRUE(pa->getConnByName()) << "37.28: '.DmExceptionAddr(...)' is a named parameter assignment";
  EXPECT_EQ(getLhsName(pa), "DmExceptionAddr");
  const hldb::Parameter *const target = getParam(getModule("rv_core_ibex"), "DmExceptionAddr");
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(getLhsActual(pa), target)
      << "23.10.2.2: \"The name of the parameter shall be the name specified in the instantiated module.\"";
}

TEST_F(ParamRefTest, URvCoreIbexValueAddsImportedAndPackageScopedNames) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("top_earlgrey", "u_rv_core_ibex"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::Operation *const add = overrides->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "11.4.3: 'ADDR_SPACE_DEBUG_MEM + dm::ExceptionAddress'";
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::RefObj *const imported = any_cast<hldb::RefObj>(add->getOperands()->at(0));
  ASSERT_NE(imported, nullptr) << "operand 0 is the name 'ADDR_SPACE_DEBUG_MEM'";
  EXPECT_EQ(imported->getName(), "ADDR_SPACE_DEBUG_MEM");
  const hldb::Parameter *const debugMem = getParam(getPkg("tl_main_pkg"), "ADDR_SPACE_DEBUG_MEM");
  ASSERT_NE(debugMem, nullptr);
  EXPECT_EQ(imported->getActual(), debugMem) << "26.3: made visible by 'import tl_main_pkg::*'";
  const hldb::RefObj *const scoped = any_cast<hldb::RefObj>(add->getOperands()->at(1));
  ASSERT_NE(scoped, nullptr) << "operand 1 is the name 'dm::ExceptionAddress'";
  const hldb::Parameter *const exceptionAddress = getParam(getPkg("dm"), "ExceptionAddress");
  ASSERT_NE(exceptionAddress, nullptr);
  EXPECT_EQ(scoped->getActual(), exceptionAddress) << "26.3: the package scope resolution operator names dm's item";
}

TEST_F(ParamRefTest, PackageScopedNameBindsThroughDm) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("top_earlgrey", "u_rv_core_ibex"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::Operation *const add = overrides->at(0)->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::RefObj *const scoped = any_cast<hldb::RefObj>(add->getOperands()->at(1));
  ASSERT_NE(scoped, nullptr);
  ASSERT_NE(scoped->getPathElems(), nullptr) << "'dm::ExceptionAddress' has a package prefix";
  ASSERT_EQ(scoped->getPathElems()->size(), 2u) << "the prefix 'dm' and the item 'ExceptionAddress'";
  const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(scoped->getPathElems()->at(0));
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "dm");
  ASSERT_NE(getPkg("dm"), nullptr);
  EXPECT_EQ(prefix->getActual(), getPkg("dm")) << "26.3: the prefix of '::' is the package dm";
  const hldb::RefObj *const item = any_cast<hldb::RefObj>(scoped->getPathElems()->at(1));
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getName(), "ExceptionAddress");
  EXPECT_EQ(item->getActual(), getParam(getPkg("dm"), "ExceptionAddress"));
}

// ---------------------------------------------------------------------------
// modules rv_core_ibex and ibex_core
// ---------------------------------------------------------------------------

TEST_F(ParamRefTest, DmExceptionAddrIsOverridableIntUnsigned) {
  struct Declared {
    std::string_view m_module;
    std::string_view m_default;
  };
  const Declared declared[] = {{"rv_core_ibex", "3"}, {"ibex_core", "4"}};
  for (const Declared &d : declared) {
    SCOPED_TRACE(testing::Message() << "module " << d.m_module);
    const hldb::Module *const m = getModule(d.m_module);
    ASSERT_NE(m, nullptr) << "module '" << d.m_module << "' not found";
    ASSERT_NE(m->getParameters(), nullptr);
    EXPECT_EQ(m->getParameters()->size(), 1u);
    const hldb::Parameter *const p = getParam(m, "DmExceptionAddr");
    ASSERT_NE(p, nullptr) << "parameter 'DmExceptionAddr' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: the module has no parameter port list, so it is overridable";
    ExpectIntType(p, false, "6.11.3: 'int unsigned' is explicitly unsigned");
    ExpectDefault(m, "DmExceptionAddr", d.m_default);
  }
}

TEST_F(ParamRefTest, RvCoreIbexInstantiatesIbexCore) {
  const hldb::Module *const m = getModule("rv_core_ibex");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getRefInstances(), nullptr);
  EXPECT_EQ(m->getRefInstances()->size(), 1u);
  const hldb::ModuleTypespec *const mt = getInstanceType(getInstance("rv_core_ibex", "u_core"));
  ASSERT_NE(mt, nullptr) << "instance 'u_core' not found";
  ASSERT_NE(getModule("ibex_core"), nullptr);
  EXPECT_EQ(mt->getModule(), getModule("ibex_core"));
}

TEST_F(ParamRefTest, UCorePassesRvCoreIbexOwnParameter) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("rv_core_ibex", "u_core"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::ParamAssign *const pa = overrides->at(0);
  EXPECT_TRUE(pa->getConnByName()) << "37.28: '.DmExceptionAddr(...)' is a named parameter assignment";
  const hldb::Parameter *const inner = getParam(getModule("ibex_core"), "DmExceptionAddr");
  const hldb::Parameter *const outer = getParam(getModule("rv_core_ibex"), "DmExceptionAddr");
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(getLhsName(pa), "DmExceptionAddr");
  EXPECT_EQ(getLhsActual(pa), inner)
      << "23.10.2.2: \"The name of the parameter shall be the name specified in the instantiated module.\"";
  const hldb::RefObj *const value = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(value, nullptr) << "unreduced, the value is the name 'DmExceptionAddr'";
  EXPECT_EQ(value->getName(), "DmExceptionAddr");
  EXPECT_EQ(value->getActual(), outer) << "26.3: the value is resolved in rv_core_ibex, where it is written";
}

// ---------------------------------------------------------------------------
// Ports
// ---------------------------------------------------------------------------

TEST_F(ParamRefTest, ModulesHaveNoPort) {
  for (std::string_view name : {"top_earlgrey", "rv_core_ibex", "ibex_core"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty())
        << "A.1.2: " << name << "'s module_ansi_header omits the list_of_port_declarations";
  }
}

TEST_F(ParamRefTest, InstancesConnectNoPort) {
  struct Inst {
    std::string_view m_module;
    std::string_view m_name;
  };
  const Inst insts[] = {{"top_earlgrey", "u_rv_core_ibex"}, {"rv_core_ibex", "u_core"}};
  for (const Inst &i : insts) {
    const hldb::RefInstance *const inst = getInstance(i.m_module, i.m_name);
    ASSERT_NE(inst, nullptr) << "instance '" << i.m_name << "' not found";
    EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty())
        << "23.3.2: \"The list of port connections shall be provided only for modules defined with ports.\"";
  }
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamRefTest, ExceptionAddressReducesToTwelve) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("dm", m_design->getTopPackages());
    ASSERT_NE(pkg, nullptr) << "the elaborated dm not found";
    ExpectElabValue(pkg, "ExceptionAddress", 12u, "11.4.3: HaltAddress + 2 = 10 + 2");
  }
}

TEST_F(ParamRefTest, OverriddenDmExceptionAddrIsThirteen) {
  if (m_design->getElaborated()) {
    ExpectElabValue(getElabInstance({"u_rv_core_ibex"}), "DmExceptionAddr", 13u,
                    "23.10.3: the override 1 + 12 replaces the default 3");
    ExpectElabValue(getElabInstance({"u_rv_core_ibex", "u_core"}), "DmExceptionAddr", 13u,
                    "23.10.3: the override passes u_rv_core_ibex's 13, replacing the default 4");
  }
}

TEST_F(ParamRefTest, TopEarlgreyIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u)
        << "23.3.1: rv_core_ibex and ibex_core appear in module instantiation statements";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top_earlgrey");
  }
}

TEST_F(ParamRefTest, NoFatalSyntaxOrErrorDiagnostics) {
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
