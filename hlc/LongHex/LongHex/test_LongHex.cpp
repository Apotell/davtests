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

// Tests for tests/LongHex/dut.sv (tags: LongHex)
//   package aes_pkg;
//      parameter logic [159:0] RndCnstMaskingLfsrSeedDefault =
//         160'hc132b5723c5a4cf4743b3c7c32d580f74f1713a;
//   endpackage
//   module aes_cipher_core import aes_pkg::*;();
//      parameter logic [159:0] RndCnstMaskingLfsrSeed = 0;
//      assign x = RndCnstMaskingLfsrSeed[31:0];
//   endmodule
//   module aes_core import aes_pkg::*;();
//      aes_cipher_core #(
//         .RndCnstMaskingLfsrSeed(RndCnstMaskingLfsrSeedDefault)
//      ) u_aes_cipher_core ();
//   endmodule
//
// The construct under test is a sized hexadecimal literal much wider than
// 64 bits (160'h... with 39 hex digits) used as a parameter value, and that
// value flowing through a package import into a module parameter override.
//
// What is checked (IEEE 1800-2023):
//   - 5.7.1 "Integer literal constants": "160'h..." is a sized based
//     literal; the size is 160 bits and the base is hexadecimal. "If the
//     size of the unsigned number is smaller than the size specified for
//     the literal constant, the unsigned number shall be padded to the left
//     with zeros", so the value must keep all 39 significant hex digits --
//     no truncation to 64 bits.
//   - 6.20.4: "parameter" in a package is a synonym for "localparam"; the
//     module-body parameter of aes_cipher_core (no parameter_port_list) is
//     a nonlocal, overridable parameter.
//   - 6.20.2: a parameter with a type and range specification
//     ("logic [159:0]") has the declared range [159:0].
//   - 26.2/26.3: aes_pkg declares RndCnstMaskingLfsrSeedDefault; 26.4 the
//     "import aes_pkg::*;" in the module header makes it visible in
//     aes_core, so the by-name override RHS binds to the package parameter.
//   - 23.10.2.2 parameter value assignment by name: u_aes_cipher_core
//     overrides aes_cipher_core.RndCnstMaskingLfsrSeed.
//   - 6.11.1 / 7.4.3: "RndCnstMaskingLfsrSeed[31:0]" is a part-select of an
//     integral parameter (allowed by 6.20.2 "Bit-selects and part-selects of
//     parameters that are of integral types shall be allowed").
//   - 6.10 "Implicit declarations": "x" is never declared and appears on the
//     LHS of a continuous assignment, so "an implicit scalar net of default
//     net type shall be assumed" (default nettype is wire, 22.8).
//
// What is NOT checked and why:
//   - the elaborated value of u_aes_cipher_core.RndCnstMaskingLfsrSeed: the
//     .hlc requests no elaboration and the HLDB design here is the
//     non-elaborated (folded) model, so per-instance parameter values do not
//     exist to query.
//   - the exact textual encoding of Constant::getValue() (any "HEX:"-style
//     prefix, letter case, leading zero padding) is an HLDB representation
//     choice, not standard-mandated; only the significant hex digits are
//     compared.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace hlc {

class LongHexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LongHex.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("aes_pkg", m_design->getAllPackages());
  }
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }
  static const hldb::Parameter *getPkgParam() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("RndCnstMaskingLfsrSeedDefault", pkg->getParameters());
  }
  static const hldb::Parameter *getCoreParam() {
    const hldb::Module *const core = getModule("aes_cipher_core");
    if (core == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("RndCnstMaskingLfsrSeed", core->getParameters());
  }

  // Lowercase, strip any "XXX:" prefix and leading zeros.
  static std::string normalizeHex(std::string_view value) {
    std::string s(value);
    const std::string::size_type colon = s.find(':');
    if (colon != std::string::npos) s = s.substr(colon + 1);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::string::size_type nz = s.find_first_not_of('0');
    return (nz == std::string::npos) ? std::string("0") : s.substr(nz);
  }

  // Checks that 'param' has a typespec logic [159:0].
  static void expectLogic159To0(const hldb::Parameter *param) {
    ASSERT_NE(param, nullptr);
    const hldb::RefTypespec *const rt = param->getTypespec();
    ASSERT_NE(rt, nullptr) << "parameter declared with 'logic [159:0]' must carry a typespec";
    const hldb::LogicTypespec *const lt = rt->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "typespec must be a LogicTypespec";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Range *const r = lt->getRanges()->at(0);
    ASSERT_NE(r, nullptr);
    const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->getDecompile(), "159");
    EXPECT_EQ(right->getDecompile(), "0");
  }
};

// ===========================================================================
// package aes_pkg
// ===========================================================================

TEST_F(LongHexTest, PackageExists) { EXPECT_NE(getPkg(), nullptr); }

// 6.20.4: "local parameters can be declared in a ... package ... In these
// contexts, the parameter keyword shall be a synonym for the localparam
// keyword."
TEST_F(LongHexTest, PackageParameterExistsAndIsLocal) {
  const hldb::Parameter *const p = getPkgParam();
  ASSERT_NE(p, nullptr) << "parameter 'RndCnstMaskingLfsrSeedDefault' not found in aes_pkg";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: 'parameter' in a package is a synonym for 'localparam'";
}

TEST_F(LongHexTest, PackageParameterTypeIsLogic159To0) { expectLogic159To0(getPkgParam()); }

