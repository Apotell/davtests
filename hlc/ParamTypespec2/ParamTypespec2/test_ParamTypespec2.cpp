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

// Validates the HLDB model built for tests/ParamTypespec2/dut.sv:
//
//   package prim_esc_pkg;
//     typedef struct packed {
//       logic esc_p;
//       logic esc_n;
//     } esc_tx_t;
//
//     parameter esc_tx_t ESC_TX_DEFAULT = '{esc_p:  1'b0,
//                                           esc_n:  1'b1};
//
//   endpackage : prim_esc_pkg
//
//   module nmi_gen
//      import prim_esc_pkg::*;
//   (
//      input esc_tx_t [2:0] a,
//      output int x
//   );
//      assign x = int'(a);
//   endmodule // nmi_gen
//
//   module top(output int o);
//      nmi_gen u_nmi_gen(
//         .a({3{prim_esc_pkg::ESC_TX_DEFAULT}}),
//         .x(o)
//      );
//   endmodule // top
//
// The point of the fixture is a package parameter whose declared type is a
// packed-struct typedef (7.2.1), initialized by a structure assignment
// pattern keyed by member name (10.9.2). The parameter is referenced from
// another module through the package scope resolution operator (26.3),
// inside a replication (11.4.12.1) that drives an input port whose type is a
// packed array of the same struct (7.4.1), imported through a wildcard
// import in the module header (26.3). The regression this file exists to
// catch is HLC losing the struct typespec of the parameter or of the port,
// or failing to bind the scoped reference to the package parameter.
//
// Values: 7.2.1, "The first member specified is the most significant and
// subsequent members follow in decreasing significance", so esc_tx_t is 2
// bits with esc_p as bit 1 and esc_n as bit 0, and ESC_TX_DEFAULT is 2'b01,
// i.e. 1. The port a is 3 * 2 = 6 bits, and {3{ESC_TX_DEFAULT}} is
// 6'b010101, also 6 bits.
//
// What is checked, and why:
//   Package prim_esc_pkg
//     - exactly 1 typedef, esc_tx_t, a packed (7.2.1) and unsigned ("with
//       unsigned being the default") struct with exactly the 2 members
//       esc_p and esc_n, in that order, each an unsigned single-bit logic
//     - exactly 1 parameter, ESC_TX_DEFAULT, local because it is in a
//       package (6.20.1), of the type esc_tx_t
//     - its unreduced value is Operation vpiAssignmentPatternOp (10.9.2)
//       with 2 operands in member declaration order: the binary literal
//       1'b0 for esc_p and 1'b1 for esc_n, each key bound to its member
//   Module nmi_gen
//     - records exactly 1 import, the wildcard import of prim_esc_pkg (26.3),
//       and declares no parameter of its own
//     - lists 2 ports, a and x, in that order, with port indexes 0 and 1
//       (37.14 detail 9)
//     - a is an input and its only net, a wire (23.2.2.3, 22.8), of a packed
//       array [2:0] (7.4.1) of esc_tx_t, the package's typedef seen through
//       the import; the port's low connection is bound to that net
//     - x is an output and its only variable (23.2.2.3) of the signed type
//       int (6.11); the port's low connection is bound to that variable
//     - exactly 1 continuous assignment, x = int'(a): LHS bound to the
//       variable x, RHS Operation vpiCastOp (6.24.1) to int of the net a
//   Module top
//     - 1 port, o, an output and its only variable, of int, port index 0;
//       top declares no net
//     - exactly 1 instance, u_nmi_gen of nmi_gen, overriding no parameter,
//       with 2 connections by name (23.3.2.2), .a and .x in that order, each
//       bound to the nmi_gen port of that name
//     - .a is Operation vpiMultiConcatOp (11.4.12.1) whose first operand is
//       the multiplier 3 (37.59 detail 1) and whose only concatenated
//       expression is the reference prim_esc_pkg::ESC_TX_DEFAULT, bound to
//       the package's Parameter (26.3); top writes no import, so the binding
//       comes from the package scope resolution operator alone
//     - .x is the reference o, bound to top's variable o
//   Elaboration
//     - on the elaborated package, ESC_TX_DEFAULT is reduced to a 2-bit
//       Constant whose value is 1 (6.20.2, 7.2.1)
//     - top is the only top-level instance (23.3.1), and holds the instance
//       u_nmi_gen of nmi_gen
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced parameter value and the instance
// tree are checked only when the design is elaborated, on
// Design::getTopPackages() and Design::getTopModules(); the declarations,
// types, unreduced values and bindings are asserted on the definitions in
// every run.
//
// Model convention: HLC keeps the member keys of the structure assignment
// pattern: each operand of the vpiAssignmentPatternOp is a TaggedPattern
// whose tag is the member name and whose pattern is the value, where 37.59
// detail 6 says "the operand iteration shall return the expressions as if
// the assignment pattern were written with the positional notation". Here
// the keys follow the member declaration order, so the operand order is the
// positional order either way. The tests read the value through a
// TaggedPattern when there is one and assert that its key is bound to the
// member at that position.
//
// Model convention: HLC holds the target type of a cast as a RefTypespec in
// the first operand and the expression being cast in the second, where 37.59
// detail 3 says the cast "is represented as a unary operation, with its sole
// argument being the expression being cast, and the typespec of the cast
// expression being the type to which the argument is being cast". The tests
// read the target type from Operation::getTypespec() when it is set and
// from the leading RefTypespec operand otherwise, and assert both the type
// and the expression being cast.
//
// Model convention: HLC holds the concatenation of a replication as a
// nested vpiConcatOp operand after the multiplier, where 37.59 detail 1
// says "The remaining operands shall be the expressions within the
// concatenation". The tests read the concatenated expressions through such
// a nested concatenation when there is one, and assert the multiplier and
// each expression.
//
// What is NOT checked, and why:
//   - The values of x and o (6'b010101 = 21). They exist only once the
//     continuous assignment and the port connection are evaluated, which is
//     a runtime property. HLC is a static compiler/elaborator and is not a
//     simulator, so this is out of scope permanently rather than a gap
//     waiting to be filled; EscTxDefaultReducesToOne and the static widths
//     of esc_tx_t and of the port a carry the static evidence.
//   - Whether the port expression {3{...}} is folded to a constant in the
//     elaborated model is a tool convention; its operands and bindings are
//     asserted on the definition.
//   - How HLC spells the name of the scoped reference
//     prim_esc_pkg::ESC_TX_DEFAULT, as a whole or split into path elements,
//     is a model convention; what it is bound to is asserted.
//   - The constant type and size of the unsized multiplier 3. vpi_user.h
//     offers both vpiDecConst ("decimal integer") and vpiIntConst ("integer
//     constant") for it, and 5.7.1 only says the size of an unsized number
//     "shall be at least 32", so neither is fixed by the standard; its value
//     is asserted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
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
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamTypespec2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamTypespec2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("prim_esc_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  static const hldb::Parameter *getEscTxDefault() { return getParam(getPkg(), "ESC_TX_DEFAULT"); }

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

  static const hldb::Typedef *getEscTxT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("esc_tx_t", pkg->getTypedefs());
  }

  // The struct esc_tx_t aliases.
  static const hldb::Struct *getEscTxStruct() {
    const hldb::Typedef *const td = getEscTxT();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr) return nullptr;
    return st->getStruct();
  }

  // Whether 'rts' names esc_tx_t, either through its TypedefTypespec or by
  // resolving to the struct type the typedef aliases.
  static bool namesEscTxT(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getEscTxT();
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr || rts->getActual() == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = rts->getActual<hldb::TypedefTypespec>();
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
           (rts->getActual() == td->getAlias()->getActual());
  }

  static const hldb::Port *getPort(const hldb::Module *m, std::string_view name) {
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Port>(name, m->getPorts());
  }

  static const hldb::RefInstance *getUNmiGen() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_nmi_gen", top->getRefInstances());
  }

  // The connection in 'inst' whose low conn names 'port'.
  static const hldb::NamedArgument *getConnection(const hldb::RefInstance *inst, std::string_view port) {
    if (inst == nullptr || inst->getArguments() == nullptr) return nullptr;
    for (const hldb::NamedArgument *const na : *inst->getArguments()) {
      const hldb::RefObj *const low = na->getLowConn<hldb::RefObj>();
      if ((low != nullptr) && (low->getName() == port)) return na;
    }
    return nullptr;
  }

  // The value an assignment pattern operand gives, looking through a
  // TaggedPattern that keeps the member key (see Model convention).
  static const hldb::Any *getPatternValue(const hldb::Any *operand) {
    if (const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(operand)) return tp->getPattern();
    return operand;
  }

  // The target type of a cast: Operation::getTypespec() or, failing that,
  // a leading RefTypespec operand (see Model convention).
  static const hldb::RefTypespec *getCastTarget(const hldb::Operation *op) {
    if (op == nullptr) return nullptr;
    if (op->getTypespec() != nullptr) return op->getTypespec();
    if (op->getOperands() == nullptr || op->getOperands()->empty()) return nullptr;
    return any_cast<hldb::RefTypespec>(op->getOperands()->at(0));
  }

  // The operands of a cast that are not its target type.
  static std::vector<const hldb::Any *> getCastArguments(const hldb::Operation *op) {
    std::vector<const hldb::Any *> args;
    if (op == nullptr || op->getOperands() == nullptr) return args;
    for (const hldb::Any *const operand : *op->getOperands()) {
      if ((op->getTypespec() == nullptr) && (operand == op->getOperands()->at(0)) &&
          (any_cast<hldb::RefTypespec>(operand) != nullptr)) {
        continue;
      }
      args.emplace_back(operand);
    }
    return args;
  }

  // The expressions within the concatenation of a replication: the operands
  // after the multiplier, looking through a single nested concatenation
  // (see Model convention).
  static std::vector<const hldb::Any *> getReplicatedItems(const hldb::Operation *op) {
    std::vector<const hldb::Any *> items;
    if (op == nullptr || op->getOperands() == nullptr || op->getOperands()->size() < 2u) return items;
    if (op->getOperands()->size() == 2u) {
      const hldb::Operation *const inner = any_cast<hldb::Operation>(op->getOperands()->at(1));
      if ((inner != nullptr) && (inner->getOpType() == vpiConcatOp)) {
        if (inner->getOperands() != nullptr) {
          for (const hldb::Any *const item : *inner->getOperands()) items.emplace_back(item);
        }
        return items;
      }
    }
    for (size_t i = 1; i < op->getOperands()->size(); ++i) items.emplace_back(op->getOperands()->at(i));
    return items;
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

  // Verifies 'rts' is a packed array [2:0] of esc_tx_t.
  static void ExpectEscTxArray2To0(const hldb::RefTypespec *rts, std::string_view what) {
    ASSERT_NE(rts, nullptr) << what << " has no type";
    const hldb::ArrayTypespec *const at = rts->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(at, nullptr) << what << ": 'esc_tx_t [2:0]' is an array of esc_tx_t";
    EXPECT_TRUE(at->getPacked()) << "7.4.1: " << what << "'s dimension [2:0] precedes the name, so it is packed";
    const hldb::Range *const r = at->getRange();
    ASSERT_NE(r, nullptr) << what << ": the dimension [2:0]";
    const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << what;
    EXPECT_EQ(left->getDecompile(), "2") << what;
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr) << what;
    EXPECT_EQ(right->getDecompile(), "0") << what;
    EXPECT_TRUE(namesEscTxT(at->getElemTypespec()))
        << "26.3: " << what << "'s element type is esc_tx_t, imported from prim_esc_pkg";
  }

  // Verifies 'rts' is the signed type int.
  static void ExpectInt(const hldb::RefTypespec *rts, std::string_view what) {
    ASSERT_NE(rts, nullptr) << what << " has no type";
    const hldb::IntTypespec *const it = rts->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << what << " is declared 'int'";
    EXPECT_TRUE(it->getSigned()) << "6.11, Table 6-8: 'int' is a 32-bit signed integer";
  }
};

