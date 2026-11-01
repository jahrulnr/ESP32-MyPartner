#include "services/McpProtocol.h"
#include "domain/DeviceUi.h"
#include "domain/CircularLog.h"
#include "domain/SpeechText.h"
#include <cassert>
#include <string>
#include <iostream>
using namespace partner;
struct Mock : IMcpActions {
  bool subscribed = false;
  bool readResource(const char* uri, std::string& result) override {
    if (std::string(uri) != "partner://sensors/presence") return false;
    result = R"({"available":true,"detected":true})"; return true;
  }
  bool subscribeResource(const char* uri, bool enabled) override {
    if (std::string(uri) != "partner://sensors/presence") return false;
    subscribed = enabled; return true;
  }
  bool call(const char* name, JsonVariantConst args, std::string& result, std::string& error) override {
    if (std::string(name) == "menu" && !args["screen"].is<const char*>()) { error = "screen required"; return false; }
    result = "done"; return true;
  }
};
int main() {
  Mock mock; McpProtocol mcp(mock); std::string out;
  assert(mcp.handle("{", out)); JsonDocument result; assert(!deserializeJson(result, out)); assert(result["error"]["code"] == -32700);
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":"x","method":"initialize","params":{"protocolVersion":"2025-11-25"}})",out));
  assert(!deserializeJson(result,out)); assert(result["id"] == "x"); assert(result["result"]["protocolVersion"] == "2025-06-18");
  assert(!mcp.handle(R"({"jsonrpc":"2.0","method":"notifications/initialized"})",out));
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})",out));
  assert(!deserializeJson(result,out)); assert(result["result"]["tools"].size() == 11);
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":10,"method":"resources/list"})",out));
  assert(!deserializeJson(result,out)); assert(result["result"]["resources"].size() == 2);
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":11,"method":"resources/subscribe","params":{"uri":"partner://sensors/presence"}})",out));
  assert(!deserializeJson(result,out)); assert(mock.subscribed && result["result"].is<JsonObject>());
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":12,"method":"resources/read","params":{"uri":"partner://sensors/presence"}})",out));
  assert(!deserializeJson(result,out)); assert(result["result"]["contents"][0]["mimeType"] == "application/json");
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":13,"method":"resources/unsubscribe","params":{"uri":"partner://sensors/presence"}})",out));
  assert(!deserializeJson(result,out)); assert(!mock.subscribed);
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":14,"method":"resources/read","params":{"uri":"partner://missing"}})",out));
  assert(!deserializeJson(result,out)); assert(result["error"]["code"] == -32002);
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"menu","arguments":{"screen":"home"}}})",out));
  assert(!deserializeJson(result,out)); assert(result["result"]["content"][0]["text"] == "done");
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"menu","arguments":{}}})",out));
  assert(!deserializeJson(result,out)); assert(result["result"]["isError"] == true);
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"missing"}})",out));
  assert(!deserializeJson(result,out)); assert(result["error"]["code"] == -32602);
  assert(mcp.handle(R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"menu","arguments":null}})",out));
  assert(!deserializeJson(result,out)); assert(result["error"]["code"] == -32602);
  Screen screen; Expression expression;
  assert(parseScreen("sensors", screen) && screen == Screen::Sensors && !parseScreen("missing", screen));
  assert(parseExpression("sleepy", expression) && expression == Expression::Sleepy && !parseExpression("missing", expression));
  CircularLog<5> log; char buf[5]; log.append("abc",3); log.append("defg",4); assert(log.copy(buf,5)==5); assert(std::string(buf,5)=="cdefg");
  assert(!validSpeechText(""));
  assert(validSpeechText(std::string(255, 'a').c_str()));
  assert(!validSpeechText(std::string(256, 'a').c_str()));
  assert(!validSpeechText("\xF0\x9F"));
  assert(!validSpeechText("\xED\xA0\x80"));
  std::string unicode;
  for (int i = 0; i < 255; ++i) unicode += "\xF0\x9F\x99\x82";
  assert(validSpeechText(unicode.c_str()));
  unicode += "x";
  assert(!validSpeechText(unicode.c_str()));
  std::cout << "MCP: lifecycle, catalog, calls, errors, UI names and log ring passed\n";
}
