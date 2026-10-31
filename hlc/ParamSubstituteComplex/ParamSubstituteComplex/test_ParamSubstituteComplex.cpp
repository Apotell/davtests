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

// Validates the HLDB model built for tests/ParamSubstituteComplex/dut.sv:
//
//   module GOOD ();
//   endmodule
//
//   module top();
//
//   typedef logic [0:4]       fmt_logic_t;
//
//   typedef struct packed {
//       int unsigned Width;
//       logic        EnableVectors;
//       logic        EnableNanBox;
//       fmt_logic_t  FpFmtMask;
//       fmt_logic_t IntFmtMask;
//     } fpu_features_t;
//
//   localparam fpu_features_t RV64D_Xsflt = '{
//       Width:         64,
//       EnableVectors: 1'b1,
//       EnableNanBox:  1'b1,
//       FpFmtMask:     5'b11111,
//       IntFmtMask:    4'b1111
//     };
//    localparam XLEN = 64;
//
//   localparam IS_XLEN64  = (XLEN == 32) ? 1'b0 : 1'b1;
//   localparam bit RVF = IS_XLEN64;
//   localparam bit RVD = IS_XLEN64;
//   localparam bit XF16    = 1'b0;
//   localparam bit XF16ALT = 1'b0;
//   localparam bit XF8     = 1'b0;
//   localparam bit XFVEC   = 1'b0;
//
//   localparam fpu_features_t FPU_FEATURES = '{
//         Width:         64,
//         EnableVectors: XFVEC,
//         EnableNanBox:  1'b1,
//         FpFmtMask:     {RVF, RVD, XF16, XF8, XF16ALT},
//         IntFmtMask:    {XFVEC && XF8, XFVEC && (XF16 || XF16ALT), 1'b1, 1'b1}
//       };
//    parameter fpu_features_t   Features       = FPU_FEATURES;
//    parameter fmt_logic_t      FpFmtMask     = Features.FpFmtMask;
//
//   localparam DEBUGME = FpFmtMask[0];
//
//   if (FpFmtMask == 5'b11000) begin
//     GOOD good1();
//   end
//
//   if (DEBUGME == 1) begin
//     GOOD good2();
//   end
//
//   endmodule
//
// The point of the fixture is substituting a member of a struct-valued
// parameter into another parameter: FPU_FEATURES is a packed structure built
// with a named assignment pattern (10.9.2) whose members are themselves
// computed from other local parameters, Features takes all of it, and
// FpFmtMask then takes one member of Features (6.20.2: "an individual member
// of an aggregate parameter may be used in an expression"). The result is
// consumed by a bit-select of a [0:4] (ascending) vector and by two
// if-generate constructs (27.5). The regression this file exists to catch is
// HLC losing the member value on the way through Features, binding the
// member name FpFmtMask to the module parameter of the same name, or reading
// bit [0] of an ascending range as the least significant bit, any of which
// drops good1 or good2.
//
// Values: XLEN = 64, so (XLEN == 32) is false and IS_XLEN64 = 1'b1 (11.4.11).
// RVF = RVD = 1 and XF16 = XF16ALT = XF8 = XFVEC = 0. The FpFmtMask member of
// FPU_FEATURES is {RVF, RVD, XF16, XF8, XF16ALT} = 5'b11000 (11.4.12).
// Features = FPU_FEATURES, so FpFmtMask = Features.FpFmtMask = 5'b11000 = 24.
// fmt_logic_t is [0:4], so index 0 is the most significant bit (6.9.1) and
// DEBUGME = FpFmtMask[0] = RVF = 1. Both if-generate conditions hold
// (5'b11000 == 5'b11000 and 1 == 1), so both blocks are instantiated: the
// first unnamed block is genblk1 and holds good1, the second is genblk2 and
// holds good2 (27.6).
//
// What is checked, and why:
//   Design
//     - exactly 2 modules, GOOD and top
//     - each '()' declares one null port (37.14 detail 10, "module M();"):
//       no name (detail 8), port index 0 (detail 9), no low connection
//   Types of top
//     - exactly 2 typedefs, fmt_logic_t and fpu_features_t (6.18)
//     - fmt_logic_t is a logic with the single packed range [0:4], unsigned
//       (6.9.1)
//     - fpu_features_t is a packed structure, unsigned by default (7.2.1),
//       with exactly 5 members in source order: Width ('int unsigned', an
//       unsigned int, 6.11.3), EnableVectors and EnableNanBox (logic with no
//       packed range), FpFmtMask and IntFmtMask (both fmt_logic_t)
//   Parameters of top
//     - exactly 13, in source order
//     - all are local except Features and FpFmtMask: 'module top();' writes
//       a port list but no parameter_port_list, so 'parameter' is not a
//       synonym for 'localparam' there (6.20.1)
//     - RV64D_Xsflt, FPU_FEATURES and Features are fpu_features_t;
//       FpFmtMask is fmt_logic_t; RVF, RVD, XF16, XF16ALT, XF8 and XFVEC are
//       an unsigned bit with no packed range (6.11.3)
//     - exactly 13 ParamAssigns, in source order, each LHS bound to its own
//       Parameter
//   Unreduced values (the source form, in every run)
//     - RV64D_Xsflt and FPU_FEATURES are assignment patterns of exactly 5
//       keyed items in the order Width, EnableVectors, EnableNanBox,
//       FpFmtMask, IntFmtMask, and every key is bound to that member of
//       fpu_features_t (10.9.2)
//     - RV64D_Xsflt's items are 64, 1'b1, 1'b1, 5'b11111 and 4'b1111, the
//       based literals with the exact size and the binary constant type of
//       5.7.1
//     - FPU_FEATURES's items are 64, the name XFVEC, 1'b1, the concatenation
//       {RVF, RVD, XF16, XF8, XF16ALT} in that order, and the concatenation
//       of (XFVEC && XF8), (XFVEC && (XF16 || XF16ALT)), 1'b1 and 1'b1; every
//       name is bound to top's Parameter
//     - XLEN is 64; IS_XLEN64 is the conditional operator over
//       (XLEN == 32), 1'b0 and 1'b1, in that order; RVF and RVD are the name
//       IS_XLEN64; XF16, XF16ALT, XF8 and XFVEC are 1'b0
//     - Features is the name FPU_FEATURES
//     - FpFmtMask is the member select Features.FpFmtMask: the prefix is
//       bound to the Parameter Features and the selected name to the member
//       FpFmtMask of fpu_features_t, not to the Parameter FpFmtMask
//     - DEBUGME is the bit-select FpFmtMask[0] whose prefix is bound to the
//       Parameter FpFmtMask
//   Generate constructs of top
//     - exactly 2 if-generates with no else (27.5), in source order
//     - the first tests FpFmtMask == 5'b11000 and its block holds exactly 1
//       instance, good1 of GOOD
//     - the second tests DEBUGME == 1 and its block holds exactly 1
//       instance, good2 of GOOD
//   Elaboration
//     - IS_XLEN64 reduces to 1, FpFmtMask to 24 (5'b11000) and DEBUGME to 1
//     - the untyped IS_XLEN64 and DEBUGME are logic [0:0] and XLEN is a
//       logic vector [N:0] with N at least 31 (6.20.2)
//     - genblk1 holds good1 and genblk2 holds good2, both of GOOD (27.5,
//       27.6)
//     - GOOD appears in instantiation statements, so top is the only
//       top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the unreduced values, bindings and generate
// constructs are asserted on the definitions in Design::getAllModules(),
// which hold the source form in every run. The reduced values, the implied
// types of the untyped parameters and the instance tree are asserted only
// when the design is elaborated, on Design::getTopModules().
//
// KNOWN COMPILER BUG (pattern member key bound to parameter), not a defect in
// this test: HLC binds the key 'FpFmtMask' in both named assignment patterns,
// 'FpFmtMask: 5'b11111' in RV64D_Xsflt and 'FpFmtMask: {RVF, ...}' in
// FPU_FEATURES, to the module parameter FpFmtMask (declared later, on the
// line 'parameter fmt_logic_t FpFmtMask = Features.FpFmtMask;') instead of
// to the member FpFmtMask of fpu_features_t, although 10.9.2 says "A member:value
// specifies an explicit value for a named member of the structure." The
// other four keys of each pattern are bound to their members.
// Rv64dXsfltKeysNameStructMembers and FpuFeaturesKeysNameStructMembers are
// expected to fail until HLC is fixed; they are intentionally not skipped or
// relaxed.
//
// Model convention: HLC holds each 'key: value' item of a named assignment
// pattern as a TaggedPattern whose tag is a RefObj naming the key and whose
// pattern is the value. The tests read the key's binding from that RefObj;
// the TaggedPattern holds no other reference to the member.
//
// Model convention: HLC holds the member select Features.FpFmtMask as a
// RefObj whose path elements are the prefix 'Features' and the selected
// member 'FpFmtMask'. FpFmtMaskIsMemberOfFeatures follows that shape and
// asserts what each element is bound to.
//
// Model convention: a reference to a typedef'd type may resolve to the
// TypedefTypespec or to the type it aliases. Both are that type (6.18), so
// either is accepted.
//
// What is NOT checked, and why:
//   - The constant type of the unsized decimal literals 64, 32 and 1. The
//     standard does not say whether an unsized decimal is a vpiDecConst or a
//     vpiIntConst, so only the literal text and the minimum size of 32 bits
//     (5.7.1) are asserted. The based literals are asserted to be
//     vpiBinaryConst.
//   - The reduced value of the struct-valued parameters RV64D_Xsflt,
//     FPU_FEATURES and Features. Whether HLC keeps such a value as a
//     pattern of reduced members or folds it into one 44-bit vector is a
//     tool convention; the member that matters, FpFmtMask, is asserted
//     through the parameter of the same name.
//   - The external name of the unnamed generate blocks in the definition.
//     The names genblk1 and genblk2 (27.6) belong to the instantiated scopes
//     and are asserted on the elaborated top.
//   - The port connection list of good1 and good2. Whether 'GOOD good1();'
//     against GOOD's single null port is modeled as no connection or as one
//     empty connection is not decided by the standard.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/int_typespec.h>
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
#include <vector>