// ---------------------------------------------------------------------------
// package prim_esc_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamTypespec2Test, EscTxTIsPackedStructOfEscPAndEscN) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'prim_esc_pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "prim_esc_pkg declares exactly one typedef";
  ASSERT_NE(getEscTxT(), nullptr) << "typedef 'esc_tx_t' not found";
  const hldb::Struct *const s = getEscTxStruct();
  ASSERT_NE(s, nullptr) << "esc_tx_t aliases a struct";
  EXPECT_TRUE(s->getPacked()) << "7.2.1: declared 'struct packed'";
  EXPECT_FALSE(s->getSigned()) << "7.2.1: a packed structure is unsigned by default";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u);
  const std::string_view names[] = {"esc_p", "esc_n"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TypespecMember *const m = s->getMembers()->at(i);
    ASSERT_NE(m, nullptr) << "member " << i;
    EXPECT_EQ(m->getName(), names[i]) << "member " << i << ", in declaration order";
    ASSERT_NE(m->getTypespec(), nullptr) << names[i];
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << names[i] << " is declared 'logic'";
    EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty())
        << names[i] << " has no packed dimension, so it is a single bit";
    EXPECT_FALSE(lt->getSigned()) << "6.11.3: logic is unsigned by default";
  }
}

TEST_F(ParamTypespec2Test, EscTxDefaultIsLocalEscTxT) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u) << "prim_esc_pkg declares exactly one parameter";
  const hldb::Parameter *const p = getEscTxDefault();
  ASSERT_NE(p, nullptr) << "parameter 'ESC_TX_DEFAULT' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1: a param_assignment in a package becomes a localparam";
  EXPECT_TRUE(namesEscTxT(p->getTypespec())) << "'ESC_TX_DEFAULT' is declared 'esc_tx_t'";
}

