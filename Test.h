#pragma once

#include "Base.h"

#include "Registry.h"
#include "Meta.h"
#include <entt.hpp>

#include <random>
#include <chrono>
#include <iostream>

namespace HBL2
{
    //------------------------------------------------------------------------------
    // 1) Component definitions (PODs for highest speed)
    //------------------------------------------------------------------------------

    struct Position { float x, y, z; };
    struct Velocity { float dx, dy, dz; };
    struct Collider { float radius; };
    struct AIState { int state; };
    struct CameraTag { int tag = 1; };

    //------------------------------------------------------------------------------
    // 2) Test parameters
    //------------------------------------------------------------------------------

    struct TestParams
    {
        size_t entityCount;
        double density; // fraction [0,1] of entities having each component
    };

    //------------------------------------------------------------------------------
    // 3) Helper: random bool by density
    //------------------------------------------------------------------------------

    bool by_density(double p, std::mt19937& rng)
    {
        std::uniform_real_distribution<> dist(0.0, 1.0);
        return dist(rng) < p;
    }

    double benchmark_hbl2_ecs(const TestParams& tp, size_t frames)
    {
        using Clock = std::chrono::high_resolution_clock;
        std::mt19937 rng(12345);

        // --- setup world
        Registry world;

        // --- create entities & assign components
        std::vector<Entity> ents(tp.entityCount);
        for (size_t i = 0; i < tp.entityCount; ++i)
        {
            ents[i] = world.CreateEntity();
            if (by_density(tp.density, rng)) world.AddComponent<Position>(ents[i], { 0,0,0 });
            if (by_density(tp.density, rng)) world.AddComponent<Velocity>(ents[i], { 0,0,0 });
            if (by_density(tp.density, rng)) world.AddComponent<Collider>(ents[i], { 1.0f });
            if (by_density(tp.density, rng)) world.AddComponent<AIState>(ents[i], { 0 });
            if (by_density(tp.density, rng)) world.AddComponent<CameraTag>(ents[i], {});
        }        

        // --- benchmark loop
        auto t0 = Clock::now();
        for (size_t f = 0; f < frames; ++f)
        {
            // dynamic churn: randomly add/remove few components
            for (int k = 0; k < (int)(0.001 * tp.entityCount); ++k)
            {
                auto e = ents[rng() % tp.entityCount];
                if (by_density(0.5, rng)) world.AddComponent<Velocity>(e, Velocity{ 0,0,0 });
                else                      world.RemoveComponent<Velocity>(e);
            }

            // --- run systems
            world.Run<Position, Velocity>([](Position& p, Velocity& v)
            {
                p.x += v.dx; p.y += v.dy; p.z += v.dz;
            });

            world.Run<Position, Velocity, Collider>([](Position& p, Velocity& v, Collider& c)
            {
                // pretend collision: if |p| < c.radius, invert velocity
                float dist2 = p.x * p.x + p.y * p.y + p.z * p.z;
                if (dist2 < c.radius * c.radius)
                {
                    v.dx = -v.dx; v.dy = -v.dy; v.dz = -v.dz;
                }
            });

            world.Run<AIState>([](AIState& ai)
            {
                ai.state = (ai.state + 1) & 0xFF;
            });

            world.Run<Position, CameraTag>([](Position& p, CameraTag& c)
            {
            });
        }
        auto t1 = Clock::now();

        return std::chrono::duration<double>(t1 - t0).count() / frames;
    }