namespace hlc {

class ParamSubstituteComplexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamSubstituteComplex.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Module *getTop() { return getModule("top"); }

  // The elaborated top-level instance of top.
  static const hldb::Module *getElabTop() {
    return hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
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

  static const hldb::Typedef *getTypedef(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, top->getTypedefs());
  }

  // Whether 'rts' names top's typedef 'name'. Model convention: the reference
  // may resolve to the TypedefTypespec or to the type it aliases.
  static bool namesTypedef(const hldb::RefTypespec *rts, std::string_view name) {
    const hldb::Typedef *const td = getTypedef(name);
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr || rts->getActual() == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = rts->getActual<hldb::TypedefTypespec>();
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
           (rts->getActual() == td->getAlias()->getActual());
  }

  // The structure fpu_features_t aliases.
  static const hldb::Struct *getFeaturesStruct() {
    const hldb::Typedef *const td = getTypedef("fpu_features_t");
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr) return nullptr;
    return st->getStruct();
  }

  // The member 'name' of fpu_features_t.
  static const hldb::TypespecMember *getMember(std::string_view name) {
    const hldb::Struct *const s = getFeaturesStruct();
    if (s == nullptr || s->getMembers() == nullptr) return nullptr;
    for (const hldb::TypespecMember *const m : *s->getMembers()) {
      if (m->getName() == name) return m;
    }
    return nullptr;
  }

  // The unreduced value of top's parameter 'name'.
  static const hldb::Any *getDefault(std::string_view name) {
    const hldb::ParamAssign *const pa = getParamAssign(getTop(), name);
    if (pa == nullptr) return nullptr;
    return pa->getRhs();
  }

  // The keyed item at 'index' of the assignment pattern 'pattern'.
  static const hldb::TaggedPattern *getItem(const hldb::Operation *pattern, size_t index) {
    if (pattern == nullptr || pattern->getOperands() == nullptr || index >= pattern->getOperands()->size()) {
      return nullptr;
    }
    return any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(index));
  }

  static const hldb::GenIf *getGenIf(size_t index) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getGenStmts() == nullptr || index >= top->getGenStmts()->size()) return nullptr;
    return any_cast<hldb::GenIf>(top->getGenStmts()->at(index));
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

  // Verifies 'e' is the based binary literal 'text' of 'size' bits.
  static void ExpectBinaryLiteral(const hldb::Any *e, std::string_view text, int32_t size, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": expected the Constant " << text;
    EXPECT_EQ(c->getDecompile(), text) << why;
    EXPECT_EQ(c->getConstType(), vpiBinaryConst) << why << ": 5.7.1, the base format 'b is binary";
    EXPECT_EQ(c->getSize(), size) << why << ": 5.7.1, the size constant gives the exact number of bits";
  }

  // Verifies 'e' is the unsized decimal literal 'text'.
  static void ExpectUnsizedLiteral(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": expected the Constant " << text;
    EXPECT_EQ(c->getDecompile(), text) << why;
    EXPECT_GE(c->getSize(), 32) << why << ": 5.7.1, an unsized number has at least 32 bits";
  }

  // Verifies 'e' is a RefObj named 'name' bound to top's Parameter 'name'.
  static void ExpectRefToParam(const hldb::Any *e, std::string_view name, std::string_view why) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(e);
    ASSERT_NE(ref, nullptr) << why << ": expected the name " << name;
    EXPECT_EQ(ref->getName(), name) << why;
    const hldb::Parameter *const p = getParam(getTop(), name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_EQ(ref->getActual(), p) << why << ": '" << name << "' is top's parameter";
  }

  // Verifies 'e' is Operation 'opType' over exactly 'count' operands.
  static void ExpectOperation(const hldb::Any *e, int32_t opType, size_t count, std::string_view why) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(e);
    ASSERT_NE(op, nullptr) << why << ": expected an Operation";
    EXPECT_EQ(op->getOpType(), opType) << why;
    ASSERT_NE(op->getOperands(), nullptr) << why;
    EXPECT_EQ(op->getOperands()->size(), count) << why;
  }

  // Verifies every key of the assignment pattern that 'param' defaults to is
  // bound to the member of fpu_features_t it names (10.9.2).
  static void ExpectKeysNameMembers(std::string_view param) {
    const hldb::Operation *const pattern = any_cast<hldb::Operation>(getDefault(param));
    ASSERT_NE(pattern, nullptr) << "unreduced, '" << param << "' is an assignment pattern";
    ASSERT_NE(pattern->getOperands(), nullptr);
    ASSERT_EQ(pattern->getOperands()->size(), 5u);
    const std::string_view members[] = {"Width", "EnableVectors", "EnableNanBox", "FpFmtMask", "IntFmtMask"};
    for (size_t i = 0; i < 5; ++i) {
      const hldb::TaggedPattern *const item = getItem(pattern, i);
      ASSERT_NE(item, nullptr) << param << " item " << i << " is keyed";
      const hldb::RefObj *const key = item->getTag<hldb::RefObj>();
      ASSERT_NE(key, nullptr) << param << " item " << i << " is keyed by a member name";
      EXPECT_EQ(key->getName(), members[i]) << param << " item " << i;
      const hldb::TypespecMember *const member = getMember(members[i]);
      ASSERT_NE(member, nullptr) << "member '" << members[i] << "' of fpu_features_t not found";
      EXPECT_EQ(key->getActual(), member) << "10.9.2: \"A member:value specifies an explicit value for a named "
                                             "member of the structure.\" The key '"
                                          << members[i] << "' of " << param << " names the member of fpu_features_t";
    }
  }

  // Verifies the block of 'gi' holds exactly one instance, 'name', of GOOD.
  static void ExpectBlockInstantiatesGood(const hldb::GenIf *gi, std::string_view name) {
    const hldb::GenBlock *const blk = gi->getStmt<hldb::GenBlock>();
    ASSERT_NE(blk, nullptr) << "the block is written with begin ... end";
    ASSERT_NE(blk->getStmts(), nullptr);
    ASSERT_EQ(blk->getStmts()->size(), 1u) << "the block holds one item";
    const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(blk->getStmts()->at(0));
    ASSERT_NE(inst, nullptr) << "the item is a module instantiation";
    EXPECT_EQ(inst->getName(), name);
    ASSERT_NE(inst->getTypespec(), nullptr);
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mt, nullptr);
    ASSERT_NE(getModule("GOOD"), nullptr);
    EXPECT_EQ(mt->getModule(), getModule("GOOD")) << "'" << name << "' instantiates GOOD";
    EXPECT_TRUE(mt->getParamAssigns() == nullptr || mt->getParamAssigns()->empty())
        << "'" << name << "' writes no parameter value assignment";
  }

  // Verifies 'e' is a Constant whose integer value is 'expected'.
  static void ExpectIntConstant(const hldb::Any *e, uint64_t expected, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }

  // Verifies the elaborated top's 'block' holds exactly one instance, 'name',
  // of GOOD.
  static void ExpectElabBlockHoldsGood(std::string_view block, std::string_view name, std::string_view why) {
    const hldb::Module *const top = getElabTop();
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::GenScopeArray *const gen = hldb::findByName<hldb::GenScopeArray>(block, top->getGenScopeArrays());
    ASSERT_NE(gen, nullptr) << why;
    ASSERT_NE(gen->getGenScopes(), nullptr);
    ASSERT_EQ(gen->getGenScopes()->size(), 1u) << "an if-generate instantiates at most one block";
    const hldb::GenScope *const scope = gen->getGenScopes()->at(0);
    ASSERT_NE(scope, nullptr);
    ASSERT_NE(scope->getModules(), nullptr) << why;
    ASSERT_EQ(scope->getModules()->size(), 1u) << "the block holds the one instance '" << name << "'";
    const hldb::Module *const inst = scope->getModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), name);
    EXPECT_EQ(inst->getDefName(), "GOOD");
  }
};

