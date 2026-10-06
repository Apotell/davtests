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

// Validates the HLDB model built for tests/PackDataType/dut.sv:
//
//   package keymgr_pkg;
//    parameter int KeyWidth = 256;
//    typedef struct packed {
//       logic ready;
//       logic done;
//       logic [KeyWidth-10:0] digest_share0;
//     } kmac_data_rsp_t;
//   endpackage
//
//   module OK();
//   endmodule
//
//   module kmac_keymgr(output keymgr_pkg::kmac_data_rsp_t keymgr_data_o);
//    localparam int KeyMgrDigestW = $bits(keymgr_data_o.digest_share0);
//    if (KeyMgrDigestW == 247)
//      OK ok();
//    logic [KeyMgrDigestW-1:0] keymgr_digest [2];
//   endmodule
//
// The point of the fixture is a port whose type is a packed structure from a
// package (IEEE 1800-2023 26.3), and a local parameter computed with $bits
// from one member of that port. The member's width depends on a package
// parameter: [KeyWidth-10:0] = [246:0], 247 bits. The design checks the
// computed width itself: the module OK is instantiated only if
// KeyMgrDigestW is 247. The regression this file exists to catch is HLC
// mis-sizing the member of the package-typed port, which would drop the 'ok'
// instance or mis-size keymgr_digest.
//
// What is checked, and why:
//   Package keymgr_pkg (26.2)
//     - exactly 1 parameter, KeyWidth: a signed int (6.11) with the Constant
//       value "256"; a local parameter, because in a package the keyword
//       'parameter' is a synonym for 'localparam' (6.20.4)
//     - exactly 1 Typedef, 'kmac_data_rsp_t', whose alias is a packed
//       StructTypespec (7.2.1) with exactly 3 members in source order: ready
//       and done (logic with no packed range) and digest_share0, whose
//       single packed range has the left bound Operation vpiSubOp over RefObj
//       'KeyWidth' (bound to the Parameter) and Constant "10", and the right
//       bound Constant "0"
//   Module OK
//     - '()' declares one null port (37.14 detail 10, "module M();"): no
//       name (detail 8), port index 0 (detail 9), and no low connection
//     - its body is empty: no nets, variables, processes or instances
//   Module kmac_keymgr (23.2)
//     - exactly 1 port, keymgr_data_o: output, index 0. Its data type is
//       written with the explicit data_type syntax, so the port is a
//       variable (23.2.2.3): the module's Variable keymgr_data_o is the
//       port's low connection
//     - keymgr_data_o's type is the package-scoped name
//       keymgr_pkg::kmac_data_rsp_t: a RefTypespec path whose prefix is
//       bound to keymgr_pkg and whose last element resolves to the typedef
//     - exactly 1 parameter, KeyMgrDigestW, a local signed int. Unreduced,
//       its value is the SysFuncCall "$bits" (20.6.2) with exactly 1
//       argument, a RefObj path whose prefix is bound to the port variable
//       keymgr_data_o and whose member digest_share0 is bound to that
//       member of kmac_data_rsp_t
//     - exactly 1 generate construct, an if-generate (27.5) whose condition
//       is Operation vpiEqOp over RefObj 'KeyMgrDigestW' (bound to the
//       Parameter) and Constant "247"; its block holds exactly 1 instance,
//       'ok', of the module OK
//     - keymgr_digest is a Variable whose type is an unpacked array (7.4.2)
//       of exactly 2 elements ('[2]' means '[0:1]') whose element type is a
//       LogicTypespec with a single packed range: left bound Operation
//       vpiSubOp over RefObj 'KeyMgrDigestW' and Constant "1", right bound
//       Constant "0"
//   Elaboration
//     - KeyMgrDigestW is a constant expression (6.20.2) reduced to
//       $bits(logic [246:0]) = 247
//     - OK appears in a module instantiation statement, so it is not a
//       top-level module even though that statement is inside a generate
//       block (23.3.1); kmac_keymgr is the only top-level instance
//     - the condition 247 == 247 holds, so the if-generate selects its
//       block. The block is unnamed and the construct is the first in its
//       scope, so its scope is named "genblk1" (27.6); it holds exactly 1
//       module instance, 'ok', of the module OK
//     - keymgr_digest's packed range is reduced to [246:0]
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the unreduced forms are asserted on the
// definitions in Design::getAllModules() and Design::getAllPackages(), which
// hold the source form in every run. The reduced values and the instance
// tree are asserted only when the design is elaborated, on
// Design::getTopModules().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test:
// HLC models '()' with no port at all, although 37.14
// detail 10 names "module M();" as declaring a null port.
// ModuleOKHasOneNullPort is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (unpacked [size] dimension malformed), not a defect in this test:
// HLC records '[2]' as a range whose
// left bound is a vpiSubOp Operation with the single operand 2 and which has
// no right bound. 7.4.2 says "[size] shall mean the same as [0:size-1]", but
// neither those bounds nor the element count can be read from that range.
// KeymgrDigestIsTwoElementUnpackedArray is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Whether the package element of a scoped type path is a RefObj or a
//     RefTypespec wrapping one is a model convention; either is accepted.
//   - The node kind of the generate block in the module definition, and how
//     HLC represents the instance 'ok' there. The instance is found by name
//     in the if-generate's statement; only its name and module are asserted.
//   - How an unpacked '[2]' dimension is recorded (as [0:1] or as a size) is
//     a tool convention; the element count, 2, is asserted either way.
//   - How a path's last element refers to kmac_data_rsp_t: it may resolve to
//     the TypedefTypespec or to the StructTypespec it aliases. Both are that
//     type (6.18), so either is accepted.
//   - HLC represents a package-scoped type name as a RefTypespec path (the
//     package, then the type). That shape is a model convention; the test
//     follows it and asserts both the package and the type binding.
//   - The values the port and keymgr_digest carry only exist while
//     simulation runs. Permanently out of scope.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; keymgr_pkg is looked up
//     by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
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
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace hlc {

class PackDataTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackDataType.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The reference to the package in the first element of a package-scoped
  // type path. HLC records it either as a RefObj or as a RefTypespec whose own
  // path holds that RefObj; which one is a model convention.
  static const hldb::RefObj *getScopePackageRef(const hldb::Any *elem) {
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem)) return ref;
    const hldb::RefTypespec *const rts = any_cast<hldb::RefTypespec>(elem);
    if (rts == nullptr || rts->getPathElems() == nullptr || rts->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(rts->getPathElems()->at(0));
  }

  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("keymgr_pkg", m_design->getAllPackages());
  }

  static const hldb::Parameter *getKeyWidth() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("KeyWidth", pkg->getParameters());
  }

  static const hldb::Typedef *getRspT() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("kmac_data_rsp_t", pkg->getTypedefs());
  }

  static const hldb::Struct *getRspStruct() {
    const hldb::Typedef *const td = getRspT();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr) return nullptr;
    return st->getStruct();
  }

  static const hldb::TypespecMember *getDigestShare0() {
    const hldb::Struct *const s = getRspStruct();
    if (s == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>("digest_share0", s->getMembers());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Module *const m = getModule("kmac_keymgr");
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, m->getVariables());
  }

  static const hldb::Parameter *getDigestW() {
    const hldb::Module *const m = getModule("kmac_keymgr");
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("KeyMgrDigestW", m->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Instance *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::GenIf *getGenIf() {
    const hldb::Module *const m = getModule("kmac_keymgr");
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(stmt)) return genIf;
    }
    return nullptr;
  }

  // The instance 'ok' in the if-generate's block: the block's statement
  // itself, or an item of the Begin HLC may wrap it in.
  static const hldb::RefInstance *getOkInstance() {
    const hldb::GenIf *const genIf = getGenIf();
    if (genIf == nullptr) return nullptr;
    if (const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(genIf->getStmt())) return inst;
    const hldb::Begin *const block = any_cast<hldb::Begin>(genIf->getStmt());
    if (block == nullptr || block->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const item : *block->getStmts()) {
      if (const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(item)) return inst;
    }
    return nullptr;
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if (ch != '_') text.push_back(ch);
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

  // Verifies 'expr' is a Constant whose integer value is 'expected'.
  static void ExpectIntConstant(const hldb::Any *expr, uint64_t expected, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
    ASSERT_NE(c, nullptr) << what << " should be a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << what;
  }

  // Verifies 'range' is [<name>-<offset>:0] with 'name' bound to 'target'.
  static void ExpectNameMinusConstToZero(const hldb::Range *range, std::string_view name, const hldb::Any *target,
                                         std::string_view offset) {
    ASSERT_NE(range, nullptr);
    const hldb::Operation *const left = range->getLeftExpr<hldb::Operation>();
    ASSERT_NE(left, nullptr) << "the left bound '" << name << "-" << offset << "' is an expression";
    EXPECT_EQ(left->getOpType(), vpiSubOp);
    ASSERT_NE(left->getOperands(), nullptr);
    ASSERT_EQ(left->getOperands()->size(), 2u);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(left->getOperands()->at(0));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(ref->getActual(), target);
    const hldb::Constant *const sub = any_cast<hldb::Constant>(left->getOperands()->at(1));
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->getDecompile(), offset);
    const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), "0");
  }

  // Verifies 'm' has one null port (37.14 detail 10).
  static void ExpectOneNullPort(const hldb::Module *m, std::string_view what) {
    ASSERT_NE(m, nullptr) << what << " not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
    const hldb::Port *const port = m->getPorts()->at(0);
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
    EXPECT_EQ(port->getLowConn(), nullptr) << "37.14 detail 10: a null port has no low connection";
  }
};