TEST_F(ParamTypespec2Test, EscTxDefaultIsPatternOfZeroAndOne) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "ESC_TX_DEFAULT");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'ESC_TX_DEFAULT'";
  EXPECT_EQ(pa->getLhs<hldb::RefObj>() != nullptr ? pa->getLhs<hldb::RefObj>()->getActual() : pa->getLhs(),
            getEscTxDefault())
      << "the assignment sets the package's parameter";
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a structure assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u) << "10.9.2: one expression per member";
  const hldb::Struct *const s = getEscTxStruct();
  ASSERT_NE(s, nullptr);
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u);
  const std::string_view texts[] = {"1'b0", "1'b1"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Any *const operand = pattern->getOperands()->at(i);
    if (const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(operand)) {
      const hldb::RefObj *const key = tp->getTag<hldb::RefObj>();
      ASSERT_NE(key, nullptr) << "operand " << i << ": the member key";
      EXPECT_EQ(key->getActual(), s->getMembers()->at(i))
          << "37.59 detail 6: operand " << i << " is the value of member " << s->getMembers()->at(i)->getName();
    }
    const hldb::Constant *const c = any_cast<hldb::Constant>(getPatternValue(operand));
    ASSERT_NE(c, nullptr) << "operand " << i << " is a literal";
    EXPECT_EQ(c->getConstType(), vpiBinaryConst) << "5.7.1: " << texts[i] << " is a binary literal";
    EXPECT_EQ(c->getSize(), 1) << "5.7.1: " << texts[i] << " is 1 bit wide";
    EXPECT_EQ(c->getDecompile(), texts[i]);
  }
}