// ---------------------------------------------------------------------------
// Design and ports
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, DesignHoldsGoodAndTop) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u) << "the fixture declares the modules GOOD and top";
  EXPECT_NE(getModule("GOOD"), nullptr) << "module 'GOOD' not found";
  EXPECT_NE(getModule("top"), nullptr) << "module 'top' not found";
}

TEST_F(ParamSubstituteComplexTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"GOOD", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << name << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: " << name << "'s '()' declares exactly one null port";
    const hldb::Port *const port = m->getPorts()->at(0);
    EXPECT_EQ(port->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9: the first port has a port index of zero";
    EXPECT_EQ(port->getLowConn(), nullptr) << "37.14 detail 10: a null port has no low connection";
  }
}

// ---------------------------------------------------------------------------
// Types of top
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, TopDeclaresTwoTypedefs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 2u) << "6.18: top declares the two types fmt_logic_t and fpu_features_t";
  EXPECT_NE(getTypedef("fmt_logic_t"), nullptr) << "typedef 'fmt_logic_t' not found in top";
  EXPECT_NE(getTypedef("fpu_features_t"), nullptr) << "typedef 'fpu_features_t' not found in top";
}

TEST_F(ParamSubstituteComplexTest, FmtLogicTIsLogic0To4) {
  const hldb::Typedef *const td = getTypedef("fmt_logic_t");
  ASSERT_NE(td, nullptr) << "typedef 'fmt_logic_t' not found in top";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::LogicTypespec *const lt = td->getAlias()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "fmt_logic_t is a logic";
  EXPECT_FALSE(lt->getSigned()) << "6.9.1: logic vectors are unsigned unless declared signed";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u) << "one packed dimension";
  const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "0") << "6.9.1: the msb, the left-hand value, is 0";
  const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "4") << "6.9.1: the lsb, the right-hand value, is 4";
}

