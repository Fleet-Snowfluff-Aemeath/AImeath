#include <benchmark/benchmark.h>
#include "agent_server.hpp"
#include <thread>
#include <vector>

// ====== lifecycle ======

static void BM_AgentCreateDestroy(benchmark::State& state) {
    for (auto _ : state) {
        auto ptr = std::make_shared<agent::AgentServer>();
        ptr->destroy();
    }
}
BENCHMARK(BM_AgentCreateDestroy);

static void BM_AgentCreate(benchmark::State& state) {
    for (auto _ : state) {
        auto ptr = std::make_shared<agent::AgentServer>();
        benchmark::DoNotOptimize(ptr);
        ptr->destroy();
    }
}
BENCHMARK(BM_AgentCreate);

static void BM_AgentDestroy(benchmark::State& state) {
    for (auto _ : state) {
        auto ptr = std::make_shared<agent::AgentServer>();
        ptr->destroy();
    }
}
BENCHMARK(BM_AgentDestroy);

static void BM_AgentStop(benchmark::State& state) {
    for (auto _ : state) {
        auto ptr = std::make_shared<agent::AgentServer>();
        ptr->stop();
        benchmark::DoNotOptimize(ptr);
        ptr->destroy();
    }
}
BENCHMARK(BM_AgentStop);

static void BM_AgentIsDone(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    for (auto _ : state) {
        benchmark::DoNotOptimize(ptr->isDone());
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentIsDone);

static void BM_AgentIsDoneAfterStop(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    ptr->stop();
    for (auto _ : state) {
        benchmark::DoNotOptimize(ptr->isDone());
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentIsDoneAfterStop);

// ====== setOutput ======

static void BM_AgentSetOutput(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    for (auto _ : state) {
        ptr->setOutput([](void*, const char*) {}, nullptr);
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentSetOutput);

// ====== IAgent interface ======

static void BM_AgentOpenPlugin(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string dummy;
    ptr->setOutput([](void* udata, const char*) {}, &dummy);
    for (auto _ : state) {
        ptr->openPlugin("snake", "{}");
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentOpenPlugin);

static void BM_AgentOpenPluginNoOutput(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    for (auto _ : state) {
        ptr->openPlugin("snake", "{}");
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentOpenPluginNoOutput);

static void BM_AgentControlPlugin(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string dummy;
    ptr->setOutput([](void* udata, const char*) {}, &dummy);
    for (auto _ : state) {
        ptr->controlPlugin("snake", "{\"value\":3}");
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentControlPlugin);

static void BM_AgentClosePlugin(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string dummy;
    ptr->setOutput([](void* udata, const char*) {}, &dummy);
    for (auto _ : state) {
        ptr->closePlugin("snake");
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentClosePlugin);

// ====== onInput ======

static void BM_AgentOnInputStop(benchmark::State& state) {
    for (auto _ : state) {
        auto ptr = std::make_shared<agent::AgentServer>();
        std::string dummy;
        ptr->setOutput([](void* udata, const char*) {}, &dummy);
        ptr->onInput("{\"action\":\"stop\"}");
        ptr->destroy();
    }
}
BENCHMARK(BM_AgentOnInputStop);

static void BM_AgentOnInputEmptyObject(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string dummy;
    ptr->setOutput([](void* udata, const char*) {}, &dummy);
    for (auto _ : state) {
        ptr->onInput("{}");
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentOnInputEmptyObject);

static void BM_AgentOnInputInvalidJson(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    std::string dummy;
    ptr->setOutput([](void* udata, const char*) {}, &dummy);
    for (auto _ : state) {
        ptr->onInput("not json");
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentOnInputInvalidJson);

// ====== process ======

static void BM_AgentProcessWithCommand(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    for (auto _ : state) {
        auto result = ptr->process(R"({"text":"/help"})");
        benchmark::DoNotOptimize(result);
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentProcessWithCommand);

static void BM_AgentProcessWithText(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    for (auto _ : state) {
        auto result = ptr->process(R"({"text":"hello"})");
        benchmark::DoNotOptimize(result);
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentProcessWithText);

static void BM_AgentProcessWithEmptyJson(benchmark::State& state) {
    auto ptr = std::make_shared<agent::AgentServer>();
    for (auto _ : state) {
        auto result = ptr->process("");
        benchmark::DoNotOptimize(result);
    }
    ptr->destroy();
}
BENCHMARK(BM_AgentProcessWithEmptyJson);

// ====== concurrent ======

static void BM_AgentConcurrentCreateDestroy(benchmark::State& state) {
    int thread_count = state.range(0);
    for (auto _ : state) {
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([]() {
                auto ptr = std::make_shared<agent::AgentServer>();
                ptr->destroy();
            });
        }
        for (auto& t : threads) t.join();
    }
}
BENCHMARK(BM_AgentConcurrentCreateDestroy)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK_MAIN();
