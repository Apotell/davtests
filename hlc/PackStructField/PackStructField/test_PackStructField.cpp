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

// Validates the HLDB model built for tests/PackStructField/dut.sv:
//
//   package my_package;
//     typedef struct packed {
//       logic a;
//       logic b;
//     } my_struct;
//     parameter my_package::my_struct my_parameter = '{
//       a: 1'b1,
//       b: 1'b0
//     };
//   endpackage : my_package
//
//   module my_module (
//       output my_package::my_struct x,
//       output my_package::my_struct y
//   );
//   assign x = my_package::my_parameter.a;
//   assign y = my_package::my_parameter.b;
//   endmodule : my_module
//
// The point of the fixture is a member select applied to a package parameter
// of structure type, named with the package scope resolution operator
// (IEEE 1800-2023 26.3): 'my_package::my_parameter.a'. The regression this
// file exists to catch is HLC failing to bind the parameter or the member
// through the scoped name.
//
// What is checked, and why:
//   Package my_package (26.2)
//     - end label "my_package"
//     - exactly 1 Typedef, 'my_struct', whose alias is a packed StructTypespec
//       (7.2.1) with exactly 2 members, a then b, each a 1-bit logic
//     - exactly 1 parameter, my_parameter: a local parameter (6.20.4: in a
//       package 'parameter' is a synonym for 'localparam') whose type is the
//       package-scoped name my_package::my_struct, resolving to the
//       package's own my_struct
//     - unreduced, its value is Operation vpiAssignmentPatternOp (10.9.2)
//       with exactly 2 member-keyed items, in source order a: 1'b1 and
//       b: 1'b0
//     - a parameter value is a constant expression (6.20.2). The first member
//       of a packed structure is its most significant (7.2.1), so on an
//       elaborated design my_parameter is reduced to 2'b10, the value 2
//   Module my_module (23.2)
//     - end label "my_module"; exactly 2 ports, x then y, both outputs. Each
//       writes its data type with the explicit data_type syntax, so each is a
//       variable (23.2.2.3) typed by my_package::my_struct, and is its port's
//       low connection
//     - port indexes 0 and 1 (37.14 detail 9)
//     - exactly 2 ContAssigns, driving x and y. Each RHS is a RefObj path
//       of 3 elements: the prefix bound to my_package, then 'my_parameter'
//       bound to the package's Parameter, then the member (a, then b) bound
//       to that member of my_struct
//   Elaboration (23.3.1)
//     - my_module appears in no instantiation, so on an elaborated design it
//       is the only top-level instance
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: my_parameter is checked in both modes, unreduced
// on the package definition in Design::getAllPackages() and reduced, only
// when the design is elaborated, on Design::getTopPackages().
//
// KNOWN COMPILER BUG (port index not set), not a defect in this test: HLC
// leaves vpiPortIndex at 0 for every port, although 37.14 detail 9 says the
// port index gives the port order. PortIndexesFollowDeclarationOrder is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - The values x and y carry only exist while simulation runs.
//   - How HLC labels a member key of an assignment pattern item (the item's
//     name or its typespec's name) is a model convention; either is
//     accepted.
//   - Whether the package element of a scoped type path is a RefObj or a
//     RefTypespec wrapping one is a model convention; either is accepted.
//   - How a RefTypespec refers to my_struct: it may resolve to the
//     TypedefTypespec or to the StructTypespec it aliases. Both are that type
//     (6.18), so either is accepted.
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
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
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

namespace hlc {

class PackStructFieldTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackStructField.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("my_package", m_design->getAllPackages());
  }

  static const hldb::Typedef *getMyStruct() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("my_struct", pkg->getTypedefs());
  }

  static const hldb::TypespecMember *getMember(std::string_view name) {
    const hldb::Typedef *const td = getMyStruct();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr || st->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>(name, st->getStruct()->getMembers());
  }

  static const hldb::Parameter *getMyParameter() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("my_parameter", pkg->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(const hldb::Package *pkg) {
    if (pkg == nullptr || pkg->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *pkg->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == "my_parameter")) return pa;
    }
    return nullptr;
  }

  static const hldb::Module *getModule() {
    return hldb::findByDefName<hldb::Module>("my_module", m_design->getAllModules());
  }

  // Whether 'rts' names my_struct, directly or as the last element of a
  // package-scoped path.
  static bool namesMyStruct(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getMyStruct();
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

  // Verifies 'expr' is 'my_package::my_parameter.<member>'.
  static void ExpectScopedMemberSelect(const hldb::Any *expr, std::string_view member) {
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(path, nullptr) << "'my_package::my_parameter." << member << "' should be a RefObj path";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 3u) << "the package, the parameter, then the member";
    const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(scope, nullptr);
    EXPECT_EQ(scope->getName(), "my_package");
    EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names my_package";
    const hldb::RefObj *const param = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(param, nullptr);
    EXPECT_EQ(param->getName(), "my_parameter");
    ASSERT_NE(getMyParameter(), nullptr);
    EXPECT_EQ(param->getActual(), getMyParameter()) << "26.3: 'my_parameter' is declared in my_package";
    const hldb::RefObj *const m = any_cast<hldb::RefObj>(path->getPathElems()->at(2));
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), member);
    ASSERT_NE(getMember(member), nullptr);
    EXPECT_EQ(m->getActual(), getMember(member)) << "7.2: '" << member << "' is a member of my_struct";
  }
};