TEST_F(ParamSubstituteComplexTest, FpuFeaturesTIsPackedUnsignedStructOfFiveMembers) {
  const hldb::Typedef *const td = getTypedef("fpu_features_t");
  ASSERT_NE(td, nullptr) << "typedef 'fpu_features_t' not found in top";
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual<hldb::StructTypespec>(), nullptr) << "fpu_features_t is a structure";
  const hldb::Struct *const s = getFeaturesStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked()) << "7.2: declared 'struct packed'";
  EXPECT_FALSE(s->getSigned()) << "7.2.1: a packed structure is unsigned by default";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 5u);
  const std::string_view names[] = {"Width", "EnableVectors", "EnableNanBox", "FpFmtMask", "IntFmtMask"};
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(s->getMembers()->at(i)->getName(), names[i]) << "7.2.1: member " << i << " in declaration order";
  }
}

TEST_F(ParamSubstituteComplexTest, FpuFeaturesTMemberTypes) {
  const hldb::TypespecMember *const width = getMember("Width");
  ASSERT_NE(width, nullptr) << "member 'Width' not found";
  ASSERT_NE(width->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = width->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "Width is an int";
  EXPECT_FALSE(it->getSigned()) << "6.11.3: 'int unsigned' is unsigned";
  for (std::string_view name : {"EnableVectors", "EnableNanBox"}) {
    const hldb::TypespecMember *const m = getMember(name);
    ASSERT_NE(m, nullptr) << "member '" << name << "' not found";
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << name << " is a logic";
    EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << name << " has no packed range";
  }
  for (std::string_view name : {"FpFmtMask", "IntFmtMask"}) {
    const hldb::TypespecMember *const m = getMember(name);
    ASSERT_NE(m, nullptr) << "member '" << name << "' not found";
    EXPECT_TRUE(namesTypedef(m->getTypespec(), "fmt_logic_t")) << name << " is declared 'fmt_logic_t'";
  }
}

// ---------------------------------------------------------------------------
// Parameters of top
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, TopDeclaresThirteenParametersInOrder) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 13u);
  const std::string_view names[] = {"RV64D_Xsflt", "XLEN",      "IS_XLEN64", "RVF",   "RVD",
                                    "XF16",        "XF16ALT",   "XF8",       "XFVEC", "FPU_FEATURES",
                                    "Features",    "FpFmtMask", "DEBUGME"};
  for (size_t i = 0; i < 13; ++i) {
    const hldb::Parameter *const p = any_cast<hldb::Parameter>(top->getParameters()->at(i));
    ASSERT_NE(p, nullptr) << "position " << i << " is a value parameter";
    EXPECT_EQ(p->getName(), names[i]) << "position " << i << " follows the order of declaration";
  }
}