// ---------------------------------------------------------------------------
// module nmi_gen
// ---------------------------------------------------------------------------

TEST_F(ParamTypespec2Test, NmiGenImportsAllOfPrimEscPkg) {
  const hldb::Module *const m = getModule("nmi_gen");
  ASSERT_NE(m, nullptr) << "module 'nmi_gen' not found";
  std::vector<const hldb::ImportTypespec *> imports;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "nmi_gen writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "prim_esc_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
  EXPECT_TRUE(m->getParameters() == nullptr || m->getParameters()->empty())
      << "26.3: the import makes ESC_TX_DEFAULT visible but does not declare it in nmi_gen";
}

TEST_F(ParamTypespec2Test, NmiGenListsPortsAThenX) {
  const hldb::Module *const m = getModule("nmi_gen");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 2u);
  const std::string_view names[] = {"a", "x"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Port *const p = m->getPorts()->at(i);
    ASSERT_NE(p, nullptr) << "port " << i;
    EXPECT_EQ(p->getName(), names[i]) << "port " << i << ", in list order";
    EXPECT_EQ(p->getPortIndex(), static_cast<int32_t>(i)) << "37.14 detail 9: the first port has index 0";
  }
}

TEST_F(ParamTypespec2Test, PortAIsInputWireOfEscTxArray) {
  const hldb::Module *const m = getModule("nmi_gen");
  ASSERT_NE(m, nullptr);
  const hldb::Port *const p = getPort(m, "a");
  ASSERT_NE(p, nullptr) << "port 'a' not found";
  EXPECT_EQ(p->getDirection(), vpiInput);
  ExpectEscTxArray2To0(p->getTypespec(), "port a");
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "nmi_gen declares exactly one net, a";
  const hldb::Net *const n = hldb::findByName<hldb::Net>("a", m->getNets());
  ASSERT_NE(n, nullptr) << "23.2.2.3: an input port with no port kind is a net";
  EXPECT_EQ(n->getNetType(), vpiWire) << "23.2.2.3, 22.8: the default net type is wire";
  ExpectEscTxArray2To0(n->getTypespec(), "net a");
  const hldb::RefObj *const low = p->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr) << "37.14: the port's low connection";
  EXPECT_EQ(low->getActual(), n) << "the port a connects to the net a";
}