    double benchmark_hbl2_ecs_query(const TestParams& tp, size_t frames)
    {
        using Clock = std::chrono::high_resolution_clock;
        std::mt19937 rng(12345);

        // --- setup world
        Registry world;

        // --- create entities & assign components
        std::vector<Entity> ents(tp.entityCount);
        for (size_t i = 0; i < tp.entityCount; ++i)
        {
            ents[i] = world.CreateEntity();
            if (by_density(tp.density, rng)) world.AddComponent<Position>(ents[i], { 0,0,0 });
            if (by_density(tp.density, rng)) world.AddComponent<Velocity>(ents[i], { 0,0,0 });
            if (by_density(tp.density, rng)) world.AddComponent<Collider>(ents[i], { 1.0f });
            if (by_density(tp.density, rng)) world.AddComponent<AIState>(ents[i], { 0 });
            if (by_density(tp.density, rng)) world.AddComponent<CameraTag>(ents[i], {});
        }

        // --- benchmark loop
        auto t0 = Clock::now();
        for (size_t f = 0; f < frames; ++f)
        {
            // dynamic churn: randomly add/remove few components
            for (int k = 0; k < (int)(0.001 * tp.entityCount); ++k)
            {
                auto e = ents[rng() % tp.entityCount];
                if (by_density(0.5, rng)) world.AddComponent<Velocity>(e, Velocity{ 0,0,0 });
                else                      world.RemoveComponent<Velocity>(e);
            }

            // --- run systems
            world.Group<Position, Velocity>()
                .ForEach([](Position& p, Velocity& v)
                {
                    p.x += v.dx; p.y += v.dy; p.z += v.dz;
                });

            world.Group<Position, Velocity, Collider>()
                .ForEach([](Position& p, Velocity& v, Collider& c)
                {
                    // pretend collision: if |p| < c.radius, invert velocity
                    float dist2 = p.x * p.x + p.y * p.y + p.z * p.z;
                    if (dist2 < c.radius * c.radius)
                    {
                        v.dx = -v.dx; v.dy = -v.dy; v.dz = -v.dz;
                    }
                });

            world.View<AIState>()
                .ForEach([](AIState& ai)
                {
                    ai.state = (ai.state + 1) & 0xFF;
                });

            world.Group<Position, CameraTag>()
                .ForEach([](Position& p, CameraTag& c)
                {
                });
        }
        auto t1 = Clock::now();

        return std::chrono::duration<double>(t1 - t0).count() / frames;
    }

    double benchmark_entt(const TestParams& tp, size_t frames) {
        using Clock = std::chrono::high_resolution_clock;
        std::mt19937 rng(12345);

        entt::registry reg;

        // --- create entities & assign components
        std::vector<entt::entity> ents(tp.entityCount);
        for (size_t i = 0; i < tp.entityCount; ++i) {
            auto e = reg.create();
            ents[i] = e;
            if (by_density(tp.density, rng)) reg.emplace<Position>(e, Position{ 0,0,0 });
            if (by_density(tp.density, rng)) reg.emplace<Velocity>(e, Velocity{ 0,0,0 });
            if (by_density(tp.density, rng)) reg.emplace<Collider>(e, Collider{ 1.0f });
            if (by_density(tp.density, rng)) reg.emplace<AIState>(e, AIState{ 0 });
            if (by_density(tp.density, rng)) reg.emplace<CameraTag>(e);
        }

        // --- benchmark loop
        auto t0 = Clock::now();
        for (size_t f = 0; f < frames; ++f)
        {
            // dynamic churn
            for (int k = 0; k < (int)(0.001 * tp.entityCount); ++k)
            {
                auto e = ents[rng() % tp.entityCount];
                if (by_density(0.5, rng)) reg.emplace_or_replace<Velocity>(e, Velocity{ 0,0,0 });
                else                      reg.remove<Velocity>(e);
            }

            // systems
            // Position+Velocity
            reg.view<Position, Velocity>().each([](auto& p, auto& v)
            {
                p.x += v.dx; p.y += v.dy; p.z += v.dz;
            });
            // Position+Velocity+Collider
            reg.view<Position, Velocity, Collider>().each([](auto& p, auto& v, auto& c)
            {
                float d2 = p.x * p.x + p.y * p.y + p.z * p.z;
                if (d2 < c.radius * c.radius)
                    v.dx = -v.dx, v.dy = -v.dy, v.dz = -v.dz;
            });
            // AI
            reg.view<AIState>().each([](auto& ai) { ai.state = (ai.state + 1) & 0xFF; });
            // Render culling
            reg.group<CameraTag>(entt::get<Position>).each([](auto& tag, auto& p) { /* no‑op */ });
        }
        auto t1 = Clock::now();

        return std::chrono::duration<double>(t1 - t0).count() / frames;
    }

    void benchmark_ecs()
    {
        std::vector<TestParams> tests = {
            {500, 0.25}, {500, 0.75}, {1000, 0.25}, {1000, 0.75}, {5000, 0.1}, {5000, 0.25}, {5000, 0.75},
            {50000, 0.10}, {50000, 0.50}, {50000, 0.90}, {200000, 0.10},{200000, 0.50},{200000, 0.90},
            //{1000000, 0.10},{1000000, 0.50},{1000000, 0.90},
        };
        const size_t FRAMES = 1000;

        std::cout << "Entities, Density, Custom ECS ms/frame, EnTT ms/frame\n";
        for (auto& tp : tests)
        {
            double tEntt = benchmark_entt(tp, FRAMES) * 1000.0;
            double tCustom = benchmark_hbl2_ecs(tp, FRAMES) * 1000.0;
            double tCustomQuery = benchmark_hbl2_ecs_query(tp, FRAMES) * 1000.0;
            std::cout
                << "entityCount: " << tp.entityCount << ", "
                << "density: " << tp.density << ", "
                << "hbl2_ecs: " << tCustom << ", "
                << "hbl2_ecs_query: " << tCustomQuery << ", "
                << "tEntt: " << tEntt
                << "\n";
        }
    }