TEST_F(ParamSubstituteComplexTest, OnlyFeaturesAndFpFmtMaskAreOverridable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (std::string_view name : {"RV64D_Xsflt", "XLEN", "IS_XLEN64", "RVF", "RVD", "XF16", "XF16ALT", "XF8", "XFVEC",
                                "FPU_FEATURES", "DEBUGME"}) {
    const hldb::Parameter *const p = getParam(top, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << name << "' is declared with 'localparam'";
  }
  for (std::string_view name : {"Features", "FpFmtMask"}) {
    const hldb::Parameter *const p = getParam(top, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: 'module top();' has no parameter_port_list, so '" << name
                                     << "', declared with 'parameter', is overridable";
  }
}

TEST_F(ParamSubstituteComplexTest, TypedParametersHaveTheirDeclaredTypes) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (std::string_view name : {"RV64D_Xsflt", "FPU_FEATURES", "Features"}) {
    const hldb::Parameter *const p = getParam(top, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_TRUE(namesTypedef(p->getTypespec(), "fpu_features_t")) << "'" << name << "' is declared 'fpu_features_t'";
  }
  const hldb::Parameter *const mask = getParam(top, "FpFmtMask");
  ASSERT_NE(mask, nullptr) << "parameter 'FpFmtMask' not found";
  EXPECT_TRUE(namesTypedef(mask->getTypespec(), "fmt_logic_t")) << "'FpFmtMask' is declared 'fmt_logic_t'";
  for (std::string_view name : {"RVF", "RVD", "XF16", "XF16ALT", "XF8", "XFVEC"}) {
    const hldb::Parameter *const p = getParam(top, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    ASSERT_NE(p->getTypespec(), nullptr) << "'" << name << "' is declared 'bit'";
    const hldb::BitTypespec *const bt = p->getTypespec()->getActual<hldb::BitTypespec>();
    ASSERT_NE(bt, nullptr) << "'" << name << "' is declared 'bit'";
    EXPECT_FALSE(bt->getSigned()) << "6.11.3: bit defaults to unsigned";
    EXPECT_TRUE(bt->getRanges() == nullptr || bt->getRanges()->empty()) << "'" << name << "' has no packed range";
  }
}

TEST_F(ParamSubstituteComplexTest, EveryParamAssignLhsBindsToItsParameter) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParamAssigns(), nullptr);
  ASSERT_EQ(top->getParamAssigns()->size(), 13u) << "each of the 13 param_assignments has one ParamAssign";
  const std::string_view names[] = {"RV64D_Xsflt", "XLEN",      "IS_XLEN64", "RVF",   "RVD",
                                    "XF16",        "XF16ALT",   "XF8",       "XFVEC", "FPU_FEATURES",
                                    "Features",    "FpFmtMask", "DEBUGME"};
  for (size_t i = 0; i < 13; ++i) {
    const hldb::RefObj *const lhs = top->getParamAssigns()->at(i)->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "ParamAssign " << i;
    EXPECT_EQ(lhs->getName(), names[i]) << "ParamAssign " << i << " in source order";
    ASSERT_NE(getParam(top, names[i]), nullptr);
    EXPECT_EQ(lhs->getActual(), getParam(top, names[i])) << "the LHS of '" << names[i] << "' is its own Parameter";
  }
}

// ---------------------------------------------------------------------------
// RV64D_Xsflt and FPU_FEATURES
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (pattern
// member key bound to parameter) in the file header.
TEST_F(ParamSubstituteComplexTest, Rv64dXsfltKeysNameStructMembers) { ExpectKeysNameMembers("RV64D_Xsflt"); }

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (pattern
// member key bound to parameter) in the file header.
TEST_F(ParamSubstituteComplexTest, FpuFeaturesKeysNameStructMembers) { ExpectKeysNameMembers("FPU_FEATURES"); }

TEST_F(ParamSubstituteComplexTest, Rv64dXsfltIsNamedPatternOfLiterals) {
  const hldb::Any *const value = getDefault("RV64D_Xsflt");
  ExpectOperation(value, vpiAssignmentPatternOp, 5u, "10.9.2: RV64D_Xsflt is a structure assignment pattern");
  const hldb::Operation *const pattern = any_cast<hldb::Operation>(value);
  ASSERT_NE(pattern, nullptr);
  const std::string_view keys[] = {"Width", "EnableVectors", "EnableNanBox", "FpFmtMask", "IntFmtMask"};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::TaggedPattern *const item = getItem(pattern, i);
    ASSERT_NE(item, nullptr) << "item " << i << " is keyed";
    ASSERT_NE(item->getTag<hldb::RefObj>(), nullptr) << "item " << i << " is keyed by a member name";
    EXPECT_EQ(item->getTag<hldb::RefObj>()->getName(), keys[i]) << "item " << i << " in source order";
  }
  ExpectUnsizedLiteral(getItem(pattern, 0)->getPattern(), "64", "Width: 64");
  ExpectBinaryLiteral(getItem(pattern, 1)->getPattern(), "1'b1", 1, "EnableVectors: 1'b1");
  ExpectBinaryLiteral(getItem(pattern, 2)->getPattern(), "1'b1", 1, "EnableNanBox: 1'b1");
  ExpectBinaryLiteral(getItem(pattern, 3)->getPattern(), "5'b11111", 5, "FpFmtMask: 5'b11111");
  ExpectBinaryLiteral(getItem(pattern, 4)->getPattern(), "4'b1111", 4, "IntFmtMask: 4'b1111");
}

TEST_F(ParamSubstituteComplexTest, FpuFeaturesIsNamedPatternOfParameters) {
  const hldb::Any *const value = getDefault("FPU_FEATURES");
  ExpectOperation(value, vpiAssignmentPatternOp, 5u, "10.9.2: FPU_FEATURES is a structure assignment pattern");
  const hldb::Operation *const pattern = any_cast<hldb::Operation>(value);
  ASSERT_NE(pattern, nullptr);
  const std::string_view keys[] = {"Width", "EnableVectors", "EnableNanBox", "FpFmtMask", "IntFmtMask"};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::TaggedPattern *const item = getItem(pattern, i);
    ASSERT_NE(item, nullptr) << "item " << i << " is keyed";
    ASSERT_NE(item->getTag<hldb::RefObj>(), nullptr) << "item " << i << " is keyed by a member name";
    EXPECT_EQ(item->getTag<hldb::RefObj>()->getName(), keys[i]) << "item " << i << " in source order";
  }
  ExpectUnsizedLiteral(getItem(pattern, 0)->getPattern(), "64", "Width: 64");
  ExpectRefToParam(getItem(pattern, 1)->getPattern(), "XFVEC", "EnableVectors: XFVEC");
  ExpectBinaryLiteral(getItem(pattern, 2)->getPattern(), "1'b1", 1, "EnableNanBox: 1'b1");
}

TEST_F(ParamSubstituteComplexTest, FpuFeaturesFpFmtMaskIsConcatOfFiveFlags) {
  const hldb::Operation *const pattern = any_cast<hldb::Operation>(getDefault("FPU_FEATURES"));
  ASSERT_NE(pattern, nullptr) << "unreduced, FPU_FEATURES is an assignment pattern";
  const hldb::TaggedPattern *const item = getItem(pattern, 3);
  ASSERT_NE(item, nullptr) << "item 3 (FpFmtMask) is keyed";
  ExpectOperation(item->getPattern(), vpiConcatOp, 5u, "11.4.12: {RVF, RVD, XF16, XF8, XF16ALT}");
  const hldb::Operation *const concat = item->getPattern<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 5u);
  const std::string_view flags[] = {"RVF", "RVD", "XF16", "XF8", "XF16ALT"};
  for (size_t i = 0; i < 5; ++i) {
    ExpectRefToParam(concat->getOperands()->at(i), flags[i], "11.4.12: concatenation operands in source order");
  }
}