// ---------------------------------------------------------------------------
// package keymgr_pkg; ... endpackage
// ---------------------------------------------------------------------------

TEST_F(PackDataTypeTest, KeyWidthIsSignedInt256) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'keymgr_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u) << "'KeyWidth' is keymgr_pkg's only parameter";
  const hldb::Parameter *const kw = getKeyWidth();
  ASSERT_NE(kw, nullptr) << "parameter 'KeyWidth' not found";
  ASSERT_NE(kw->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = kw->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'KeyWidth' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "KeyWidth");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const value = pa->getRhs<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "256");
}

TEST_F(PackDataTypeTest, KeyWidthIsLocalParam) {
  const hldb::Parameter *const kw = getKeyWidth();
  ASSERT_NE(kw, nullptr) << "parameter 'KeyWidth' not found";
  EXPECT_TRUE(kw->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
}

TEST_F(PackDataTypeTest, RspTIsPackedStructOfThreeMembers) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'kmac_data_rsp_t' is keymgr_pkg's only typedef";
  const hldb::Struct *const s = getRspStruct();
  ASSERT_NE(s, nullptr) << "6.18: typedef 'kmac_data_rsp_t' should alias a structure type";
  EXPECT_TRUE(s->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 3u);
  const char *const names[] = {"ready", "done", "digest_share0"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::TypespecMember *const m = s->getMembers()->at(i);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), names[i]) << "member " << i << ", in source order";
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "'" << names[i] << "' is declared 'logic'";
    if (i < 2) {
      EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << "'" << names[i] << "' is a single bit";
    }
  }
}

