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

// Tests for dut.sv (tags: LargeHexCast)
//   package kmac_pkg;
//      typedef struct packed {
//         logic [95:0] Prefix;
//      } app_config_t;
//
//      parameter app_config_t A = '{
//         Prefix: 96'(96'h 4c52_5443_5f4d_4f52_4001_0001)
//      };
//
//      parameter app_config_t B = '{
//         Prefix: 96'h 4c52_5443_5f4d_4f52_4001_0001
//      };
//   endpackage : kmac_pkg
//
//   module kmac_entropy;
//      import kmac_pkg::*;
//   endmodule : kmac_entropy
//
// What is checked (IEEE 1800-2023):
//   - package kmac_pkg exists with end label "kmac_pkg" (26.2)
//   - typedef app_config_t aliases a packed struct (7.2.1) with exactly one
//     member "Prefix" of type logic [95:0]
//   - 6.20.1: "All param_assignments appearing within a ... package ... shall
//     become localparam declarations" -- A and B are local parameters typed
//     by app_config_t
//   - A and B are assignment patterns (10.9.2) -> vpiAssignmentPatternOp;
//     per 37.59 detail 6 the operand iteration "shall return the expressions
//     as if the assignment pattern were written with the positional
//     notation", so the single operand is the Prefix value expression itself
//   - A's Prefix value is a size cast 96'(...) (6.24.1) -> vpiCastOp; per
//     37.59 detail 3 a cast "is represented as a unary operation, with its
//     sole argument being the expression being cast, and the typespec of the
//     cast expression being the type to which the argument is being cast",
//     and per detail 5 that typespec "shall always be available"
//   - the hex literal "96'h 4c52_..." is a sized hex literal (5.7.1:
//     whitespace is allowed between the base format and the digits,
//     underscores are ignored): vpiHexConst, size 96
//   - module kmac_entropy (end label "kmac_entropy") has a wildcard import
//     of kmac_pkg (26.3)
//
// What is NOT checked and why:
//   - the HLDB string encoding of Constant::getValue(): not standard-defined.
//   - the exact kind of typespec produced by the size cast: 6.24.1 only says
//     the operand is padded/truncated to 96 bits with signedness unchanged.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class LargeHexCastTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LargeHexCast.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static constexpr std::string_view kHexText = "96'h 4c52_5443_5f4d_4f52_4001_0001";

  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("kmac_pkg", m_design->getAllPackages());
  }

  static const hldb::Operation *getPattern(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>(name, pkg->getParamAssigns());
    if (pa == nullptr) return nullptr;
    return pa->getRhs<hldb::Operation>();
  }

  // The Prefix value expression of the pattern, unwrapping a TaggedPattern
  // if HLC keeps the key (the positional-form rule is checked separately).
  static const hldb::Any *getPrefixValue(std::string_view name) {
    const hldb::Operation *const op = getPattern(name);
    if ((op == nullptr) || (op->getOperands() == nullptr) || op->getOperands()->empty()) return nullptr;
    const hldb::Any *const first = op->getOperands()->at(0);
    if (const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(first)) return tp->getPattern();
    return first;
  }

  static void checkHex(const hldb::Constant *c) {
    EXPECT_EQ(c->getConstType(), vpiHexConst);
    EXPECT_EQ(c->getSize(), 96) << "5.7.1: the size constant gives the exact width";
    EXPECT_EQ(c->getDecompile(), kHexText);
  }
};

TEST_F(LargeHexCastTest, PackageExistsWithEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  EXPECT_EQ(pkg->getEndLabel(), "kmac_pkg");
}

TEST_F(LargeHexCastTest, TypedefAppConfigIsPackedStruct) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("app_config_t", pkg->getTypedefs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "app_config_t aliases a struct";
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = s->getMembers()->at(0);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "Prefix");
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "Prefix is 'logic [95:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "95");
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(LargeHexCastTest, PackageParametersAreLocalTypedByAppConfig) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  for (std::string_view name : {"A", "B"}) {
    const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_TRUE(p->getLocalParam()) << "6.20.1: package parameters become localparam";
    ASSERT_NE(p->getTypespec(), nullptr) << name;
    EXPECT_EQ(p->getTypespec()->getName(), "app_config_t") << name;
    const hldb::TypedefTypespec *const tt = p->getTypespec()->getActual<hldb::TypedefTypespec>();
    ASSERT_NE(tt, nullptr) << name;
    EXPECT_EQ(tt->getName(), "app_config_t");
  }
}

TEST_F(LargeHexCastTest, ParameterValuesAreAssignmentPatterns) {
  for (std::string_view name : {"A", "B"}) {
    const hldb::Operation *const op = getPattern(name);
    ASSERT_NE(op, nullptr) << name << " should be initialized by an assignment pattern";
    EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp) << name;
    ASSERT_NE(op->getOperands(), nullptr) << name;
    EXPECT_EQ(op->getOperands()->size(), 1u) << name << ": the struct has exactly one member";
  }
}

TEST_F(LargeHexCastTest, AssignmentPatternOperandsArePositional) {
  for (std::string_view name : {"A", "B"}) {
    const hldb::Operation *const op = getPattern(name);
    ASSERT_NE(op, nullptr) << name;
    ASSERT_NE(op->getOperands(), nullptr) << name;
    ASSERT_EQ(op->getOperands()->size(), 1u) << name;
    const hldb::Any *const first = op->getOperands()->at(0);
    ASSERT_NE(first, nullptr);
    EXPECT_NE(first->getAnyType(), hldb::AnyType::TaggedPattern)
        << "37.59 detail 6: operands of vpiAssignmentPatternOp are returned in positional form (" << name << ")";
  }
}

TEST_F(LargeHexCastTest, BPrefixIsSizedHexLiteral) {
  const hldb::Constant *const c = any_cast<hldb::Constant>(getPrefixValue("B"));
  ASSERT_NE(c, nullptr) << "B's Prefix value is the literal itself";
  checkHex(c);
}

TEST_F(LargeHexCastTest, APrefixIsUnarySizeCast) {
  const hldb::Operation *const cast = any_cast<hldb::Operation>(getPrefixValue("A"));
  ASSERT_NE(cast, nullptr) << "A's Prefix value is 96'(...)";
  EXPECT_EQ(cast->getOpType(), vpiCastOp);
  EXPECT_NE(cast->getTypespec(), nullptr) << "37.59 detail 5: a vpiCastOp always has a typespec (the target type)";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u) << "37.59 detail 3: a cast is a unary operation";
  const hldb::Constant *const c = any_cast<hldb::Constant>(cast->getOperands()->at(0));
  ASSERT_NE(c, nullptr) << "the sole operand is the expression being cast";
  checkHex(c);
}

TEST_F(LargeHexCastTest, ModuleKmacEntropyImportsPackage) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("kmac_entropy", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getEndLabel(), "kmac_entropy");
  const hldb::ImportTypespec *imp = nullptr;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) {
        imp = it;
        break;
      }
    }
  }
  ASSERT_NE(imp, nullptr) << "26.3: 'import kmac_pkg::*;'";
  EXPECT_EQ(imp->getName(), "kmac_pkg");
  ASSERT_NE(imp->getItem(), nullptr);
  EXPECT_EQ(imp->getItem()->getDecompile(), "*") << "wildcard import";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
