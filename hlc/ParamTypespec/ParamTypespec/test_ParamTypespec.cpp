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

// Validates the HLDB model built for tests/ParamTypespec/dut.sv:
//
//   package otp_ctrl_part_pkg;
//      parameter logic [2:0] PartInvDefault = 3'({
//                                                 3'({
//                                                     3'h1
//                                                     })
//                                                 });
//   endpackage : otp_ctrl_part_pkg
//
//   module top(output int o);
//      import otp_ctrl_part_pkg::*;
//
//      parameter P = PartInvDefault;
//
//      logic [$bits(PartInvDefault)/1-1:0] part_buf_data = '1;
//      assign o = int'(part_buf_data);
//   endmodule
//
// The point of the fixture is the type of a package parameter seen through a
// wildcard import (26.3): PartInvDefault is declared 'logic [2:0]', its value
// is a size cast of a concatenation of a size cast of a concatenation of
// 3'h1 (6.24.1, 11.4.12), and module top uses it twice -- as the value of
// the untyped parameter P, and inside $bits() to size part_buf_data. The
// regression this file exists to catch is HLC losing the declared type of
// PartInvDefault, so that P is not typed after it and $bits(PartInvDefault)
// is not 3.
//
// Values:
//   - PartInvDefault = 3'({3'({3'h1})}) = 3'b001 = 1, 3 bits wide. 6.24.1:
//     "If the casting type is a constant expression with a positive integral
//     value, the expression in parentheses shall be padded or truncated to
//     the size specified."
//   - P has no type and no range, so 6.20.2 gives it the type of its final
//     value: "A parameter declaration with no type or range specification
//     shall default to the type and range of the final value assigned to the
//     parameter, after any value overrides have been applied." and "If the
//     expression is integral, the parameter is a logic vector of the same
//     size with range [size-1:0]." top is never instantiated, so nothing
//     overrides P: P is logic [2:0] and equals 1.
//   - $bits(PartInvDefault) = 3 (20.6.2: "The $bits system function returns
//     the number of bits required to hold an expression as a bit stream."),
//     so part_buf_data is logic [3/1-1:0] = logic [2:0].
//
// What is checked, and why:
//   Package otp_ctrl_part_pkg
//     - exactly 1 parameter, PartInvDefault, local because it is in a package
//       (6.20.1), of type logic with the single packed range [2:0], unsigned
//       (6.11.3)
//     - its unreduced value is Operation vpiCastOp whose size is Constant "3"
//       and whose expression is Operation vpiConcatOp over exactly one
//       operand, the inner Operation vpiCastOp, again of size Constant "3",
//       over Operation vpiConcatOp over exactly one operand, the Constant
//       3'h1: vpiHexConst, 3 bits (5.7.1)
//     - each size Constant "3" is an unsized decimal, at least 32 bits
//       (5.7.1)
//   Module top
//     - exactly 1 port, o, at index 0, an output of the signed type int
//       (6.11.3), connected inside to the Variable o: 23.2.2.3 says of an
//       output port "If the data type is declared with the explicit data_type
//       syntax, the port kind shall default to variable."
//     - exactly 2 variables, o and part_buf_data, and no net: both are
//       declared with a data type and no net type keyword (23.2.2.3, 6.8)
//     - records exactly 1 import, the wildcard import of otp_ctrl_part_pkg
//       (26.3)
//     - exactly 1 parameter, P, overridable (top has no parameter port list,
//       6.20.1), whose value is the RefObj PartInvDefault bound through the
//       import to the package's Parameter (26.3)
//     - part_buf_data is an unsigned logic variable (6.11.3) with one packed
//       range whose left bound is Operation vpiSubOp over (Operation vpiDivOp
//       over the SysFuncCall $bits and Constant "1") and Constant "1" -- '/'
//       binds tighter than '-' (11.3.2) -- and whose right bound is Constant
//       "0"; the single $bits argument is the RefObj PartInvDefault bound to
//       the package's Parameter
//     - part_buf_data is initialized with the unbased unsized literal '1
//       (5.7.1). 6.5: "An assignment as part of the declaration of a variable
//       is a variable initialization, not a continuous assignment."
//     - so exactly 1 continuous assignment, o = int'(part_buf_data): the LHS
//       is bound to the Variable o, the RHS is Operation vpiCastOp to the
//       signed int of the RefObj part_buf_data bound to the Variable
//       part_buf_data (6.24.1)
//   Elaboration
//     - the elaborated PartInvDefault is reduced to a 3-bit Constant equal to
//       1 (6.24.1)
//     - the elaborated P is a logic [2:0], unsigned, equal to 1 (6.20.2)
//     - the elaborated part_buf_data is logic [2:0] (20.6.2)
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics. In
//       particular both concatenations join a sized expression, which 11.4.12
//       requires: "Unsized constant numbers shall not be allowed in
//       concatenations."
//
// Reduction and elaboration: the reduced values and P's type are checked only
// when the design is elaborated, on Design::getTopPackages() and
// Design::getTopModules(); the unreduced expressions and bindings are
// asserted on the definitions in every run. P's type is asserted only after
// elaboration because 6.20.2 makes it depend on the final value "after any
// value overrides have been applied", which only elaboration knows. Note that
// ParamTypespec.hlc does not ask HLC to elaborate, so under that command file
// the gated checks assert nothing; they take effect on an elaborated design.
//
// Model convention: HLC models a cast as an Operation vpiCastOp with two
// operands, the casting type first and the expression being cast second.
// For a simple_type casting type ('int') the first operand is a RefTypespec;
// for a size cast ('3') it is the Constant giving the size. The Operation's
// own typespec is left empty. 37.59 detail 3 describes the same fact with
// the cast "represented as a unary operation, with its sole argument being
// the expression being cast, and the typespec of the cast expression being
// the type to which the argument is being cast". The tests read the casting
// type from the first operand and the cast expression from the second.
//
// Model convention: HLC holds each argument of a system function call in a
// NamedArgument, with the argument expression as its high connection. The
// tests read the $bits argument from there.
//
// What is NOT checked, and why:
//   - The constant type of the unsized decimals (the cast sizes '3', the
//     divisor and subtrahend '1', the range bounds). The VPI constants
//     vpiDecConst ("decimal integer") and vpiIntConst ("integer constant")
//     both fit a simple decimal number, and the standard does not say which
//     one it is.
//   - The constant type and the size of '1. The standard defines no VPI
//     constant type for an unbased unsized literal, and its width depends on
//     the context it is used in (5.7.1); its effect is asserted through its
//     decompiled text.
//   - The typespec of a Constant. 37.59 detail 5 says that, outside casts,
//     simple expressions and typed assignment patterns, "it is
//     implementation dependent as to whether or not there is any associated
//     typespec".
//   - P's type in an unelaborated run. 6.20.2 ties it to the final value
//     after overrides, which only elaboration determines.
//   - The value o is driven with, int'(3'b111) = 7. It is the value of a
//     continuous assignment, which only exists while the design runs. HLC is
//     a static compiler/elaborator and not a simulator, so this is out of
//     scope permanently rather than a gap waiting to be filled, and no
//     placeholder test is carried for it; ContAssignCastsPartBufDataToInt
//     and ElaboratedPartBufDataIs2DownTo0 are the static evidence.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamTypespec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("otp_ctrl_part_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  static const hldb::Parameter *getPartInvDefault() { return getParam(getPkg(), "PartInvDefault"); }

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

  // The Parameter a ParamAssign assigns: its LHS itself, or what a RefObj
  // LHS is bound to.
  static const hldb::Any *getLhsTarget(const hldb::ParamAssign *pa) {
    const hldb::RefObj *const ref = pa->getLhs<hldb::RefObj>();
    return (ref != nullptr) ? ref->getActual() : pa->getLhs();
  }

  static const hldb::Variable *getVar(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, scope->getVariables());
  }

  static const hldb::Module *getElabTop() {
    return hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
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

  // Asserts that 'rangeExpr' is a Constant whose integer value is 'expected'.
  static void expectIntConstant(const hldb::Expr *rangeExpr, uint64_t expected, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(rangeExpr);
    ASSERT_NE(c, nullptr) << what << " should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << what << ": unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << what;
  }

  // Asserts that 'cast' is a size cast of size "3" whose cast expression is
  // a one-operand concatenation, and returns that operand.
  static const hldb::Any *expectSize3CastOfConcat(const hldb::Operation *cast, std::string_view what) {
    EXPECT_EQ(cast->getOpType(), vpiCastOp) << what << ": 6.24.1, a cast";
    if ((cast->getOperands() == nullptr) || (cast->getOperands()->size() != 2u)) {
      ADD_FAILURE() << what << ": a cast holds its casting type and its expression";
      return nullptr;
    }
    const hldb::Constant *const size = any_cast<hldb::Constant>(cast->getOperands()->at(0));
    if (size == nullptr) {
      ADD_FAILURE() << what << ": the casting type '3' is a Constant";
      return nullptr;
    }
    EXPECT_EQ(size->getDecompile(), "3") << what;
    EXPECT_GE(size->getSize(), 32) << what
                                   << ": 5.7.1, the number of bits that make up an unsized number shall be at least 32";
    const hldb::Operation *const concat = any_cast<hldb::Operation>(cast->getOperands()->at(1));
    if (concat == nullptr) {
      ADD_FAILURE() << what << ": the cast expression is a concatenation";
      return nullptr;
    }
    EXPECT_EQ(concat->getOpType(), vpiConcatOp) << what << ": 11.4.12";
    if ((concat->getOperands() == nullptr) || (concat->getOperands()->size() != 1u)) {
      ADD_FAILURE() << what << ": the concatenation joins exactly one expression";
      return nullptr;
    }
    return concat->getOperands()->at(0);
  }
};