TEST_F(PackDataTypeTest, DigestShare0RangeIsKeyWidthMinus10To0) {
  const hldb::TypespecMember *const member = getDigestShare0();
  ASSERT_NE(member, nullptr) << "member 'digest_share0' not found";
  ASSERT_NE(member->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = member->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectNameMinusConstToZero(lt->getRanges()->at(0), "KeyWidth", getKeyWidth(), "10");
}

// ---------------------------------------------------------------------------
// module OK(); endmodule
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG
// (null port missing) in the file header.
TEST_F(PackDataTypeTest, ModuleOKHasOneNullPort) { ExpectOneNullPort(getModule("OK"), "module 'OK'"); }

TEST_F(PackDataTypeTest, ModuleOKBodyIsEmpty) {
  const hldb::Module *const ok = getModule("OK");
  ASSERT_NE(ok, nullptr);
  EXPECT_TRUE(ok->getNets() == nullptr || ok->getNets()->empty());
  EXPECT_TRUE(ok->getVariables() == nullptr || ok->getVariables()->empty());
  EXPECT_TRUE(ok->getProcesses() == nullptr || ok->getProcesses()->empty());
  EXPECT_TRUE(ok->getRefInstances() == nullptr || ok->getRefInstances()->empty());
}

// ---------------------------------------------------------------------------
// module kmac_keymgr(output keymgr_pkg::kmac_data_rsp_t keymgr_data_o);
// ---------------------------------------------------------------------------

TEST_F(PackDataTypeTest, KmacKeymgrHasOneOutputVariablePort) {
  const hldb::Module *const m = getModule("kmac_keymgr");
  ASSERT_NE(m, nullptr) << "module 'kmac_keymgr' not found";
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  const hldb::Port *const port = m->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "keymgr_data_o");
  EXPECT_EQ(port->getDirection(), vpiOutput);
  EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9";
  const hldb::Variable *const v = getVar("keymgr_data_o");
  ASSERT_NE(v, nullptr) << "23.2.2.3: an output port whose data type uses the explicit data_type syntax is a variable";
  EXPECT_TRUE(m->getNets() == nullptr || m->getNets()->empty()) << "the module declares no net";
  const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), v) << "37.14 detail 4: the port connects to its own variable";
}