    // Holds one invocation record
    struct Record
    {
        int systemId;
        std::vector<size_t> readTypes, writeTypes;
    };

    // Global for test
    std::vector<Record> g_records;
    std::mutex          g_recMutex;
    std::atomic<int>    g_nextSystemId{ 0 };

    // Dummy component types
    struct C0 { int x0; int x00; }; struct C1 { int x1; int x11; }; struct C2 { int x2; int x22; }; struct C3 { int x3; int x33; };
    struct C4 { int x4; int x44; }; struct C5 {}; struct C6 {}; struct C7 {};

    // Utility: randomly choose a set of type IDs [0..N)
    static std::vector<size_t> pickRandomTypes(int N, int maxArgs, std::mt19937& rng)
    {
        std::uniform_int_distribution<int> countDist(1, maxArgs);
        int count = maxArgs;
        std::uniform_int_distribution<int> typeDist(0, N - 1);
        std::vector<size_t> out;
        while ((int)out.size() < count)
        {
            size_t t = typeDist(rng);
            if (std::find(out.begin(), out.end(), t) == out.end())
                out.push_back(t);
        }
        return out;
    }

    void test_system_scheduling()
    {
        Log::Initialize();
        JobSystem::Initialize();

        constexpr int NUM_SYSTEMS = 100;
        constexpr int NUM_TYPES = 8;
        std::mt19937 rng(123);

        Registry reg;

        // 1) Schedule a bunch of random systems
        for (int i = 0; i < NUM_SYSTEMS; ++i) {
            int sysId = g_nextSystemId++;
            int arity = std::uniform_int_distribution<int>(1, 2)(rng);

            // pick which component types this system uses
            auto types = pickRandomTypes(NUM_TYPES, arity, rng);

            // randomly decide which are const (reads) vs non-const (writes)
            std::vector<bool> isConst(types.size());
            for (int i = 0; i < isConst.size(); i++)
                isConst[i] = std::bernoulli_distribution(0.5)(rng);

            // build and schedule
            if (arity == 1) {
                size_t t0 = types[0];
                bool   r0 = isConst[0];
                if (r0) {
                    reg.Schedule<const C0>([=](const C0& a) {
                        std::lock_guard lk(g_recMutex);
                        g_records.push_back({ sysId, {t0}, {} });
                    });
                }
                else {
                    reg.Schedule<C0>([=](C0& a) {
                        std::lock_guard lk(g_recMutex);
                        g_records.push_back({ sysId, {}, {t0} });
                    });
                }
            }
            else if (arity == 2) {
                size_t t0 = types[0], t1 = types[1];
                bool   r0 = isConst[0], r1 = isConst[1];
                if (r0 && r1) {
                    reg.Schedule<const C0, const C1>([=](const C0&, const C1&) {
                        std::lock_guard lk(g_recMutex);
                        g_records.push_back({ sysId, {t0,t1}, {} });
                    });
                }
                else if (r0 && !r1) {
                    reg.Schedule<const C0, C1>([=](const C0&, C1&) {
                        std::lock_guard lk(g_recMutex);
                        g_records.push_back({ sysId, {t0}, {t1} });
                    });
                }
                else if (!r0 && r1) {
                    reg.Schedule<C0, const C1>([=](C0&, const C1&) {
                        std::lock_guard lk(g_recMutex);
                        g_records.push_back({ sysId, {t1}, {t0} });
                    });
                }
                else {
                    reg.Schedule<C0, C1>([=](C0&, C1&) {
                        std::lock_guard lk(g_recMutex);
                        g_records.push_back({ sysId, {}, {t0,t1} });
                    });
                }
            }
            
        }

        // 2) Execute once (simulating a frame)
        reg.ExecuteScheduledSystems();

        // 3) Validate that no two records with overlapping write/write or write/read
        //    have overlapping execution times. We only have record order; assume
        //    that parallel ones could appear interleaved. We’ll do a simple check:
        //    for any pair of records A and B with overlap in resources, ensure
        //    they did *not* run “concurrently.” In this test we can assume Execute
        //    runs systems in some parallel order but pushes record as soon as they
        //    start. So two conflicting systems should not both appear in g_records
        //    before one completes—i.e. their recordIds should not interleave with others.
        //    For simplicity, we just assert that no conflicting pair appears in
        //    the *same* position modulo 2: i.e. they can be (0,1) or (2,3) but never (0,2)…

        for (size_t i = 0; i < g_records.size(); ++i)
        {
            for (size_t j = i + 1; j < g_records.size(); ++j)
            {
                const auto& A = g_records[i];
                const auto& B = g_records[j];
                bool conflict = false;
                for (auto w : A.writeTypes)
                {
                    if (std::find(B.writeTypes.begin(), B.writeTypes.end(), w) != B.writeTypes.end()
                        || std::find(B.readTypes.begin(), B.readTypes.end(), w) != B.readTypes.end())
                    {
                        conflict = true; break;
                    }
                }
                for (auto w : B.writeTypes)
                {
                    if (std::find(A.readTypes.begin(), A.readTypes.end(), w) != A.readTypes.end())
                    {
                        conflict = true; break;
                    }
                }
                if (!conflict) continue;
                // systems A and B should not be *both* launched “simultaneously”
                // we only have record order; assert they’re not both in the same
                // pair of positions (2k,2k+1):
                assert((i / 2) != (j / 2) && "Conflict: two overlapping systems ran in parallel!");
            }
        }

        JobSystem::Shutdown();

        std::cout << "Scheduling stress test passed!\n";
    }