// 5.7.1: 160'h... is a sized hex literal of exactly 160 bits.
TEST_F(LongHexTest, LongHexLiteralIsSized160Hex) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign>("RndCnstMaskingLfsrSeedDefault", pkg->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "RHS should be a Constant literal";
  EXPECT_EQ(c->getConstType(), vpiHexConst);
  EXPECT_EQ(c->getSize(), 160);
  EXPECT_EQ(c->getDecompile(), "160'hc132b5723c5a4cf4743b3c7c32d580f74f1713a");
}

// 5.7.1: all significant digits must be retained (no 64-bit truncation).
TEST_F(LongHexTest, LongHexLiteralValueIsNotTruncated) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign>("RndCnstMaskingLfsrSeedDefault", pkg->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(normalizeHex(c->getValue()), "c132b5723c5a4cf4743b3c7c32d580f74f1713a");
}

TEST_F(LongHexTest, PackageParamAssignLhsBindsToParameter) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign>("RndCnstMaskingLfsrSeedDefault", pkg->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual(), getPkgParam());
}

// ===========================================================================
// module aes_cipher_core
// ===========================================================================

TEST_F(LongHexTest, CipherCoreImportsPackageWildcard) {
  const hldb::Module *const core = getModule("aes_cipher_core");
  ASSERT_NE(core, nullptr);
  const hldb::ImportTypespec *const imp = hldb::findByName<hldb::ImportTypespec>("aes_pkg", core->getTypespecs());
  ASSERT_NE(imp, nullptr) << "26.4: 'import aes_pkg::*;' in the module header";
  const hldb::Constant *const item = imp->getItem();
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getDecompile(), "*");
}

TEST_F(LongHexTest, CipherCoreParameterTypeAndDefault) {
  const hldb::Parameter *const p = getCoreParam();
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam());
  expectLogic159To0(p);

  const hldb::Module *const core = getModule("aes_cipher_core");
  ASSERT_NE(core, nullptr);
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign>("RndCnstMaskingLfsrSeed", core->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "0");
}

// 6.20.2 / 7.4.3: part-select [31:0] of the 160-bit parameter.
TEST_F(LongHexTest, ContAssignRhsIsPartSelectOfParameter) {
  const hldb::Module *const core = getModule("aes_cipher_core");
  ASSERT_NE(core, nullptr);
  ASSERT_NE(core->getContAssigns(), nullptr);
  ASSERT_EQ(core->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = core->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::PartSelect *const ps = ca->getRhs<hldb::PartSelect>();
  ASSERT_NE(ps, nullptr) << "RHS should be a PartSelect";
  const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "RndCnstMaskingLfsrSeed");
  ASSERT_NE(prefix->getActual(), nullptr);
  EXPECT_EQ(prefix->getActual(), getCoreParam());
  const hldb::Range *const r = ps->getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "31");
  EXPECT_EQ(right->getDecompile(), "0");
}

// 6.10: "x" on the LHS of a continuous assignment is an implicit scalar wire.
TEST_F(LongHexTest, ImplicitNetXIsCreated) {
  const hldb::Module *const core = getModule("aes_cipher_core");
  ASSERT_NE(core, nullptr);
  const hldb::Net *const x = hldb::findByName<hldb::Net>("x", core->getNets());
  ASSERT_NE(x, nullptr) << "6.10: an implicit scalar net 'x' of the default net type shall be assumed";
  EXPECT_TRUE(x->getImplicitDecl());
  EXPECT_EQ(x->getNetType(), vpiWire);
  EXPECT_TRUE(x->getScalar());
}

TEST_F(LongHexTest, ContAssignLhsBindsToImplicitNet) {
  const hldb::Module *const core = getModule("aes_cipher_core");
  ASSERT_NE(core, nullptr);
  ASSERT_NE(core->getContAssigns(), nullptr);
  ASSERT_EQ(core->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = core->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "x");
  ASSERT_NE(lhs->getActual(), nullptr) << "6.10: LHS 'x' must resolve to its implicit net";
  EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Net);
}

// ===========================================================================
// module aes_core: by-name override from the imported package parameter
// ===========================================================================

TEST_F(LongHexTest, AesCoreInstantiatesCipherCore) {
  const hldb::Module *const top = getModule("aes_core");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst =
      hldb::findByName<hldb::RefInstance>("u_aes_cipher_core", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  const hldb::RefTypespec *const rt = inst->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::ModuleTypespec *const mt = rt->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getDefName(), "aes_cipher_core");
  ASSERT_NE(mt->getModule(), nullptr);
  EXPECT_EQ(mt->getModule(), getModule("aes_cipher_core"));
}

// 23.10.2.2 + 26.3/26.4
TEST_F(LongHexTest, OverrideByNameBindsBothSides) {
  const hldb::Module *const top = getModule("aes_core");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst =
      hldb::findByName<hldb::RefInstance>("u_aes_cipher_core", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mt->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());

  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "RndCnstMaskingLfsrSeed");
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual(), getCoreParam());

  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "RndCnstMaskingLfsrSeedDefault");
  ASSERT_NE(rhs->getActual(), nullptr) << "26.4: wildcard import makes the package parameter visible";
  EXPECT_EQ(rhs->getActual(), getPkgParam());
}

TEST_F(LongHexTest, NoBindingFailures) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "RndCnstMaskingLfsrSeedDefault"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "RndCnstMaskingLfsrSeed"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
