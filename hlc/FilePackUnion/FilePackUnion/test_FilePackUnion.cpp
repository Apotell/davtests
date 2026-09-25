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

// Tests for tests/FilePackUnion/dut.sv (the trailing block after 'endmodule
// top' is entirely commented out in the source and is not under test here):
//
//   package foo_flags_pkg;
//     typedef struct packed { logic a; logic b; logic c; } common_flags_t;
//   endpackage : foo_flags_pkg
//
//   package fooes_pkg;
//     typedef enum logic [1:0] { a, b, c, d } classes_e;
//   endpackage : fooes_pkg
//
//   typedef struct packed {
//     logic a;
//     fooes_pkg::classes_e b;
//   } padded_fooes_t;
//
//   package goog;
//     typedef union packed {
//       foo_flags_pkg::common_flags_t [3:0][7:0] atype_t;
//       padded_fooes_t                [3:0][7:0] btype_t;
//     } top_flag_t;
//   endpackage: goog
//
//   module top(input goog::top_flag_t a, output goog::top_flag_t b);
//     assign b = a;
//     assign c = 4 * 5;
//   endmodule
//
// IEEE 1800-2023 Sec 7.3 "Packed unions": 'top_flag_t' is a packed union
// declared inside package 'goog' with two members, each itself a packed
// aggregate type declared in a different package/scope -- exercising
// packed-union members whose types span multiple declaration sites/files.
//
// IEEE 1800-2023 Sec 6.10 "Implicit declarations": 'assign c = 4 * 5;'
// uses an undeclared identifier 'c' as the LHS of a continuous assignment.
// This IS a net-expression context, so (with no `default_nettype none`
// override present) 'c' must be implicitly declared as a 1-bit net, not
// reported as a binding failure -- unlike an undeclared identifier used in
// an ordinary expression context (see FilePackageImportTest for that case).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/union.h>
#include <hldb/union_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class FilePackUnionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FilePackUnion.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getGoog() { return hldb::findByName<hldb::Package>("goog", m_design->getAllPackages()); }
  static const hldb::Package *getFooFlagsPkg() {
    return hldb::findByName<hldb::Package>("foo_flags_pkg", m_design->getAllPackages());
  }
  static const hldb::Package *getFooesPkg() {
    return hldb::findByName<hldb::Package>("fooes_pkg", m_design->getAllPackages());
  }
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::UnionTypespec *getTopFlagT() {
    const hldb::Package *const pkg = getGoog();
    if (pkg == nullptr || pkg->getTypespecs() == nullptr) return nullptr;
    const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("top_flag_t", pkg->getTypespecs());
    if (tt == nullptr || tt->getTypedef() == nullptr) return nullptr;
    return tt->getTypedef()->getAlias()->getActual<hldb::UnionTypespec>();
  }
};

TEST_F(FilePackUnionTest, AllThreePackagesExist) {
  EXPECT_NE(getFooFlagsPkg(), nullptr) << "package 'foo_flags_pkg' not found";
  EXPECT_NE(getFooesPkg(), nullptr) << "package 'fooes_pkg' not found";
  EXPECT_NE(getGoog(), nullptr) << "package 'goog' not found";
}

TEST_F(FilePackUnionTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr) << "module 'top' not found"; }

// 7.3: 'top_flag_t' is a packed union with exactly two members.
TEST_F(FilePackUnionTest, TopFlagTIsPackedUnionWithTwoMembers) {
  const hldb::UnionTypespec *const ut = getTopFlagT();
  ASSERT_NE(ut, nullptr) << "'goog::top_flag_t' should resolve to a UnionTypespec";
  const hldb::Union *const u = ut->getUnion();
  ASSERT_NE(u, nullptr);
  ASSERT_NE(u->getMembers(), nullptr);
  ASSERT_EQ(u->getMembers()->size(), 2u) << "'top_flag_t' declares exactly two members: atype_t, btype_t";
  EXPECT_EQ(u->getMembers()->at(0)->getName(), std::string_view{"atype_t"});
  EXPECT_EQ(u->getMembers()->at(1)->getName(), std::string_view{"btype_t"});
}

// Ports 'a' and 'b' of 'top' are both typed 'goog::top_flag_t', a
// package-qualified reference into a different package than 'top' itself.
TEST_F(FilePackUnionTest, PortsAAndBExistWithDirections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", top->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", top->getPorts());
  ASSERT_NE(a, nullptr) << "port 'a' not found";
  ASSERT_NE(b, nullptr) << "port 'b' not found";
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiOutput);
}

// 'assign b = a;' -- Sec 10.3.2 continuous assignment between two ports of
// the same packed-union type.
TEST_F(FilePackUnionTest, ContAssignBEqualsAExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::ContAssign *ca = nullptr;
  for (const hldb::ContAssign *const c : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = c->getLhs<hldb::RefObj>();
    if (lhs != nullptr && lhs->getName() == std::string_view{"b"}) {
      ca = c;
      break;
    }
  }
  ASSERT_NE(ca, nullptr) << "'assign b = a;' ContAssign not found";
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(ca->getRhs(), nullptr);
  ASSERT_NE(rhs, nullptr) << "'assign b = a;': RHS must be a RefObj referencing 'a'";
  EXPECT_EQ(rhs->getName(), std::string_view{"a"});
}

// 6.10: 'assign c = 4 * 5;' -- 'c' is undeclared but appears as a
// continuous-assignment LHS, a net-expression context, so it must be
// implicitly declared as a 1-bit net (no `default_nettype none` in effect),
// not reported as a binding failure.
TEST_F(FilePackUnionTest, UndeclaredCBecomesImplicitNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "'assign c = 4 * 5;' should implicitly declare net 'c'";
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr) << "implicit net 'c' not found";
  EXPECT_TRUE(c->getImplicitDecl()) << "6.10: 'c' has no explicit declaration and must be marked implicit";
}

TEST_F(FilePackUnionTest, UndeclaredCIsNotABindingError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"c"}), nullptr)
      << "6.10: an undeclared net-context identifier is implicitly declared, not a binding error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