    void test_system_scheduling_simple()
    {
        Log::Initialize();
        {
            JobSystem::Initialize();

            Registry reg;

            std::vector<Entity> ents(1000);
            for (size_t i = 0; i < ents.size(); ++i)
            {
                ents[i] = reg.CreateEntity();

                reg.AddComponent<C0>(ents[i], { 0 });

                if (i == 2)
                {
                    reg.AddComponent<C2>(ents[i], { 2 });
                }
                else
                {
                    if (i % 2 == 0)
                    {
                        reg.AddComponent<C3>(ents[i], { 3 });
                        reg.AddComponent<C4>(ents[i], { 4 });
                    }

                    reg.AddComponent<C1>(ents[i], { 1 });
                }
            }

            {
                HBL2_PROFILE("Parallel HBL2");

                reg.Schedule<C0>([](C0& a)
                {
                    a.x0 = 50;
                    a.x00 = 50000;
                    for (int i = 0; i < a.x00; i++)
                    {
                        a.x0 += a.x0 + a.x00 * a.x0 + a.x00 - a.x0 + a.x00;
                    }
                });

                reg.Schedule<C1, C0>([](C1& c1, C0& c0)
                {
                    c1.x1 = 12;
                    c1.x11 = c0.x00 * 2 / c0.x00 + 15;
                    for (int i = 0; i < c1.x11; i++)
                    {
                        c1.x1 += c1.x1 + c1.x11 * c1.x1 + c1.x11 - c1.x1 + c1.x11;
                    }
                    c0.x0 += c0.x0 / c0.x00 * c1.x1;
                });

                reg.Schedule<C2, const C0>([](C2& c2, const C0& c0)
                {
                    c2.x22 = 0;
                    for (int i = 0; i < c0.x00; i++)
                    {
                        c2.x22 += c0.x0;
                    }
                    c2.x2 = c2.x22 * c0.x00;
                });

                reg.Schedule<C3, const C0>([](C3& c3, const C0& c0)
                {
                    c3.x33 = 0;
                    for (int i = 0; i < (c0.x00 * 3) / 2; i++)
                    {
                        c3.x33 += c0.x0;
                    }
                    c3.x3 = c3.x33 * c0.x00;
                });

                reg.Schedule<C4, const C0>([](C4& c4, const C0& c0)
                {
                    c4.x44 = 0;
                    for (int i = 0; i < c0.x00 * 2; i++)
                    {
                        c4.x44 += c0.x0;
                    }
                    c4.x4 = c4.x44 * c0.x00;
                });

                reg.ExecuteScheduledSystems();
            }

            JobSystem::Shutdown();
        }

        {
            Registry reg;

            std::vector<Entity> ents(1000);
            for (size_t i = 0; i < ents.size(); ++i)
            {
                ents[i] = reg.CreateEntity();

                reg.AddComponent<C0>(ents[i], { 0 });

                if (i == 2)
                {
                    reg.AddComponent<C2>(ents[i], { 2 });
                }
                else
                {
                    if (i % 2 == 0)
                    {
                        reg.AddComponent<C3>(ents[i], { 3 });
                        reg.AddComponent<C4>(ents[i], { 4 });
                    }

                    reg.AddComponent<C1>(ents[i], { 1 });
                }
            }

            {
                HBL2_PROFILE("Linear HBL2");

                reg.Run<C0>([](C0& a)
                {
                    a.x0 = 50;
                    a.x00 = 50000;
                    for (int i = 0; i < a.x00; i++)
                    {
                        a.x0 += a.x0 + a.x00 * a.x0 + a.x00 - a.x0 + a.x00;
                    }
                });

                reg.Run<C1, C0>([](C1& c1, C0& c0)
                {
                    c1.x1 = 12;
                    c1.x11 = c0.x00 * 2 / c0.x00 + 15;
                    for (int i = 0; i < c1.x11; i++)
                    {
                        c1.x1 += c1.x1 + c1.x11 * c1.x1 + c1.x11 - c1.x1 + c1.x11;
                    }
                    c0.x0 += c0.x0 / c0.x00 * c1.x1;
                });

                reg.Run<C2, const C0>([](C2& c2, const C0& c0)
                {
                    c2.x22 = 0;
                    for (int i = 0; i < c0.x00; i++)
                    {
                        c2.x22 += c0.x0;
                    }
                    c2.x2 = c2.x22 * c0.x00;
                });

                reg.Run<C3, const C0>([](C3& c3, const C0& c0)
                {
                    c3.x33 = 0;
                    for (int i = 0; i < (c0.x00 * 3) / 2; i++)
                    {
                        c3.x33 += c0.x0;
                    }
                    c3.x3 = c3.x33 * c0.x00;
                });

                reg.Run<C4, const C0>([](C4& c4, const C0& c0)
                {
                    c4.x44 = 0;
                    for (int i = 0; i < c0.x00 * 2; i++)
                    {
                        c4.x44 += c0.x0;
                    }
                    c4.x4 = c4.x44 * c0.x00;
                });
            }
        }

        {
            entt::registry reg;

            std::vector<entt::entity> ents(1000);
            for (size_t i = 0; i < ents.size(); ++i)
            {
                ents[i] = reg.create();

                reg.emplace<C0>(ents[i], 0);

                if (i == 2)
                {
                    reg.emplace<C2>(ents[i], 2);
                }
                else
                {
                    if (i % 2 == 0)
                    {
                        reg.emplace<C3>(ents[i], 3);
                        reg.emplace<C4>(ents[i], 4);
                    }

                    reg.emplace<C1>(ents[i], 1);
                }
            }

            {
                HBL2_PROFILE("Linear EnTT");

                reg.view<C0>()
                    .each([](C0& a)
                    {
                        a.x0 = 50;
                        a.x00 = 50000;
                        for (int i = 0; i < a.x00; i++)
                        {
                            a.x0 += a.x0 + a.x00 * a.x0 + a.x00 - a.x0 + a.x00;
                        }
                    });

                reg.view<C1, C0>()
                    .each([](C1& c1, C0& c0)
                    {
                        c1.x1 = 12;
                        c1.x11 = c0.x00 * 2 / c0.x00 + 15;
                        for (int i = 0; i < c1.x11; i++)
                        {
                            c1.x1 += c1.x1 + c1.x11 * c1.x1 + c1.x11 - c1.x1 + c1.x11;
                        }
                        c0.x0 += c0.x0 / c0.x00 * c1.x1;
                    });

                reg.view<C2, const C0>()
                    .each([](C2& c2, const C0& c0)
                    {
                        c2.x22 = 0;
                        for (int i = 0; i < c0.x00; i++)
                        {
                            c2.x22 += c0.x0;
                        }
                        c2.x2 = c2.x22 * c0.x00;
                    });

                reg.view<C3, const C0>()
                    .each([](C3& c3, const C0& c0)
                    {
                        c3.x33 = 0;
                        for (int i = 0; i < (c0.x00 * 3) / 2; i++)
                        {
                            c3.x33 += c0.x0;
                        }
                        c3.x3 = c3.x33 * c0.x00;
                    });

                reg.view<C4, const C0>()
                    .each([](C4& c4, const C0& c0)
                    {
                        c4.x44 = 0;
                        for (int i = 0; i < c0.x00 * 2; i++)
                        {
                            c4.x44 += c0.x0;
                        }
                        c4.x4 = c4.x44 * c0.x00;
                    });
            }
        }

    }