TEST_F(PackDataTypeTest, PortIsTypedByKeymgrPkgRspT) {
  const hldb::Variable *const v = getVar("keymgr_data_o");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const type = v->getTypespec();
  ASSERT_NE(type, nullptr);
  ASSERT_NE(type->getPathElems(), nullptr) << "'keymgr_pkg::kmac_data_rsp_t' should be a package-scoped path";
  ASSERT_EQ(type->getPathElems()->size(), 2u) << "the package, then the type";
  const hldb::RefObj *const pkgRef = getScopePackageRef(type->getPathElems()->at(0));
  ASSERT_NE(pkgRef, nullptr) << "the path starts with the package";
  EXPECT_EQ(pkgRef->getName(), "keymgr_pkg");
  EXPECT_EQ(pkgRef->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names keymgr_pkg";
  const hldb::RefTypespec *const last = any_cast<hldb::RefTypespec>(type->getPathElems()->at(1));
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(last->getName(), "kmac_data_rsp_t");
  const hldb::Typedef *const td = getRspT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::Typespec *const actual = last->getActual();
  ASSERT_NE(actual, nullptr) << "26.3: 'kmac_data_rsp_t' is declared in keymgr_pkg, so the scoped type must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  const bool isRspT =
      ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
  EXPECT_TRUE(isRspT) << "26.3: the port is typed by keymgr_pkg's kmac_data_rsp_t";
}

// ---------------------------------------------------------------------------
// localparam int KeyMgrDigestW = $bits(keymgr_data_o.digest_share0);
// ---------------------------------------------------------------------------

TEST_F(PackDataTypeTest, KeyMgrDigestWIsLocalSignedInt) {
  const hldb::Module *const m = getModule("kmac_keymgr");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "'KeyMgrDigestW' is the module's only parameter";
  const hldb::Parameter *const w = getDigestW();
  ASSERT_NE(w, nullptr) << "parameter 'KeyMgrDigestW' not found";
  EXPECT_TRUE(w->getLocalParam()) << "6.20.4: declared 'localparam'";
  ASSERT_NE(w->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = w->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'KeyMgrDigestW' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
}

TEST_F(PackDataTypeTest, KeyMgrDigestWIsBitsOfDigestShare0) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("kmac_keymgr"), "KeyMgrDigestW");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'KeyMgrDigestW'";
  const hldb::SysFuncCall *const bits = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr) << "unreduced, '$bits(...)' is a system function call";
  EXPECT_EQ(bits->getName(), "$bits");
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  const hldb::RefObj *const path = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'keymgr_data_o.digest_share0' should be a RefObj path";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the port variable, then its member";
  const hldb::RefObj *const port = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "keymgr_data_o");
  ASSERT_NE(getVar("keymgr_data_o"), nullptr);
  EXPECT_EQ(port->getActual(), getVar("keymgr_data_o"));
  const hldb::RefObj *const member = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getName(), "digest_share0");
  ASSERT_NE(getDigestShare0(), nullptr);
  EXPECT_EQ(member->getActual(), getDigestShare0()) << "the member of kmac_data_rsp_t";
}

// ---------------------------------------------------------------------------
// if (KeyMgrDigestW == 247) OK ok();
// ---------------------------------------------------------------------------

TEST_F(PackDataTypeTest, IfGenerateComparesKeyMgrDigestWWith247) {
  const hldb::Module *const m = getModule("kmac_keymgr");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr);
  EXPECT_EQ(m->getGenStmts()->size(), 1u) << "the module body holds one generate construct";
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr) << "27.5: 'if (KeyMgrDigestW == 247) OK ok();' is an if-generate";
  const hldb::Operation *const eq = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  const hldb::RefObj *const w = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getName(), "KeyMgrDigestW");
  ASSERT_NE(getDigestW(), nullptr);
  EXPECT_EQ(w->getActual(), getDigestW());
  const hldb::Constant *const c = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "247");
}

