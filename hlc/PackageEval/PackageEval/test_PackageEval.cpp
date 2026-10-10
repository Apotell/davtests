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

// Validates the HLDB model built for tests/PackageEval/dut.sv:
//
//   package test_pkg;
//    parameter int NStraps  = 2;
//    parameter int MioStrapPos [0:NStraps-1] = '{1, 0};
//   typedef struct packed {
//       logic valid;
//       logic [63:0] data;
//   } t_test;
//   parameter TEST_SIZE = $bits(t_test);
//   endpackage
//
// The point of the fixture is evaluation of constant expressions inside a
// package: an array bound computed from another parameter, and a parameter
// computed by $bits of a packed struct type.
//
// What is checked, and why:
//   Package test_pkg exists; no ': label' after endpackage.
//   Parameters (6.20)
//     - exactly 3 parameters, NStraps, MioStrapPos and TEST_SIZE, and exactly
//       3 ParamAssigns
//     - 6.20.4: inside a package 'parameter' is a synonym for 'localparam',
//       so all 3 have getLocalParam() true
//   parameter int NStraps = 2;
//     - signed IntTypespec (6.11); ParamAssign RHS Constant "2"
//   parameter int MioStrapPos [0:NStraps-1] = '{1, 0};
//     - ArrayTypespec, vpiStaticArray (a fixed-size unpacked dimension,
//       7.4.2), element typespec signed IntTypespec
//     - unpacked range left bound Constant "0"
//     - right bound: on an elaborated design it is reduced to 1
//       (NStraps - 1 = 2 - 1); otherwise it is the unreduced Operation
//       vpiSubOp over RefObj 'NStraps' (bound to that Parameter) and
//       Constant "1"
//     - ParamAssign RHS is an assignment pattern (10.9.1): Operation
//       vpiAssignmentPatternOp with exactly 2 operands, Constants "1", "0"
//   typedef struct packed { ... } t_test; (7.2.1)
//     - the package owns exactly 1 TypedefTypespec 't_test', aliasing a
//       StructTypespec whose Struct declaration has getPacked() true
//     - exactly 2 members in declaration order: 'valid' (scalar logic, no
//       packed range) and 'data' (logic with exactly 1 packed range [63:0])
//   parameter TEST_SIZE = $bits(t_test);
//     - unreduced, the ParamAssign RHS is a SysFuncCall "$bits" with exactly
//       1 argument naming 't_test'
//     - on an elaborated design it is reduced to 65: $bits of a packed
//       struct is the sum of its members' widths, 1 + 64 (20.6.2, 7.2.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics, and no
//       COMP_FAILED_TO_BIND for 'NStraps' or 't_test'
//
// What is NOT checked, and why:
//   - The type HLC gives the implicitly typed TEST_SIZE. 6.20.2 says it
//     takes the type of its final value, and which typespec node HLC
//     attaches to record that is a tool convention; the value itself is
//     asserted in TestSizeIsBitsOfTTest.
//   - The node kind HLC uses for the type argument of $bits (a typespec
//     reference or an object reference) is a tool convention; only its name
//     is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {

class PackageEvalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageEval.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("test_pkg", m_design->getAllPackages());
  }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::ParamAssign>(name, pkg->getParamAssigns());
  }

  static const hldb::ArrayTypespec *getMioStrapPosType() {
    const hldb::Parameter *const p = getParam("MioStrapPos");
    if (p == nullptr || p->getTypespec() == nullptr) return nullptr;
    return p->getTypespec()->getActual<hldb::ArrayTypespec>();
  }

  static const hldb::TypedefTypespec *getTTest() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::TypedefTypespec>("t_test", pkg->getTypespecs());
  }

  static const hldb::StructTypespec *getStruct() {
    const hldb::TypedefTypespec *const td = getTTest();
    if (td == nullptr || td->getTypedef() == nullptr || td->getTypedef()->getAlias() == nullptr) return nullptr;
    return td->getTypedef()->getAlias()->getActual<hldb::StructTypespec>();
  }

  // The struct declaration the typespec refers to. It owns the packed flag
  // and the members.
  static const hldb::Struct *getStructDecl() {
    const hldb::StructTypespec *const st = getStruct();
    if (st == nullptr) return nullptr;
    return st->getStruct();
  }

  // Integer value of a Constant, from its decompiled text ("65") or from a
  // sized/based literal ("32'd65", "32'h41"). The radix HLC picks for a
  // folded value is a tool convention, so every IEEE 1800 integer form is
  // accepted and the numeric value is what gets compared.
  static bool parseConstantValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if (ch != '_') text.push_back(ch);
    }
    if (text.empty()) text = std::string(c->getValue());
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
};