TEST_F(ParamTypespec2Test, PortXIsOutputIntVariable) {
  const hldb::Module *const m = getModule("nmi_gen");
  ASSERT_NE(m, nullptr);
  const hldb::Port *const p = getPort(m, "x");
  ASSERT_NE(p, nullptr) << "port 'x' not found";
  EXPECT_EQ(p->getDirection(), vpiOutput);
  ExpectInt(p->getTypespec(), "port x");
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 1u) << "nmi_gen declares exactly one variable, x";
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("x", m->getVariables());
  ASSERT_NE(v, nullptr) << "23.2.2.3: an output port with an explicit data type is a variable";
  ExpectInt(v->getTypespec(), "variable x");
  EXPECT_EQ(hldb::findByName<hldb::Net>("x", m->getNets()), nullptr) << "23.2.2.3: x is a variable, not a net";
  const hldb::RefObj *const low = p->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr) << "37.14: the port's low connection";
  EXPECT_EQ(low->getActual(), v) << "the port x connects to the variable x";
}

TEST_F(ParamTypespec2Test, XIsAssignedIntCastOfA) {
  const hldb::Module *const m = getModule("nmi_gen");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_EQ(m->getContAssigns()->size(), 1u) << "nmi_gen writes exactly one continuous assignment";
  const hldb::ContAssign *const ca = m->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the LHS is the name 'x'";
  EXPECT_EQ(lhs->getName(), "x");
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("x", m->getVariables()))
      << "x is bound to nmi_gen's variable x";
  const hldb::Operation *const cast = ca->getRhs<hldb::Operation>();
  ASSERT_NE(cast, nullptr) << "the RHS int'(a) is an Operation";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1: a cast";
  ExpectInt(getCastTarget(cast), "the cast target");
  const std::vector<const hldb::Any *> args = getCastArguments(cast);
  ASSERT_EQ(args.size(), 1u) << "37.59 detail 3: a cast has exactly one expression being cast";
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(args[0]);
  ASSERT_NE(a, nullptr) << "the expression being cast is the name 'a'";
  EXPECT_EQ(a->getName(), "a");
  EXPECT_EQ(a->getActual(), hldb::findByName<hldb::Net>("a", m->getNets())) << "a is bound to nmi_gen's net a";
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamTypespec2Test, TopHasOutputIntVariableO) {
  const hldb::Module *const m = getModule("top");
  ASSERT_NE(m, nullptr) << "module 'top' not found";
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  const hldb::Port *const p = m->getPorts()->at(0);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getName(), "o");
  EXPECT_EQ(p->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
  EXPECT_EQ(p->getDirection(), vpiOutput);
  ExpectInt(p->getTypespec(), "port o");
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 1u) << "top declares exactly one variable, o";
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("o", m->getVariables());
  ASSERT_NE(v, nullptr) << "23.2.2.3: an output port with an explicit data type is a variable";
  ExpectInt(v->getTypespec(), "variable o");
  EXPECT_TRUE(m->getNets() == nullptr || m->getNets()->empty()) << "top declares no net";
  const hldb::RefObj *const low = p->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr) << "37.14: the port's low connection";
  EXPECT_EQ(low->getActual(), v) << "the port o connects to the variable o";
}

TEST_F(ParamTypespec2Test, UNmiGenInstantiatesNmiGenByName) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u) << "top holds exactly one instance";
  const hldb::RefInstance *const inst = getUNmiGen();
  ASSERT_NE(inst, nullptr) << "instance 'u_nmi_gen' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "u_nmi_gen is a module instance";
  const hldb::Module *const nmiGen = getModule("nmi_gen");
  ASSERT_NE(nmiGen, nullptr);
  EXPECT_EQ(mt->getModule(), nmiGen) << "u_nmi_gen instantiates nmi_gen";
  EXPECT_TRUE(mt->getParamAssigns() == nullptr || mt->getParamAssigns()->empty()) << "u_nmi_gen overrides nothing";
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 2u) << "u_nmi_gen writes two connections";
  const std::string_view names[] = {"a", "x"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::NamedArgument *const na = inst->getArguments()->at(i);
    ASSERT_NE(na, nullptr) << "connection " << i;
    EXPECT_TRUE(na->getConnByName()) << "23.3.2.2: '." << names[i] << "(...)' connects by name";
    const hldb::RefObj *const low = na->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr) << "connection " << i << " names its port";
    EXPECT_EQ(low->getName(), names[i]) << "connection " << i << ", in source order";
    EXPECT_EQ(low->getActual(), getPort(nmiGen, names[i]))
        << "23.3.2.2: '" << names[i] << "' is the name of a port of nmi_gen";
  }
}

