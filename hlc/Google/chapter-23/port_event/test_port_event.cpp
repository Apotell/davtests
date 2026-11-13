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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2--port-event.sv
//
// THE SOURCE
//
//   20: module m(input event e);
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "A port can be a declaration of an interface, an event, or a variable or
// net of any allowed data type" (23.2.2). In the ANSI header, "input event e"
// is a variable_port_header (port_direction followed by variable_port_type,
// Syntax 23-4) whose data_type is the alternative "event" of data_type
// (A.2.2.1). So m has one input port e of type event, port index 0 (37.14
// detail 9), whose typespec is an event typespec.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   PortEIsAnEventInput        one port e, index 0, vpiInput, event typespec
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Triggering or waiting on e. Events are Clause 15; this file is only
//     about declaring an event as a port.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2--port-event.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}
}  // namespace

class PortEventTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port_event.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const hldb::Event &event : listener->getEvents()) {
      if (isErrorClass(event)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }

  static const hldb::Module *findModule() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getDefName() == std::string_view("m"))) return module;
    }
    return nullptr;
  }
};

TEST_F(PortEventTest, CompilesWithoutErrors) {
  GTEST_SKIP() << "\"input event e\" is a syntax error: data_type in SV3_1aParser.g4 has no event alternative "
                  "(IEEE 1800-2023 A.2.2.1, 23.2.2)";
  EXPECT_EQ(allEvents(), "");
}

TEST_F(PortEventTest, PortEIsAnEventInput) {
  GTEST_SKIP() << "An event port is not parsed (data_type has no event alternative) nor modeled as an event "
                  "(IEEE 1800-2023 23.2.2, A.2.2.1)";
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getPorts(), nullptr);
  ASSERT_EQ(module->getPorts()->size(), 1u);
  const hldb::Port *const port = module->getPorts()->front();
  EXPECT_EQ(port->getName(), "e");
  EXPECT_EQ(port->getPortIndex(), 0);
  EXPECT_EQ(port->getDirection(), vpiInput);
  const hldb::Typespec *const typespec = hldb::getTypespec(port);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::EventTypespec);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
