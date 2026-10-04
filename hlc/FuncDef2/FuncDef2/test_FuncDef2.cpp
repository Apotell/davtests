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

// Tests for FuncDef2.hlc (tests/FuncDef2/dut.sv):
//   package tnoc_pkg;
//     ...
//     function automatic int tnoc_clog2(bit [31:0] n);
//       int result;
//       result = 0;
//       for (int i = 31; i >= 0; --i) begin
//         if (n[i]) begin
//           result = i;
//           break;
//         end
//       end
//       if ((2**result) == n) begin
//         return result;
//       end
//       else begin
//         return result + 1;
//       end
//     endfunction
//     ... 33 more "function automatic ..." definitions ...
//   endpackage
//   module tnoc_vc_splitter import tnoc_pkg::*; #( ... ) ( ... );
//     ...
//   endmodule
//
// This is a second, distinct function-definition corner case vs. FuncDef:
// where FuncDef exercises a non-ANSI, static-lifetime, module-local
// function with an implicit "return by assignment to the function name"
// (13.4.1) and no explicit "return" statement, FuncDef2 exercises ANSI-style
// functions declared "automatic" (explicit dynamic lifetime, 13.4.2) inside
// a package, using explicit "return <expr>;" statements (13.4.1, second
// return mechanism) instead of assigning to the function's own name, with a
// two-state "bit" formal argument instead of "integer"/"logic".
//
// What is checked:
//   - package tnoc_pkg exists and declares exactly 34 functions (one
//     "function automatic ..." per line in the source -- confirmed by
//     directly counting occurrences in tests/FuncDef2/dut.sv, not from any
//     .log dump)
//   - "tnoc_clog2" is automatic (13.4.2: explicit 'automatic' keyword)
//   - "tnoc_clog2" returns int -> IntTypespec
//   - "tnoc_clog2" has exactly 1 formal IODecl "n", direction vpiInput,
//     whose typespec resolves to BitTypespec (two-state 'bit', vector,
//     32-bit range) -- distinct from FuncDef's IntegerTypespec argument
//   - module tnoc_vc_splitter exists (sanity: the package is usable from
//     the importing module)
//
// What is NOT checked and why: the detailed body of "tnoc_clog2" (nested
// for/if/break) and the remaining 33 functions are out of scope for this
// "second function-definition corner case" test; FuncDef already covers
// begin-block/for-loop body shape in depth.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncDef2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncDef2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("tnoc_pkg", m_design->getAllPackages());
  }

  static const hldb::Function *getTnocClog2() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr || pkg->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("tnoc_clog2", pkg->getTaskFuncs());
  }

  static const hldb::Module *getSplitter() {
    return hldb::findByName<hldb::Module>("tnoc_vc_splitter", m_design->getAllModules());
  }
};

TEST_F(FuncDef2Test, PackageExists) { ASSERT_NE(getPkg(), nullptr); }

TEST_F(FuncDef2Test, PackageHas34Functions) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 34u);
}

TEST_F(FuncDef2Test, TnocClog2ExistsAndIsAutomatic) {
  const hldb::Function *const f = getTnocClog2();
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->getName(), std::string_view("tnoc_clog2"));
  EXPECT_TRUE(f->getAutomatic()) << "13.4.2: explicit 'automatic' keyword requires dynamic lifetime";
}

TEST_F(FuncDef2Test, TnocClog2ReturnsInt) {
  const hldb::Function *const f = getTnocClog2();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getReturn(), nullptr);
  EXPECT_NE(f->getReturn()->getActual<hldb::IntTypespec>(), nullptr)
      << "'function automatic int tnoc_clog2(...)' should resolve its return typespec to IntTypespec";
}

TEST_F(FuncDef2Test, TnocClog2HasOneInputIODeclNAsBitVector) {
  const hldb::Function *const f = getTnocClog2();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  const hldb::IODecl *const n = f->getIODecls()->at(0);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getName(), std::string_view("n"));
  EXPECT_EQ(n->getDirection(), vpiInput);
  ASSERT_NE(n->getTypespec(), nullptr);
  const hldb::BitTypespec *const bt = n->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr) << "'bit [31:0] n' should resolve to BitTypespec, not IntegerTypespec/LogicTypespec";
  EXPECT_FALSE(bt->getScalar()) << "'bit [31:0]' is a 32-bit vector, not a scalar bit";
}

TEST_F(FuncDef2Test, ImportingModuleExists) { EXPECT_NE(getSplitter(), nullptr); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