TEST_F(PackDataTypeTest, GenerateBlockInstantiatesOK) {
  const hldb::RefInstance *const ok = getOkInstance();
  ASSERT_NE(ok, nullptr) << "the if-generate's block holds the instance 'ok'";
  EXPECT_EQ(ok->getName(), "ok");
  ASSERT_NE(ok->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = ok->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "23.3.2: 'OK ok();' instantiates a module";
  ASSERT_NE(getModule("OK"), nullptr);
  EXPECT_EQ(mt->getModule(), getModule("OK")) << "'ok' is an instance of the module OK";
}

// ---------------------------------------------------------------------------
// logic [KeyMgrDigestW-1:0] keymgr_digest [2];
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG
// (unpacked [size] dimension malformed) in the file header.
TEST_F(PackDataTypeTest, KeymgrDigestIsTwoElementUnpackedArray) {
  const hldb::Variable *const v = getVar("keymgr_digest");
  ASSERT_NE(v, nullptr) << "variable 'keymgr_digest' not found";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'[2]' after the name declares an unpacked array";
  EXPECT_FALSE(at->getPacked()) << "7.4.2: a dimension after the name is unpacked";
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  int64_t count = r->getSize();
  if ((left != nullptr) && (right != nullptr)) {
    uint64_t l = 0;
    uint64_t rr = 0;
    ASSERT_TRUE(getIntValue(left, &l) && getIntValue(right, &rr));
    count = std::llabs(static_cast<int64_t>(l) - static_cast<int64_t>(rr)) + 1;
  }
  EXPECT_EQ(count, 2) << "7.4.2: '[2]' means '[0:1]', two elements";
}

TEST_F(PackDataTypeTest, KeymgrDigestElementIsLogicKeyMgrDigestWMinus1To0) {
  const hldb::Variable *const v = getVar("keymgr_digest");
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = at->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "the element type is 'logic [KeyMgrDigestW-1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectNameMinusConstToZero(lt->getRanges()->at(0), "KeyMgrDigestW", getDigestW(), "1");
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(PackDataTypeTest, KeyMgrDigestWReducesTo247) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("kmac_keymgr", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "kmac_keymgr is a top-level instance";
    const hldb::ParamAssign *const pa = getParamAssign(top, "KeyMgrDigestW");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'KeyMgrDigestW' in the elaborated kmac_keymgr";
    ExpectIntConstant(pa->getRhs(), 247, "20.6.2: $bits(logic [256-10:0]) = 247");
  }
}

TEST_F(PackDataTypeTest, KmacKeymgrIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u)
        << "23.3.1: OK appears in an instantiation (inside a generate block), so it is not top-level";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "kmac_keymgr");
  }
}

TEST_F(PackDataTypeTest, IfGenerateInstantiatesOkInGenblk1) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("kmac_keymgr", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::GenScopeArray *const gen = hldb::findByName<hldb::GenScopeArray>("genblk1", top->getGenScopeArrays());
    ASSERT_NE(gen, nullptr) << "27.5, 27.6: 247 == 247, so the unnamed block of the first generate construct, "
                               "genblk1, is instantiated";
    ASSERT_NE(gen->getGenScopes(), nullptr);
    ASSERT_EQ(gen->getGenScopes()->size(), 1u);
    const hldb::GenScope *const scope = gen->getGenScopes()->at(0);
    ASSERT_NE(scope, nullptr);
    ASSERT_NE(scope->getModules(), nullptr);
    ASSERT_EQ(scope->getModules()->size(), 1u) << "the block holds the one instance 'ok'";
    const hldb::Module *const ok = scope->getModules()->at(0);
    ASSERT_NE(ok, nullptr);
    EXPECT_EQ(ok->getName(), "ok");
    EXPECT_EQ(ok->getDefName(), "OK");
  }
}

TEST_F(PackDataTypeTest, KeymgrDigestElementReducesTo246To0) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("kmac_keymgr", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>("keymgr_digest", top->getVariables());
    ASSERT_NE(v, nullptr) << "'keymgr_digest' not found in the elaborated kmac_keymgr";
    ASSERT_NE(v->getTypespec(), nullptr);
    const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(at, nullptr);
    ASSERT_NE(at->getElemTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = at->getElemTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    ExpectIntConstant(lt->getRanges()->at(0)->getLeftExpr(), 246, "KeyMgrDigestW-1 = 247-1 = 246");
    ExpectIntConstant(lt->getRanges()->at(0)->getRightExpr(), 0, "the right bound is 0");
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackDataTypeTest, NoFatalSyntaxOrErrorDiagnostics) {
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