TEST_F(ParamSubstituteComplexTest, FpuFeaturesIntFmtMaskIsConcatOfFour) {
  const hldb::Operation *const pattern = any_cast<hldb::Operation>(getDefault("FPU_FEATURES"));
  ASSERT_NE(pattern, nullptr) << "unreduced, FPU_FEATURES is an assignment pattern";
  const hldb::TaggedPattern *const item = getItem(pattern, 4);
  ASSERT_NE(item, nullptr) << "item 4 (IntFmtMask) is keyed";
  ExpectOperation(item->getPattern(), vpiConcatOp, 4u, "11.4.12: a concatenation of 4 items");
  const hldb::Operation *const concat = item->getPattern<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 4u);

  // XFVEC && XF8
  const hldb::Any *const first = concat->getOperands()->at(0);
  ExpectOperation(first, vpiLogAndOp, 2u, "11.4.7: XFVEC && XF8");
  const hldb::Operation *const and1 = any_cast<hldb::Operation>(first);
  ASSERT_NE(and1, nullptr);
  ASSERT_NE(and1->getOperands(), nullptr);
  ASSERT_EQ(and1->getOperands()->size(), 2u);
  ExpectRefToParam(and1->getOperands()->at(0), "XFVEC", "the left operand of XFVEC && XF8");
  ExpectRefToParam(and1->getOperands()->at(1), "XF8", "the right operand of XFVEC && XF8");

  // XFVEC && (XF16 || XF16ALT)
  const hldb::Any *const second = concat->getOperands()->at(1);
  ExpectOperation(second, vpiLogAndOp, 2u, "11.4.7: XFVEC && (XF16 || XF16ALT)");
  const hldb::Operation *const and2 = any_cast<hldb::Operation>(second);
  ASSERT_NE(and2, nullptr);
  ASSERT_NE(and2->getOperands(), nullptr);
  ASSERT_EQ(and2->getOperands()->size(), 2u);
  ExpectRefToParam(and2->getOperands()->at(0), "XFVEC", "the left operand of XFVEC && (XF16 || XF16ALT)");
  ExpectOperation(and2->getOperands()->at(1), vpiLogOrOp, 2u, "11.4.7: XF16 || XF16ALT");
  const hldb::Operation *const orOp = any_cast<hldb::Operation>(and2->getOperands()->at(1));
  ASSERT_NE(orOp, nullptr);
  ASSERT_NE(orOp->getOperands(), nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2u);
  ExpectRefToParam(orOp->getOperands()->at(0), "XF16", "the left operand of XF16 || XF16ALT");
  ExpectRefToParam(orOp->getOperands()->at(1), "XF16ALT", "the right operand of XF16 || XF16ALT");

  ExpectBinaryLiteral(concat->getOperands()->at(2), "1'b1", 1, "the third item, 1'b1");
  ExpectBinaryLiteral(concat->getOperands()->at(3), "1'b1", 1, "the fourth item, 1'b1");
}