// ---------------------------------------------------------------------------
// package otp_ctrl_part_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamTypespecTest, PartInvDefaultIsLocalLogic2To0) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'otp_ctrl_part_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u) << "the package declares exactly one parameter";
  const hldb::Parameter *const p = getPartInvDefault();
  ASSERT_NE(p, nullptr) << "parameter 'PartInvDefault' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1: a param_assignment in a package becomes a localparam";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'PartInvDefault' is declared 'logic [2:0]'";
  EXPECT_FALSE(lt->getSigned()) << "6.11.3: logic defaults to unsigned";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u) << "one packed dimension";
  const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "2");
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ParamTypespecTest, PartInvDefaultIsNestedSizeCastOf3h1) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "PartInvDefault");
  ASSERT_NE(pa, nullptr) << "the ParamAssign of 'PartInvDefault' not found";
  ASSERT_NE(getPartInvDefault(), nullptr);
  EXPECT_EQ(getLhsTarget(pa), getPartInvDefault()) << "the LHS is the package's PartInvDefault";
  const hldb::Operation *const outer = pa->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr) << "unreduced, the value 3'({...}) is an Operation";
  const hldb::Any *const outerItem = expectSize3CastOfConcat(outer, "outer cast");
  ASSERT_NE(outerItem, nullptr);
  const hldb::Operation *const inner = any_cast<hldb::Operation>(outerItem);
  ASSERT_NE(inner, nullptr) << "the outer concatenation joins the inner cast 3'({3'h1})";
  const hldb::Any *const innerItem = expectSize3CastOfConcat(inner, "inner cast");
  ASSERT_NE(innerItem, nullptr);
  const hldb::Constant *const lit = any_cast<hldb::Constant>(innerItem);
  ASSERT_NE(lit, nullptr) << "the inner concatenation joins the literal 3'h1";
  EXPECT_EQ(lit->getDecompile(), "3'h1");
  EXPECT_EQ(lit->getConstType(), vpiHexConst) << "5.7.1: 'h is the hexadecimal base format";
  EXPECT_EQ(lit->getSize(), 3) << "5.7.1: the size constant 3 is the exact number of bits";
}