TEST_F(ParamTypespec2Test, PortAGetsThreeCopiesOfEscTxDefault) {
  const hldb::NamedArgument *const na = getConnection(getUNmiGen(), "a");
  ASSERT_NE(na, nullptr) << "connection '.a' not found";
  const hldb::Operation *const repl = na->getHighConn<hldb::Operation>();
  ASSERT_NE(repl, nullptr) << "'{3{...}}' is an Operation";
  EXPECT_EQ(repl->getOpType(), vpiMultiConcatOp) << "11.4.12.1: a replication";
  ASSERT_NE(repl->getOperands(), nullptr);
  ASSERT_GE(repl->getOperands()->size(), 2u);
  const hldb::Constant *const multiplier = any_cast<hldb::Constant>(repl->getOperands()->at(0));
  ASSERT_NE(multiplier, nullptr) << "37.59 detail 1: the first operand is the multiplier";
  EXPECT_EQ(multiplier->getDecompile(), "3");
  uint64_t count = 0;
  ASSERT_TRUE(getIntValue(multiplier, &count)) << "unparsable constant '" << multiplier->getDecompile() << "'";
  EXPECT_EQ(count, 3u);
  const std::vector<const hldb::Any *> items = getReplicatedItems(repl);
  ASSERT_EQ(items.size(), 1u) << "37.59 detail 1: the concatenation holds one expression";
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(items[0]);
  ASSERT_NE(ref, nullptr) << "the concatenated expression is the name prim_esc_pkg::ESC_TX_DEFAULT";
  ASSERT_NE(getEscTxDefault(), nullptr);
  EXPECT_EQ(ref->getActual(), getEscTxDefault()) << "26.3: prim_esc_pkg::ESC_TX_DEFAULT is the package's parameter";
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  bool topImports = false;
  if (top->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *top->getTypespecs()) {
      if (any_cast<hldb::ImportTypespec>(ts) != nullptr) topImports = true;
    }
  }
  EXPECT_FALSE(topImports) << "26.3: top writes no import, so the binding comes from the '::' operator alone";
}

TEST_F(ParamTypespec2Test, PortXDrivesO) {
  const hldb::NamedArgument *const na = getConnection(getUNmiGen(), "x");
  ASSERT_NE(na, nullptr) << "connection '.x' not found";
  const hldb::RefObj *const high = na->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr) << "'.x(o)' connects the name 'o'";
  EXPECT_EQ(high->getName(), "o");
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(high->getActual(), hldb::findByName<hldb::Variable>("o", top->getVariables()))
      << "o is bound to top's variable o";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamTypespec2Test, EscTxDefaultReducesToOne) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("prim_esc_pkg", m_design->getTopPackages());
    ASSERT_NE(pkg, nullptr) << "the elaborated prim_esc_pkg not found";
    const hldb::ParamAssign *const pa = getParamAssign(pkg, "ESC_TX_DEFAULT");
    ASSERT_NE(pa, nullptr);
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: the value should be reduced to a Constant";
    EXPECT_EQ(c->getSize(), 2) << "7.2.1: esc_tx_t packs two 1-bit members";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 1u) << "7.2.1: esc_p = 0 is the most significant bit and esc_n = 1 the least";
  }
}

TEST_F(ParamTypespec2Test, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: nmi_gen is instantiated by top";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->getDefName(), "top");
    const hldb::Module *const inst = hldb::findByName<hldb::Module>("u_nmi_gen", top->getModules());
    ASSERT_NE(inst, nullptr) << "the instance u_nmi_gen is elaborated";
    EXPECT_EQ(inst->getDefName(), "nmi_gen");
  }
}

TEST_F(ParamTypespec2Test, NoFatalSyntaxOrErrorDiagnostics) {
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