// ---------------------------------------------------------------------------
// The scalar local parameters
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, XlenIs64) { ExpectUnsizedLiteral(getDefault("XLEN"), "64", "XLEN = 64"); }

TEST_F(ParamSubstituteComplexTest, IsXlen64IsConditionalOnXlenEq32) {
  const hldb::Any *const value = getDefault("IS_XLEN64");
  ExpectOperation(value, vpiConditionOp, 3u, "11.4.11: (XLEN == 32) ? 1'b0 : 1'b1");
  const hldb::Operation *const cond = any_cast<hldb::Operation>(value);
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u);
  ExpectOperation(cond->getOperands()->at(0), vpiEqOp, 2u, "11.4.5: the condition XLEN == 32");
  const hldb::Operation *const eq = any_cast<hldb::Operation>(cond->getOperands()->at(0));
  ASSERT_NE(eq, nullptr);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  ExpectRefToParam(eq->getOperands()->at(0), "XLEN", "the left operand of XLEN == 32");
  ExpectUnsizedLiteral(eq->getOperands()->at(1), "32", "the right operand of XLEN == 32");
  ExpectBinaryLiteral(cond->getOperands()->at(1), "1'b0", 1, "11.4.11: the value if the condition is true");
  ExpectBinaryLiteral(cond->getOperands()->at(2), "1'b1", 1, "11.4.11: the value if the condition is false");
}

TEST_F(ParamSubstituteComplexTest, RvfAndRvdTakeIsXlen64) {
  ExpectRefToParam(getDefault("RVF"), "IS_XLEN64", "RVF = IS_XLEN64");
  ExpectRefToParam(getDefault("RVD"), "IS_XLEN64", "RVD = IS_XLEN64");
}

TEST_F(ParamSubstituteComplexTest, XfFlagsAreOneBitZero) {
  for (std::string_view name : {"XF16", "XF16ALT", "XF8", "XFVEC"}) {
    ExpectBinaryLiteral(getDefault(name), "1'b0", 1, name);
  }
}

// ---------------------------------------------------------------------------
// Features, FpFmtMask and DEBUGME
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, FeaturesDefaultsToFpuFeatures) {
  ExpectRefToParam(getDefault("Features"), "FPU_FEATURES", "Features = FPU_FEATURES");
}

TEST_F(ParamSubstituteComplexTest, FpFmtMaskIsMemberOfFeatures) {
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(getDefault("FpFmtMask"));
  ASSERT_NE(ref, nullptr) << "unreduced, FpFmtMask's default is the member select Features.FpFmtMask";
  ASSERT_NE(ref->getPathElems(), nullptr) << "Model convention: a member select is a RefObj path";
  ASSERT_EQ(ref->getPathElems()->size(), 2u) << "the prefix 'Features' and the member 'FpFmtMask'";
  const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(ref->getPathElems()->at(0));
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "Features");
  ASSERT_NE(getParam(getTop(), "Features"), nullptr);
  EXPECT_EQ(prefix->getActual(), getParam(getTop(), "Features")) << "the prefix is top's parameter Features";
  const hldb::RefObj *const member = any_cast<hldb::RefObj>(ref->getPathElems()->at(1));
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "FpFmtMask");
  ASSERT_NE(getMember("FpFmtMask"), nullptr);
  EXPECT_EQ(member->getActual(), getMember("FpFmtMask"))
      << "7.2: '.FpFmtMask' selects the member of fpu_features_t, not top's parameter FpFmtMask";
}

TEST_F(ParamSubstituteComplexTest, DebugmeIsBitZeroOfFpFmtMask) {
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(getDefault("DEBUGME"));
  ASSERT_NE(sel, nullptr) << "11.5.1: DEBUGME's default is the bit-select FpFmtMask[0]";
  ExpectRefToParam(sel->getPrefix(), "FpFmtMask", "the bit-select's prefix");
  ExpectUnsizedLiteral(sel->getIndex(), "0", "the bit-select's index");
}

// ---------------------------------------------------------------------------
// Generate constructs of top
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, TopHasTwoIfGenerates) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  ASSERT_EQ(top->getGenStmts()->size(), 2u) << "27.5: top writes two if-generate constructs";
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_NE(getGenIf(i), nullptr) << "27.5: construct " << i << " is an if-generate with no else";
  }
}