// ---------------------------------------------------------------------------
// module top: header, import and parameter
// ---------------------------------------------------------------------------

TEST_F(ParamTypespecTest, TopHasOneOutputIntPortO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u) << "'top(output int o)' declares exactly one port";
  const hldb::Port *const o = top->getPorts()->at(0);
  EXPECT_EQ(o->getName(), "o");
  EXPECT_EQ(o->getPortIndex(), 0);
  EXPECT_EQ(o->getDirection(), vpiOutput) << "23.2.2.3: 'output'";
  ASSERT_NE(o->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = o->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'o' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11.3: int defaults to signed";
  const hldb::RefObj *const low = o->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr) << "the port connects inside to 'o'";
  EXPECT_EQ(low->getName(), "o");
  ASSERT_NE(getVar(top, "o"), nullptr);
  EXPECT_EQ(low->getActual(), getVar(top, "o")) << "23.2.2.3: 'output int o' is a variable port";
}

TEST_F(ParamTypespecTest, OIsSignedIntVariable) {
  const hldb::Variable *const o = getVar(getTop(), "o");
  ASSERT_NE(o, nullptr) << "23.2.2.3: 'output int o' declares the variable 'o'";
  ASSERT_NE(o->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = o->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'o' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11.3: int defaults to signed";
}

TEST_F(ParamTypespecTest, TopDeclaresTwoVariablesAndNoNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u) << "'o' and 'part_buf_data'";
  EXPECT_NE(getVar(top, "o"), nullptr) << "23.2.2.3: an output with an explicit data type is a variable";
  EXPECT_NE(getVar(top, "part_buf_data"), nullptr) << "6.8: a data declaration with no net type is a variable";
  EXPECT_TRUE((top->getNets() == nullptr) || top->getNets()->empty()) << "top declares no net";
}

