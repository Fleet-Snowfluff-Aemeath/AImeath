#include <benchmark/benchmark.h>
#include "llm_client.hpp"
#include "llm_utils.hpp"
#include <boost/asio.hpp>

// ====== LlmClient ======

static void BM_LlmClientCreateDestroy(benchmark::State& state)
{
    boost::asio::io_context io;
    for (auto _ : state)
    {
        auto client = std::make_shared<LlmClient>(io);
        benchmark::DoNotOptimize(client);
    }
}
BENCHMARK(BM_LlmClientCreateDestroy);

static void BM_LlmClientCreateCancel(benchmark::State& state)
{
    boost::asio::io_context io;
    for (auto _ : state)
    {
        auto client = std::make_shared<LlmClient>(io);
        client->cancel();
        benchmark::DoNotOptimize(client);
    }
}
BENCHMARK(BM_LlmClientCreateCancel);

static void BM_LlmClientStartCancel(benchmark::State& state)
{
    boost::asio::io_context io;
    for (auto _ : state)
    {
        auto client = std::make_shared<LlmClient>(io);
        client->start("nonexistent.invalid", "443", "{}", "",
            [](LlmEvent) {}, [](std::string, std::string, int, int) {},
            std::chrono::seconds(1));
        client->cancel();
        io.run_for(std::chrono::milliseconds(10));
    }
}
BENCHMARK(BM_LlmClientStartCancel);

// ====== Llm data structs ======

static void BM_LlmUsageCreate(benchmark::State& state)
{
    for (auto _ : state)
    {
        LlmUsage usage;
        benchmark::DoNotOptimize(usage);
    }
}
BENCHMARK(BM_LlmUsageCreate);

static void BM_LlmToolCallCreate(benchmark::State& state)
{
    for (auto _ : state)
    {
        LlmToolCall tc;
        benchmark::DoNotOptimize(tc);
    }
}
BENCHMARK(BM_LlmToolCallCreate);

static void BM_LlmToolCallPopulated(benchmark::State& state)
{
    for (auto _ : state)
    {
        LlmToolCall tc;
        tc.id = "call_abc123";
        tc.type = "function";
        tc.function_name = "open_app";
        tc.function_arguments = "{\"app\":\"snake\"}";
        benchmark::DoNotOptimize(tc);
    }
}
BENCHMARK(BM_LlmToolCallPopulated);

// ====== llm utilities ======

static void BM_LlmBuildChatBody(benchmark::State& state)
{
    int N = state.range(0);
    boost::json::array msgs;
    for (int i = 0; i < N; ++i)
    {
        boost::json::object msg;
        msg["role"] = "user";
        msg["content"] = "message " + std::to_string(i);
        msgs.push_back(msg);
    }
    for (auto _ : state)
    {
        auto body = llm::build_chat_body(msgs, "test-model", true, false);
        benchmark::DoNotOptimize(body);
    }
}
BENCHMARK(BM_LlmBuildChatBody)->Arg(1)->Arg(10)->Arg(100);

static void BM_LlmGetDefaultTools(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto tools = llm::get_default_tools();
        benchmark::DoNotOptimize(tools);
    }
}
BENCHMARK(BM_LlmGetDefaultTools);

static void BM_LlmInjectTools(benchmark::State& state)
{
    std::string body = R"({"model":"test","messages":[{"role":"user","content":"hello"}]})";
    for (auto _ : state)
    {
        std::string copy = body;
        llm::inject_tools(copy, true);
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_LlmInjectTools);

static void BM_LlmInjectToolsDisabled(benchmark::State& state)
{
    std::string body = R"({"model":"test","messages":[{"role":"user","content":"hello"}]})";
    for (auto _ : state)
    {
        std::string copy = body;
        llm::inject_tools(copy, false);
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_LlmInjectToolsDisabled);

static void BM_LlmMergeToolCalls(benchmark::State& state)
{
    int N = state.range(0);
    std::vector<LlmToolCall> chunks;
    for (int i = 0; i < N; ++i)
    {
        LlmToolCall tc;
        tc.id = "call_" + std::to_string(i % 5);
        tc.function_name = "func" + std::to_string(i % 5);
        tc.function_arguments = std::string(10, 'x');
        chunks.push_back(tc);
    }
    for (auto _ : state)
    {
        auto merged = llm::merge_tool_calls(chunks);
        benchmark::DoNotOptimize(merged);
    }
}
BENCHMARK(BM_LlmMergeToolCalls)->Arg(1)->Arg(10)->Arg(100)->Arg(500);

BENCHMARK_MAIN();
