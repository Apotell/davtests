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

// FileListWithCompressed.hlc compiles pack.sv together with dut.sv.gz, a
// gzip-compressed source file the tool must transparently decompress before
// preprocessing. Decompressing tests/FileListWithCompressed/dut.sv.gz
// confirms its contents are byte-identical to tests/FileList/dut.sv:
//   module foo(input clk, output out);
//     import prim_util_pkg::vbits;
//     logic [vbits(4)-1:0] a;
//     always @(posedge clk) begin
//       a <= a + 1'b1;
//     end
//     assign out = a[0];
//   endmodule
// and pack.sv is identical to tests/FileList/pack.sv:
//   package prim_util_pkg;
//     function automatic integer vbits(integer value);
//       return (value == 1) ? 1 : $clog2(value);
//     endfunction
//   endpackage
//
// IEEE 1800-2023 does not mandate transparent decompression of compressed
// source files -- that is purely a tool convenience feature -- but once
// decompressed the file must be compiled exactly as the plain-text
// equivalent in tests/FileList/. This test therefore mirrors
// hlc/FileList/FileList/test_FileList.cpp's assertions, verifying the
// decompressed-file compile produces the same shape.

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

class FileListWithCompressedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FileListWithCompressed.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("prim_util_pkg", m_design->getAllPackages());
  }
  static const hldb::Module *getFoo() { return hldb::findByDefName<hldb::Module>("foo", m_design->getAllModules()); }
};

// pack.sv declares the package; dut.sv.gz (compressed) depends on it -- both
// must be read as part of the same compilation, after decompression, for
// 'foo' to resolve.
TEST_F(FileListWithCompressedTest, PackagePrimUtilPkgExists) {
  ASSERT_NE(getPkg(), nullptr) << "package 'prim_util_pkg' not found";
}

TEST_F(FileListWithCompressedTest, PackageHasVbitsFunction) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("vbits", pkg->getTaskFuncs()), nullptr);
}

// Module 'foo' must compile from the decompressed dut.sv.gz.
TEST_F(FileListWithCompressedTest, ModuleFooExists) {
  ASSERT_NE(getFoo(), nullptr) << "module 'foo' not found -- dut.sv.gz was not decompressed/compiled";
}

TEST_F(FileListWithCompressedTest, FooPortsClkAndOutExist) {
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

TEST_F(FileListWithCompressedTest, FooDeclaresVariableA) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getVariables(), nullptr) << "'logic [vbits(4)-1:0] a;' should declare a variable 'a'";
  ASSERT_EQ(foo->getVariables()->size(), 1u);
  EXPECT_EQ(foo->getVariables()->at(0)->getName(), std::string_view{"a"});
}

TEST_F(FileListWithCompressedTest, ContAssignToOutExists) {
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
