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

// FileList.hlc compiles two separate source files as one compilation:
//   pack.sv:
//     package prim_util_pkg;
//       function automatic integer vbits(integer value);
//         return (value == 1) ? 1 : $clog2(value);
//       endfunction
//     endpackage
//   dut.sv:
//     module foo(input clk, output out);
//       import prim_util_pkg::vbits;
//       logic [vbits(4)-1:0] a;
//       always @(posedge clk) begin
//         a <= a + 1'b1;
//       end
//       assign out = a[0];
//     endmodule
//
// IEEE 1800-2023 Sec 3.12.1/3.12.2: multiple source files compiled together
// form a single compilation, so a package declared in pack.sv is visible in
// dut.sv via an explicit 'import' (Sec 26.3 "Package import declaration").

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class FileListTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileList.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("prim_util_pkg", m_design->getAllPackages());
  }
  static const hldb::Module *getFoo() { return hldb::findByDefName<hldb::Module>("foo", m_design->getAllModules()); }
};

// pack.sv declares the package that dut.sv depends on -- both files must
// have been read as part of the same compilation for 'foo' to resolve.
TEST_F(FileListTest, PackagePrimUtilPkgExists) { ASSERT_NE(getPkg(), nullptr) << "package 'prim_util_pkg' not found"; }

TEST_F(FileListTest, PackageHasVbitsFunction) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("vbits", pkg->getTaskFuncs()), nullptr);
}

TEST_F(FileListTest, ModuleFooExists) { ASSERT_NE(getFoo(), nullptr) << "module 'foo' not found in dut.sv"; }

TEST_F(FileListTest, FooPortsClkAndOutExist) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getPorts(), nullptr);
  const hldb::Port *const clk = hldb::findByName<hldb::Port>("clk", foo->getPorts());
  const hldb::Port *const out = hldb::findByName<hldb::Port>("out", foo->getPorts());
  ASSERT_NE(clk, nullptr) << "port 'clk' not found";
  ASSERT_NE(out, nullptr) << "port 'out' not found";
  EXPECT_EQ(clk->getDirection(), vpiInput) << "'input clk' must have vpiInput direction";
  EXPECT_EQ(out->getDirection(), vpiOutput) << "'output out' must have vpiOutput direction";
}

// 'import prim_util_pkg::vbits;' -- Sec 26.3: an explicit (non-wildcard)
// package import. It must place 'vbits' in scope for 'dut.sv' so the
// declaration 'logic [vbits(4)-1:0] a;' (a constant function call) resolves.
TEST_F(FileListTest, FooDeclaresVariableA) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getVariables(), nullptr) << "'logic [vbits(4)-1:0] a;' should declare a variable 'a'";
  ASSERT_EQ(foo->getVariables()->size(), 1u);
  EXPECT_EQ(foo->getVariables()->at(0)->getName(), std::string_view{"a"});
}

// 'assign out = a[0];' -- Sec 10.3.2 continuous assignment, LHS 'out'.
TEST_F(FileListTest, ContAssignToOutExists) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getContAssigns(), nullptr);
  const hldb::ContAssign *ca = nullptr;
  for (const hldb::ContAssign *const c : *foo->getContAssigns()) {
    ASSERT_NE(c, nullptr);
    const hldb::RefObj *const lhs = c->getLhs<hldb::RefObj>();
    if (lhs != nullptr && lhs->getName() == std::string_view{"out"}) {
      ca = c;
      break;
    }
  }
  ASSERT_NE(ca, nullptr) << "'assign out = a[0];' ContAssign not found";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