TEST_F(ParamSubstituteComplexTest, FirstIfGenerateTestsFpFmtMaskAndHoldsGood1) {
  const hldb::GenIf *const gi = getGenIf(0);
  ASSERT_NE(gi, nullptr) << "the first generate construct is an if-generate";
  ExpectOperation(gi->getCondition(), vpiEqOp, 2u, "11.4.5: FpFmtMask == 5'b11000");
  const hldb::Operation *const eq = gi->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  ExpectRefToParam(eq->getOperands()->at(0), "FpFmtMask", "the left operand of the condition");
  ExpectBinaryLiteral(eq->getOperands()->at(1), "5'b11000", 5, "the right operand of the condition");
  ExpectBlockInstantiatesGood(gi, "good1");
}

TEST_F(ParamSubstituteComplexTest, SecondIfGenerateTestsDebugmeAndHoldsGood2) {
  const hldb::GenIf *const gi = getGenIf(1);
  ASSERT_NE(gi, nullptr) << "the second generate construct is an if-generate";
  ExpectOperation(gi->getCondition(), vpiEqOp, 2u, "11.4.5: DEBUGME == 1");
  const hldb::Operation *const eq = gi->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  ExpectRefToParam(eq->getOperands()->at(0), "DEBUGME", "the left operand of the condition");
  ExpectUnsizedLiteral(eq->getOperands()->at(1), "1", "the right operand of the condition");
  ExpectBlockInstantiatesGood(gi, "good2");
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, IsXlen64ReducesToOne) {
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const pa = getParamAssign(getElabTop(), "IS_XLEN64");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'IS_XLEN64' in the elaborated top";
    ExpectIntConstant(pa->getRhs(), 1u, "11.4.11: 64 == 32 is false, so the value is 1'b1");
  }
}

TEST_F(ParamSubstituteComplexTest, FpFmtMaskReducesTo11000) {
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const pa = getParamAssign(getElabTop(), "FpFmtMask");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'FpFmtMask' in the elaborated top";
    ExpectIntConstant(pa->getRhs(), 24u,
                      "Features.FpFmtMask = FPU_FEATURES.FpFmtMask = {RVF, RVD, XF16, XF8, XF16ALT} = 5'b11000");
  }
}

TEST_F(ParamSubstituteComplexTest, DebugmeReducesToOne) {
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const pa = getParamAssign(getElabTop(), "DEBUGME");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'DEBUGME' in the elaborated top";
    ExpectIntConstant(pa->getRhs(), 1u, "6.9.1, 11.5.1: in [0:4], bit 0 is the msb of 5'b11000, which is 1");
  }
}

TEST_F(ParamSubstituteComplexTest, UntypedParametersTakeTheRangeOfTheirFinalValue) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = getElabTop();
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    for (std::string_view name : {"IS_XLEN64", "DEBUGME"}) {
      const hldb::Parameter *const p = getParam(top, name);
      ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found in the elaborated top";
      ASSERT_NE(p->getTypespec(), nullptr);
      const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
      ASSERT_NE(lt, nullptr) << "6.20.2: an untyped parameter with an integral value is a logic vector";
      ASSERT_NE(lt->getRanges(), nullptr) << "6.20.2: with range [size-1:0]";
      ASSERT_EQ(lt->getRanges()->size(), 1u);
      ExpectIntConstant(lt->getRanges()->at(0)->getLeftExpr(), 0u,
                        "6.20.2: the final value of '" + std::string(name) + "' has 1 bit, so the range is [0:0]");
      ExpectIntConstant(lt->getRanges()->at(0)->getRightExpr(), 0u, "6.20.2: the lsb is 0");
    }
    const hldb::Parameter *const xlen = getParam(top, "XLEN");
    ASSERT_NE(xlen, nullptr) << "parameter 'XLEN' not found in the elaborated top";
    ASSERT_NE(xlen->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = xlen->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "6.20.2: an untyped parameter with an integral value is a logic vector";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << "the msb should be reduced to a Constant";
    uint64_t msb = 0;
    ASSERT_TRUE(getIntValue(left, &msb)) << "unparsable constant '" << left->getDecompile() << "'";
    EXPECT_GE(msb, 31u) << "6.20.2: the final value 64 is unsized, so the msb is at least 31";
    ExpectIntConstant(lt->getRanges()->at(0)->getRightExpr(), 0u, "6.20.2: the lsb is 0");
  }
}

TEST_F(ParamSubstituteComplexTest, Genblk1HoldsGood1) {
  if (m_design->getElaborated()) {
    ExpectElabBlockHoldsGood("genblk1", "good1",
                             "27.5, 27.6: FpFmtMask == 5'b11000 holds, so the unnamed block of the first generate "
                             "construct, genblk1, is instantiated");
  }
}

TEST_F(ParamSubstituteComplexTest, Genblk2HoldsGood2) {
  if (m_design->getElaborated()) {
    ExpectElabBlockHoldsGood("genblk2", "good2",
                             "27.5, 27.6: DEBUGME == 1 holds, so the unnamed block of the second generate "
                             "construct, genblk2, is instantiated");
  }
}

TEST_F(ParamSubstituteComplexTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u)
        << "23.3.1: GOOD appears in module instantiation statements, so it is not top-level";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamSubstituteComplexTest, NoFatalSyntaxOrErrorDiagnostics) {
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