// ---------------------------------------------------------------------------
// package test_pkg
// ---------------------------------------------------------------------------

TEST_F(PackageEvalTest, PackageExistsWithoutEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'test_pkg' not found in Design::getAllPackages()";
  EXPECT_EQ(pkg->getName(), "test_pkg");
  EXPECT_EQ(pkg->getEndLabel(), "");
}

TEST_F(PackageEvalTest, PackageHasThreeParametersAndThreeParamAssigns) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 3u);
  EXPECT_NE(getParam("NStraps"), nullptr);
  EXPECT_NE(getParam("MioStrapPos"), nullptr);
  EXPECT_NE(getParam("TEST_SIZE"), nullptr);
  ASSERT_NE(pkg->getParamAssigns(), nullptr);
  EXPECT_EQ(pkg->getParamAssigns()->size(), 3u);
}

TEST_F(PackageEvalTest, AllParametersAreLocalParams) {
  for (std::string_view name : {"NStraps", "MioStrapPos", "TEST_SIZE"}) {
    const hldb::Parameter *const p = getParam(name);
    ASSERT_NE(p, nullptr) << name;
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << name << "' is declared in a package, where 'parameter' "
                                    << "is a synonym for 'localparam'";
  }
}

// ---------------------------------------------------------------------------
// parameter int NStraps = 2;
// ---------------------------------------------------------------------------

TEST_F(PackageEvalTest, NStrapsIsSignedIntTwo) {
  const hldb::Parameter *const p = getParam("NStraps");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = p->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "declared 'int'";
  EXPECT_TRUE(ts->getSigned());

  const hldb::ParamAssign *const pa = getParamAssign("NStraps");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "2");
}

// ---------------------------------------------------------------------------
// parameter int MioStrapPos [0:NStraps-1] = '{1, 0};
// ---------------------------------------------------------------------------

TEST_F(PackageEvalTest, MioStrapPosIsStaticArrayOfSignedInt) {
  const hldb::ArrayTypespec *const at = getMioStrapPosType();
  ASSERT_NE(at, nullptr) << "'int MioStrapPos [0:NStraps-1]' should resolve to an ArrayTypespec";
  EXPECT_EQ(at->getArrayType(), vpiStaticArray) << "7.4.2: a fixed-size unpacked dimension";
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "the element type is 'int'";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(PackageEvalTest, MioStrapPosRangeLeftIsZero) {
  const hldb::ArrayTypespec *const at = getMioStrapPosType();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "0");
}

TEST_F(PackageEvalTest, MioStrapPosRangeRightIsNStrapsMinusOne) {
  const hldb::ArrayTypespec *const at = getMioStrapPosType();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getRightExpr(), nullptr);
  if (m_design->getElaborated()) {
    // Array dimension bounds are constant expressions fixed at elaboration.
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr) << "on an elaborated design 'NStraps-1' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(parseConstantValue(right, &v)) << "unparsable constant '" << right->getDecompile() << "'";
    EXPECT_EQ(v, 1u) << "NStraps - 1 = 2 - 1";
  } else {
    const hldb::Operation *const op = r->getRightExpr<hldb::Operation>();
    ASSERT_NE(op, nullptr) << "unreduced, 'NStraps-1' is an Operation";
    EXPECT_EQ(op->getOpType(), vpiSubOp);
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(op->getOperands()->at(0));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getName(), "NStraps");
    ASSERT_NE(getParam("NStraps"), nullptr);
    EXPECT_EQ(ref->getActual<hldb::Parameter>(), getParam("NStraps"));
    const hldb::Constant *const one = any_cast<hldb::Constant>(op->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), "1");
  }
}