TEST_F(ParamTypespecTest, TopImportsAllOfOtpCtrlPartPkg) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  std::vector<const hldb::ImportTypespec *> imports;
  if (top->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *top->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "top writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "otp_ctrl_part_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamTypespecTest, PIsOverridableAndValuedByImportedPartInvDefault) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  EXPECT_EQ(top->getParameters()->size(), 1u) << "top declares exactly one parameter";
  const hldb::Parameter *const p = getParam(top, "P");
  ASSERT_NE(p, nullptr) << "parameter 'P' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: top has no parameter port list, so 'parameter P' is overridable";
  const hldb::ParamAssign *const pa = getParamAssign(top, "P");
  ASSERT_NE(pa, nullptr);
  EXPECT_EQ(getLhsTarget(pa), p) << "the LHS is top's P";
  const hldb::RefObj *const value = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(value, nullptr) << "unreduced, the value is the name 'PartInvDefault'";
  EXPECT_EQ(value->getName(), "PartInvDefault");
  ASSERT_NE(getPartInvDefault(), nullptr);
  EXPECT_EQ(value->getActual(), getPartInvDefault()) << "26.3: imported from otp_ctrl_part_pkg";
}

// ---------------------------------------------------------------------------
// module top: part_buf_data and the continuous assignment
// ---------------------------------------------------------------------------

TEST_F(ParamTypespecTest, PartBufDataRangeIsBitsOfPartInvDefaultDiv1Minus1To0) {
  const hldb::Variable *const v = getVar(getTop(), "part_buf_data");
  ASSERT_NE(v, nullptr) << "variable 'part_buf_data' not found";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'part_buf_data' is declared 'logic [...]'";
  EXPECT_FALSE(lt->getSigned()) << "6.11.3: logic defaults to unsigned";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u) << "one packed dimension";
  const hldb::Range *const r = lt->getRanges()->at(0);

  const hldb::Operation *const sub = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(sub, nullptr) << "unreduced, the left bound is '$bits(PartInvDefault)/1-1'";
  EXPECT_EQ(sub->getOpType(), vpiSubOp) << "11.3.2: '/' binds tighter than binary '-'";
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_EQ(sub->getOperands()->size(), 2u);
  const hldb::Operation *const div = any_cast<hldb::Operation>(sub->getOperands()->at(0));
  ASSERT_NE(div, nullptr) << "the minuend is '$bits(PartInvDefault)/1'";
  EXPECT_EQ(div->getOpType(), vpiDivOp);
  const hldb::Constant *const subtrahend = any_cast<hldb::Constant>(sub->getOperands()->at(1));
  ASSERT_NE(subtrahend, nullptr);
  EXPECT_EQ(subtrahend->getDecompile(), "1");

  ASSERT_NE(div->getOperands(), nullptr);
  ASSERT_EQ(div->getOperands()->size(), 2u);
  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(div->getOperands()->at(0));
  ASSERT_NE(bits, nullptr) << "the dividend is '$bits(PartInvDefault)'";
  EXPECT_EQ(bits->getName(), "$bits");
  const hldb::Constant *const divisor = any_cast<hldb::Constant>(div->getOperands()->at(1));
  ASSERT_NE(divisor, nullptr);
  EXPECT_EQ(divisor->getDecompile(), "1");

  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u) << "20.6.2: $bits takes one expression";
  const hldb::RefObj *const arg = bits->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(arg, nullptr) << "the $bits argument is the name 'PartInvDefault'";
  EXPECT_EQ(arg->getName(), "PartInvDefault");
  ASSERT_NE(getPartInvDefault(), nullptr);
  EXPECT_EQ(arg->getActual(), getPartInvDefault()) << "26.3: imported from otp_ctrl_part_pkg";

  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ParamTypespecTest, PartBufDataIsInitializedWithTick1) {
  const hldb::Variable *const v = getVar(getTop(), "part_buf_data");
  ASSERT_NE(v, nullptr);
  const hldb::Constant *const init = v->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr)
      << "6.5: An assignment as part of the declaration of a variable is a variable initialization";
  EXPECT_EQ(init->getDecompile(), "'1")
      << "5.7.1: an unbased unsized literal; all bits of the unsized value shall be set to the value of the "
         "specified bit";
}

