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

// Tests for tests/FilePackageImport/dut.sv:
//
//   package pkg_b;
//     typedef logic [1:0] DCacheWayPath;
//   endpackage : pkg_b
//
//   import pkg_b::*;
//
//   function automatic int TreeLRU_CalcWriteEnable(logic weIn, DCacheWayPath way);
//       return we;
//   endfunction
//
// 'import pkg_b::*;' (a wildcard package import, IEEE 1800-2023 Sec 26.3)
// appears at compilation-unit scope, before 'TreeLRU_CalcWriteEnable' -- so
// the function's second argument type 'DCacheWayPath' (declared inside
// 'pkg_b') must resolve without a 'pkg_b::' qualifier.
//
// The function body 'return we;' references an identifier 'we' that is
// declared nowhere in this file (the first argument is named 'weIn', not
// 'we' -- this looks like a typo in the original source, but per the test
// guide we assert what the standard actually requires for an unresolved
// identifier used in an expression context, not "fix" the source). Per
// IEEE 1800-2023 Sec 23.8/general identifier-resolution rules, a name used
// in an ordinary expression context (not a continuous-assignment LHS or an
// undriven port connection -- see Sec 6.10 "Implicit declarations", which
// applies only to net-expression contexts) that does not resolve to any
// declaration is illegal and must be diagnosed; it is NOT implicitly
// declared. So a binding failure for 'we' is expected here.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/import_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class FilePackageImportTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FilePackageImport.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkgB() { return hldb::findByName<hldb::Package>("pkg_b", m_design->getAllPackages()); }
  static const hldb::Function *getFunc() {
    if (m_design->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("TreeLRU_CalcWriteEnable", m_design->getTaskFuncs());
  }
};

TEST_F(FilePackageImportTest, PackagePkgBExists) { ASSERT_NE(getPkgB(), nullptr) << "package 'pkg_b' not found"; }

// 'typedef logic [1:0] DCacheWayPath;' inside pkg_b.
TEST_F(FilePackageImportTest, PkgBDeclaresDCacheWayPathTypedef) {
  const hldb::Package *const pkg = getPkgB();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *const tt =
      hldb::findByName<hldb::TypedefTypespec>("DCacheWayPath", pkg->getTypespecs());
  ASSERT_NE(tt, nullptr) << "'DCacheWayPath' typedef not found in 'pkg_b'";
}

// 26.3: 'import pkg_b::*;' at compilation-unit scope must make 'pkg_b's
// members reachable in the same compilation unit.
TEST_F(FilePackageImportTest, WildcardImportOfPkgBExists) {
  ASSERT_NE(m_design->getTypespecs(), nullptr) << "no compilation-unit-scope typespecs recorded";
  const hldb::ImportTypespec *imp = nullptr;
  for (const hldb::Typespec *const t : *m_design->getTypespecs()) {
    if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(t)) {
      if (it->getName() == std::string_view{"pkg_b"}) {
        imp = it;
        break;
      }
    }
  }
  ASSERT_NE(imp, nullptr) << "'import pkg_b::*;' not found at compilation-unit scope";
}

TEST_F(FilePackageImportTest, FunctionTreeLRU_CalcWriteEnableExists) {
  ASSERT_NE(getFunc(), nullptr) << "'TreeLRU_CalcWriteEnable' function not found at compilation-unit scope";
}

// '(logic weIn, DCacheWayPath way)': two input arguments.
TEST_F(FilePackageImportTest, FunctionHasTwoInputArguments) {
  const hldb::Function *const func = getFunc();
  ASSERT_NE(func, nullptr);
  ASSERT_NE(func->getIODecls(), nullptr);
  ASSERT_EQ(func->getIODecls()->size(), 2u);
  EXPECT_EQ(func->getIODecls()->at(0)->getName(), std::string_view{"weIn"});
  EXPECT_EQ(func->getIODecls()->at(1)->getName(), std::string_view{"way"});
  EXPECT_EQ(func->getIODecls()->at(0)->getDirection(), vpiInput);
  EXPECT_EQ(func->getIODecls()->at(1)->getDirection(), vpiInput);
}

// The second argument's type must resolve to 'pkg_b::DCacheWayPath',
// reachable unqualified via the wildcard import.
TEST_F(FilePackageImportTest, SecondArgumentTypeResolvesToDCacheWayPath) {
  const hldb::Function *const func = getFunc();
  ASSERT_NE(func, nullptr);
  ASSERT_EQ(func->getIODecls()->size(), 2u);
  const hldb::IODecl *const way = func->getIODecls()->at(1);
  ASSERT_NE(way->getTypespec(), nullptr) << "'DCacheWayPath way': typespec reference must be non-null";
  ASSERT_NE(way->getTypespec()->getActual(), nullptr) << "'DCacheWayPath' must resolve to an actual typespec";
  const hldb::TypedefTypespec *const actual = way->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(actual, nullptr) << "'DCacheWayPath' must resolve to a TypedefTypespec";
  EXPECT_EQ(actual->getName(), std::string_view{"DCacheWayPath"});
}

// 'return we;' -- 'we' is not declared anywhere; per the standard this is
// an ordinary (non-net) expression context, so it must fail to bind rather
// than being implicitly declared.
TEST_F(FilePackageImportTest, ReturnStatementReferencesUnresolvedWe) {
  const hldb::Function *const func = getFunc();
  ASSERT_NE(func, nullptr);
  const hldb::ReturnStmt *const ret = func->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "'return we;' should be a plain ReturnStmt";
  ASSERT_NE(ret->getCondition(), nullptr);
  const hldb::RefObj *const weRef = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(weRef, nullptr) << "'we' should parse as a RefObj";
  EXPECT_EQ(weRef->getName(), std::string_view{"we"});
  EXPECT_EQ(weRef->getActual(), nullptr) << "'we' is never declared and must not resolve to any actual";
}

TEST_F(FilePackageImportTest, UnresolvedWeIsReportedAsBindingError) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"we"}), nullptr)
      << "'return we;' references an undeclared identifier and must be diagnosed as a failed bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