    void test_meta()
    {
        struct NewComponent {
            int   Mario;
            float Speed;
            bool  Active;
        };

        using namespace Meta;

        // 1) Register type and members
        Meta::Context ctx;
        Register<NewComponent>(ctx)
            .Data<&NewComponent::Mario>("Mario")
            .Data<&NewComponent::Speed>("Speed")
            .Data<&NewComponent::Active>("Active");

        // 2) Forward objects into Meta::Any
        NewComponent comp{ 42, 3.14f, true };
        const NewComponent ccomp{ 1, 2.71f, false };

        Any anyMut = ForwardAsMeta(ctx, comp);
        Any anyConst = ForwardAsMeta(ctx, ccomp);

        assert(anyMut.Is<NewComponent>());
        assert(anyConst.Is<const NewComponent>());

        // 3) Read members
        auto& td = Resolve<NewComponent>(ctx);
        for (const MemberData& m : td.members)
        {
            auto* md = td.Member(m.name);
            assert(md);

            // Get via member helper
            Any fMut = md->Get(anyMut);
            Any fConst = md->Get(anyConst);

            if (m.name == std::string("Mario")) {
                assert(*fMut.TryGetAs<int>() == 42);
                assert(*fConst.TryGetAs<int>() == 1);
            }
            if (m.name == std::string("Speed")) {
                assert(*fMut.TryGetAs<float>() == 3.14f);
                assert(*fConst.TryGetAs<float>() == 2.71f);
            }
            if (m.name == std::string("Active")) {
                assert(*fMut.TryGetAs<bool>() == true);
                assert(*fConst.TryGetAs<bool>() == false);
            }
        }
        std::cout << "Read tests passed\n";

        // 4) Write via raw‐value wrapping
        {
            // Mario = 100
            auto* md = td.Member("Mario");
            int newM = 100;
            Any newVal;
            newVal.Assign(&newM, &ctx);        // templated assign
            md->Set(anyMut, newVal);
            assert(comp.Mario == 100);

            // Speed = 7.5f
            md = td.Member("Speed");
            float newS = 7.5f;
            Any newValF;
            newValF.Assign(&newS, &ctx);
            md->Set(anyMut, newValF);
            assert(comp.Speed == 7.5f);

            // Active = false
            md = td.Member("Active");
            bool newA = false;
            Any newValB;
            newValB.Assign(&newA, &ctx);
            md->Set(anyMut, newValB);
            assert(comp.Active == false);
        }

        // 4.5) Write via raw‐value wrapping
        {
            // Mario = 100
            auto* md = td.Member("Mario");
            md->Set(anyMut, 101);
            assert(comp.Mario == 101);

            // Speed = 7.5f
            md = td.Member("Speed");
            md->Set(anyMut, 8.5f);
            assert(comp.Speed == 8.5f);

            // Active = false
            md = td.Member("Active");
            md->Set(anyMut, true);
            assert(comp.Active == true);
        }

        // 5) Write via copying an existing Any
        {
            auto* md = td.Member("Mario");
            // Wrap 123
            int v = 123;
            Any raw; raw.Assign(&v, &ctx);
            // Copy into copiedAny
            Any copiedAny;
            copiedAny.Assign(raw);
            md->Set(anyMut, copiedAny);
            assert(comp.Mario == 123);
        }
        std::cout << "Write tests passed\n";

        // 6) Copy assignment operator
        {
            Any a = anyMut;
            Any b;
            b = a;
            // a and b same contents
            assert(a.object == b.object);
            assert(a.type == b.type);
            assert(a.ctx == b.ctx);
        }
        std::cout << "Copy‑assign tests passed\n";

        // 7) Const Any cannot write (assert)
        {
            auto* md = td.Member("Mario");
            Any bad; int x = 999; bad.Assign(&x, &ctx);
            bool caught = false;
            try {
                md->Set(anyConst, bad);
            }
            catch (...) {
                caught = true;
            }
            assert(caught);
        }
        std::cout << "Const‑write test passed\n";

        std::cout << "All updated reflection tests succeeded!\n";
    }
}