TEST_F(ParamTypespecTest, ContAssignCastsPartBufDataToInt) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u) << "top writes exactly one continuous assignment";
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);

  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  ASSERT_NE(getVar(top, "o"), nullptr);
  EXPECT_EQ(lhs->getActual(), getVar(top, "o")) << "the LHS is the output variable 'o'";

  const hldb::Operation *const cast = ca->getRhs<hldb::Operation>();
  ASSERT_NE(cast, nullptr) << "the RHS int'(part_buf_data) is an Operation";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1: a cast";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 2u) << "the casting type and the expression being cast";
  const hldb::RefTypespec *const type = any_cast<hldb::RefTypespec>(cast->getOperands()->at(0));
  ASSERT_NE(type, nullptr) << "the casting type is the simple_type 'int'";
  ASSERT_NE(type->getActual<hldb::IntTypespec>(), nullptr) << "6.24.1: the cast is to 'int'";
  EXPECT_TRUE(type->getActual<hldb::IntTypespec>()->getSigned()) << "6.11.3: int defaults to signed";
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(cast->getOperands()->at(1));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), "part_buf_data");
  ASSERT_NE(getVar(top, "part_buf_data"), nullptr);
  EXPECT_EQ(operand->getActual(), getVar(top, "part_buf_data"));
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamTypespecTest, ElaboratedPartInvDefaultIsThreeBitOne) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("otp_ctrl_part_pkg", m_design->getTopPackages());
    ASSERT_NE(pkg, nullptr) << "the elaborated otp_ctrl_part_pkg not found";
    const hldb::ParamAssign *const pa = getParamAssign(pkg, "PartInvDefault");
    ASSERT_NE(pa, nullptr);
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: the value should be reduced to a Constant";
    EXPECT_EQ(c->getSize(), 3) << "6.24.1: padded or truncated to the size specified, 3";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 1u) << "3'({3'({3'h1})}) is 3'b001";
  }
}

TEST_F(ParamTypespecTest, ElaboratedPIsLogic2To0EqualToOne) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = getElabTop();
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::Parameter *const p = getParam(top, "P");
    ASSERT_NE(p, nullptr) << "the elaborated parameter 'P' not found";
    ASSERT_NE(p->getTypespec(), nullptr) << "6.20.2: P has the type of its final value";
    const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "6.20.2: the final value is integral, so the parameter is a logic vector";
    EXPECT_FALSE(lt->getSigned()) << "6.20.2: P takes the type of PartInvDefault, an unsigned logic [2:0]";
    ASSERT_NE(lt->getRanges(), nullptr) << "6.20.2: a logic vector of the same size with range [size-1:0]";
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    expectIntConstant(lt->getRanges()->at(0)->getLeftExpr(), 2, "6.20.2: MSB is size-1 = 2");
    expectIntConstant(lt->getRanges()->at(0)->getRightExpr(), 0, "6.20.2: LSB is 0");
    const hldb::ParamAssign *const pa = getParamAssign(top, "P");
    ASSERT_NE(pa, nullptr);
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: the value should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 1u) << "nothing overrides P, so it keeps PartInvDefault = 1";
  }
}

TEST_F(ParamTypespecTest, ElaboratedPartBufDataIs2DownTo0) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = getElabTop();
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::Variable *const v = getVar(top, "part_buf_data");
    ASSERT_NE(v, nullptr) << "the elaborated variable 'part_buf_data' not found";
    ASSERT_NE(v->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    expectIntConstant(lt->getRanges()->at(0)->getLeftExpr(), 2, "20.6.2: $bits(PartInvDefault)/1-1 = 3/1-1 = 2");
    expectIntConstant(lt->getRanges()->at(0)->getRightExpr(), 0, "right bound");
  }
}

TEST_F(ParamTypespecTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: top is the only module";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamTypespecTest, NoFatalSyntaxOrErrorDiagnostics) {
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