// ---------------------------------------------------------------------------
// package my_package; ... endpackage : my_package
// ---------------------------------------------------------------------------

TEST_F(PackStructFieldTest, MyStructIsPackedStructOfTwoBits) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'my_package' not found";
  EXPECT_EQ(pkg->getEndLabel(), "my_package");
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u);
  const hldb::Typedef *const td = getMyStruct();
  ASSERT_NE(td, nullptr) << "typedef 'my_struct' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'my_struct' names a structure type";
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
    EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty());
  }
}

TEST_F(PackStructFieldTest, MyParameterIsLocalOfScopedMyStruct) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getMyParameter();
  ASSERT_NE(p, nullptr) << "parameter 'my_parameter' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  EXPECT_TRUE(namesMyStruct(p->getTypespec())) << "26.3: 'my_package::my_struct' names the package's own my_struct";
}

TEST_F(PackStructFieldTest, MyParameterIsMemberKeyedPattern) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg());
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'my_parameter'";
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a structure assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u);
  const char *const keys[] = {"a", "b"};
  const char *const values[] = {"1'b1", "1'b0"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i << " is a 'member: value' item";
    const hldb::RefObj *const tag = item->getTag<hldb::RefObj>();
    ASSERT_NE(tag, nullptr) << "10.9.2: item " << i << " is keyed by a member name";
    EXPECT_EQ(tag->getName(), keys[i]);
    ASSERT_NE(getMember(keys[i]), nullptr);
    EXPECT_EQ(tag->getActual(), getMember(keys[i]))
        << "10.9.2: item " << i << " is keyed by the member " << keys[i] << " of my_struct";
    const hldb::Constant *const value = item->getPattern<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "item " << i;
    EXPECT_EQ(value->getDecompile(), values[i]) << "item " << i;
  }
}

TEST_F(PackStructFieldTest, MyParameterReducesToTwoBitsTen) {
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const pa =
        getParamAssign(hldb::findByName<hldb::Package>("my_package", m_design->getTopPackages()));
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'my_parameter' in the elaborated package";
    const hldb::Constant *const value = pa->getRhs<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "6.20.2: a parameter value is reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(value, &v)) << "unparsable constant '" << value->getDecompile() << "'";
    EXPECT_EQ(v, 2u) << "7.2.1: a (1) is the most significant bit, b (0) the least: 2'b10";
  }
}

// ---------------------------------------------------------------------------
// module my_module (output my_package::my_struct x, output ... y);
// ---------------------------------------------------------------------------

TEST_F(PackStructFieldTest, ModuleHasTwoOutputVariablePortsOfMyStruct) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr) << "module 'my_module' not found";
  EXPECT_EQ(m->getEndLabel(), "my_module");
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 2u);
  const char *const names[] = {"x", "y"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Port *const port = m->getPorts()->at(i);
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getName(), names[i]) << "port " << i << ", in source order";
    EXPECT_EQ(port->getDirection(), vpiOutput);
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(names[i], m->getVariables());
    ASSERT_NE(v, nullptr) << "23.2.2.3: an output port with an explicit data type is a variable";
    EXPECT_TRUE(namesMyStruct(v->getTypespec())) << "'" << names[i] << "' is declared my_package::my_struct";
    ASSERT_NE(port->getLowConn<hldb::RefObj>(), nullptr);
    EXPECT_EQ(port->getLowConn<hldb::RefObj>()->getActual(), v);
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (port index
// not set) in the file header.
TEST_F(PackStructFieldTest, PortIndexesFollowDeclarationOrder) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 2u);
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_NE(m->getPorts()->at(i), nullptr);
    EXPECT_EQ(m->getPorts()->at(i)->getPortIndex(), static_cast<int32_t>(i))
        << "37.14 detail 9: vpiPortIndex gives the port order, and the first port has index 0";
  }
}

TEST_F(PackStructFieldTest, ContAssignsSelectMembersOfScopedParameter) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_EQ(m->getContAssigns()->size(), 2u);
  const char *const targets[] = {"x", "y"};
  const char *const members[] = {"a", "b"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::ContAssign *const ca = m->getContAssigns()->at(i);
    ASSERT_NE(ca, nullptr);
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), targets[i]);
    EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>(targets[i], m->getVariables()));
    ExpectScopedMemberSelect(ca->getRhs(), members[i]);
  }
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackStructFieldTest, MyModuleIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'my_module' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "my_module");
  }
}

TEST_F(PackStructFieldTest, NoFatalSyntaxOrErrorDiagnostics) {
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
