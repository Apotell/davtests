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

#include <Surelog/Tests/Test.h>

#include <Surelog/CommandLine/CommandLineParser.h>
#include <Surelog/Common/FileSystem.h>
#include <Surelog/Common/Session.h>
#include <Surelog/ErrorReporting/ErrorContainer.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Utils/StringUtils.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace SURELOG {
namespace fs = std::filesystem;

Session *Test::m_session = nullptr;
Compiler *Test::m_compiler = nullptr;
uhdm::Design *Test::m_design = nullptr;

constexpr std::string_view kWorkingDirParam = "-wd";
constexpr std::string_view kOutputDirParam = "-o";

void Test::Compile(const std::filesystem::path &filepath, const std::initializer_list<std::string_view> &args) {
  const auto now = std::chrono::system_clock::now();
  const auto epoch = now.time_since_epoch();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);

  std::vector<const char *> cargs;
  cargs.reserve(args.size() + 5);

  const std::string programPath = LocalStore::getProgramPath().string();
  const std::string workingDir = filepath.parent_path().string();
  cargs.emplace_back(programPath.c_str());
  cargs.emplace_back(kWorkingDirParam.data());
  cargs.emplace_back(workingDir.c_str());

  for (const std::string_view &arg : args) {
    if (!arg.empty()) {
      cargs.emplace_back(arg.data());
    }
  }

  const std::string outputDir = StrCat(::testing::TempDir(), "hlc_test_", ms.count());
  cargs.emplace_back(kOutputDirParam.data());
  cargs.emplace_back(outputDir.c_str());

  m_session = new Session;

  CommandLineParser *const clp = m_session->getCommandLineParser();
  bool success = m_session->parseCommandLine(static_cast<int32_t>(cargs.size()), cargs.data());
  ASSERT_TRUE(success) << "Failed to parse command line";

  clp->setMaxProcesses(0);
  clp->setwritePpOutput(true);

  m_compiler = new Compiler(m_session);
  success = m_compiler->compile();
  ASSERT_TRUE(success) << "Compilation Failed";

  m_design = m_compiler->getUhdmDesign();
  ASSERT_NE(m_design, nullptr) << "Compiler return a null design";
}
}  // namespace SURELOG
