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

// Tests for tests/FileTypespec/dut.sv:
//
//   package foo_flags;
//     typedef struct packed { logic a; } common_flags_t;
//   endpackage : foo_flags
//
//   package fooes;
//     typedef enum logic [1:0] { a } classes;
//   endpackage : fooes
//
//   typedef struct packed {
//     fooes::classes b;
//   } padded_fooes_t;
//
//   typedef union packed {
//     foo_flags::common_flags_t atype_t;
//     padded_fooes_t            btype_t;
//   } top_flag_t;
//
//   module top(input top_flag_t a , output top_flag_t b);
//     assign b = a;
//   endmodule
//
// Unlike hlc/FilePackUnion (where the union typedef lives inside a
// package), 'padded_fooes_t' and 'top_flag_t' here are both declared at
// compilation-unit ("$unit") scope (IEEE 1800-2023 Sec 3.12.2), each
// referencing member types declared inside separate packages
// ('fooes::classes', 'foo_flags::common_flags_t'). 'top' then uses the
// unit-scoped 'top_flag_t' unqualified for both of its ports. UHDM records
// compilation-unit scope typedefs directly under Design
// (Design::getTypedefs()), not under any Module or Package.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/union.h>
#include <hldb/union_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class FileTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileTypespec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getFooFlags() {
    return hldb::findByName<hldb::Package>("foo_flags", m_design->getAllPackages());
  }
  static const hldb::Package *getFooes() { return hldb::findByName<hldb::Package>("fooes", m_design->getAllPackages()); }
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Typedef *findUnitTypedef(std::string_view name) {
    if (m_design->getTypedefs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, m_design->getTypedefs());
  }
};

TEST_F(FileTypespecTest, BothPackagesExist) {
  EXPECT_NE(getFooFlags(), nullptr) << "package 'foo_flags' not found";
  EXPECT_NE(getFooes(), nullptr) << "package 'fooes' not found";
}

TEST_F(FileTypespecTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr) << "module 'top' not found"; }

// 'padded_fooes_t' declared at $unit scope, not inside any package.
TEST_F(FileTypespecTest, PaddedFooesTIsUnitScopedTypedef) {
  const hldb::Typedef *const td = findUnitTypedef("padded_fooes_t");
  ASSERT_NE(td, nullptr) << "'padded_fooes_t' not found at compilation-unit scope";
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "'padded_fooes_t' must alias a packed struct";
}

// 'top_flag_t' declared at $unit scope, aliasing a packed union whose two
// members are each declared/typed differently (one refers into a package,
// the other refers to the sibling unit-scoped typedef above).
TEST_F(FileTypespecTest, TopFlagTIsUnitScopedPackedUnionWithTwoMembers) {
  const hldb::Typedef *const td = findUnitTypedef("top_flag_t");
  ASSERT_NE(td, nullptr) << "'top_flag_t' not found at compilation-unit scope";
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  const hldb::UnionTypespec *const ut = td->getAlias()->getActual<hldb::UnionTypespec>();
  ASSERT_NE(ut, nullptr) << "'top_flag_t' must alias a packed union";
  const hldb::Union *const u = ut->getUnion();
  ASSERT_NE(u, nullptr);
  ASSERT_NE(u->getMembers(), nullptr);
  ASSERT_EQ(u->getMembers()->size(), 2u) << "'top_flag_t' declares exactly two members: atype_t, btype_t";
  EXPECT_EQ(u->getMembers()->at(0)->getName(), std::string_view{"atype_t"});
  EXPECT_EQ(u->getMembers()->at(1)->getName(), std::string_view{"btype_t"});
}

// 'module top(input top_flag_t a, output top_flag_t b)' -- both ports use
// the unit-scoped typedef, unqualified.
TEST_F(FileTypespecTest, PortsAAndBTypedTopFlagT) {
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

// 'assign b = a;' -- Sec 10.3.2 continuous assignment between two ports
// sharing the cross-scope 'top_flag_t' union type.
TEST_F(FileTypespecTest, ContAssignBEqualsAExists) {
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

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