TEST_F(PackageEvalTest, MioStrapPosValueIsAssignmentPatternOneZero) {
  const hldb::ParamAssign *const pa = getParamAssign("MioStrapPos");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'{1, 0} should be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp) << "10.9.1: '{...} is an assignment pattern";
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::Constant *const e0 = any_cast<hldb::Constant>(rhs->getOperands()->at(0));
  const hldb::Constant *const e1 = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  ASSERT_NE(e0, nullptr);
  ASSERT_NE(e1, nullptr);
  EXPECT_EQ(e0->getDecompile(), "1");
  EXPECT_EQ(e1->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// typedef struct packed { logic valid; logic [63:0] data; } t_test;
// ---------------------------------------------------------------------------

TEST_F(PackageEvalTest, PackageOwnsOneTypedefTTest) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypespecs(), nullptr);
  size_t typedefCount = 0;
  for (const hldb::Typespec *const ts : *pkg->getTypespecs()) {
    if (any_cast<hldb::TypedefTypespec>(ts) != nullptr) ++typedefCount;
  }
  EXPECT_EQ(typedefCount, 1u) << "test_pkg declares exactly one typedef";
  const hldb::TypedefTypespec *const td = getTTest();
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "t_test");
}

TEST_F(PackageEvalTest, TTestIsPackedStructWithTwoMembers) {
  ASSERT_NE(getStruct(), nullptr) << "typedef 't_test' should alias a StructTypespec";
  const hldb::Struct *const decl = getStructDecl();
  ASSERT_NE(decl, nullptr);
  EXPECT_TRUE(decl->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(decl->getMembers(), nullptr);
  ASSERT_EQ(decl->getMembers()->size(), 2u);
  EXPECT_EQ(decl->getMembers()->at(0)->getName(), "valid");
  EXPECT_EQ(decl->getMembers()->at(1)->getName(), "data");
}

TEST_F(PackageEvalTest, MemberValidIsScalarLogic) {
  const hldb::Struct *const decl = getStructDecl();
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getMembers(), nullptr);
  ASSERT_EQ(decl->getMembers()->size(), 2u);
  const hldb::TypespecMember *const valid = decl->getMembers()->at(0);
  ASSERT_NE(valid->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = valid->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'valid' is declared 'logic'";
  EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << "'logic valid' has no packed dimension";
}

TEST_F(PackageEvalTest, MemberDataIsLogic63To0) {
  const hldb::Struct *const decl = getStructDecl();
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getMembers(), nullptr);
  ASSERT_EQ(decl->getMembers()->size(), 2u);
  const hldb::TypespecMember *const data = decl->getMembers()->at(1);
  ASSERT_NE(data->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = data->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'data' is declared 'logic [63:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "63");
  EXPECT_EQ(right->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// parameter TEST_SIZE = $bits(t_test);
// ---------------------------------------------------------------------------

TEST_F(PackageEvalTest, TestSizeIsBitsOfTTest) {
  const hldb::ParamAssign *const pa = getParamAssign("TEST_SIZE");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs(), nullptr);
  if (m_design->getElaborated()) {
    // 20.6.2: $bits of a type is an elaboration-time constant.
    const hldb::Constant *const value = pa->getRhs<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "on an elaborated design '$bits(t_test)' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(parseConstantValue(value, &v)) << "unparsable constant '" << value->getDecompile() << "'";
    EXPECT_EQ(v, 65u) << "7.2.1: a packed struct is as wide as its members together, 1 + 64";
  } else {
    const hldb::SysFuncCall *const call = pa->getRhs<hldb::SysFuncCall>();
    ASSERT_NE(call, nullptr) << "unreduced, '$bits(t_test)' is a SysFuncCall";
    EXPECT_EQ(call->getName(), "$bits");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::NamedArgument *const arg = any_cast<hldb::NamedArgument>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    const hldb::Any *const hc0 = arg->getHighConn();
    ASSERT_NE(hc0, nullptr);
    EXPECT_EQ(hc0->getName(), "t_test") << "the single argument of $bits is the type 't_test'";
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageEvalTest, CompilerReportsZeroErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "NStraps"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "t_test"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
