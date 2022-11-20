#include "include/EC2S.hpp"

#include <chrono>

struct A
{
    A()
    {
        a = 0;
    }

    A(int _a)
    {
        a = _a;
    }

    int a;
};

struct B
{
    B()
    {
        b = 0;
    }

    B(double _b)
    {
        b = _b;
    }

    double b;
};

struct C
{
    C()
    {
        c = 0;
    }

    C(char _c)
    {
        c = _c;
    }

    char c;
};

void test()
{
    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e5);

    std::chrono::high_resolution_clock::time_point start, end;
    double elapsed = 0;

    ec2s::Registry registry;

    std::vector<ec2s::Entity> entities(kTestEntityNum);

    std::cout << "test entity num : " << kTestEntityNum << "\n";
    std::cout << "test start-----------------------------\n\n";

    {
        std::cout << "create and emplace : \n";
        start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < kTestEntityNum; ++i)
        {
            entities[i] = registry.create();
            registry.add<A>(entities[i], 1);
            if (i % 2)
            {
                registry.add<B>(entities[i], 0.3);
            }
            else
            {
                registry.add<C>(entities[i], 'a');
            }
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()); //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n\n";
    }

    // composition of component(進次郎)
    // Entity 0 : A, C
    // Entity 1 : A, B
    // Entity 2 : A, C
    // Entity 3 : A, B
    // ...

    {
        std::cout << "each single component : \n";
        start = std::chrono::high_resolution_clock::now();

        registry.each<A>([](A& e) {e.a += 1; });
        registry.each<B>([](B& e) {e.b += 2.; });
        registry.each<C>([](C& e) {e.c += 1; });

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.; //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    {
        std::cout << "check : ";
        bool succeeded = true;

        registry.each<A>([&](A e) {if (e.a != 2)   succeeded = false; });
        registry.each<B>([&](B e) {if (e.b != 2.3) succeeded = false; });
        registry.each<C>([&](C e) {if (e.c != 'b') succeeded = false; });
        if (succeeded)
        {
            std::cout << "OK\n\n";
        }
        else
        {
            std::cout << "NG\n\n";
        }
    }

    {
        std::cout << "get Component A from all entities : \n";
        start = std::chrono::high_resolution_clock::now();

        for (auto&& entity : entities)
        {
            registry.get<A>(entity).a -= 1;
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()); //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    {
        std::cout << "check : ";
        bool succeeded = true;

        registry.each<A>([&](A e) {if (e.a != 1)   succeeded = false; });
        if (succeeded)
        {
            std::cout << "OK\n\n";
        }
        else
        {
            std::cout << "NG\n\n";
        }
    }

    {
        std::cout << "get view (A, C) and each : \n";
        start = std::chrono::high_resolution_clock::now();
        auto view = registry.view<A, C>();

        //std::cout << "Component A, C (by view) : \n";
        view.each([](A& e1, C& e2) {e1.a += static_cast<int>(e2.c); });

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.; //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    {
        std::cout << "check : ";
        bool succeeded = true;
        auto view = registry.view<A, C>();

        //view.each<A>([&](A e){if (e.a != 1 + static_cast<int>('b')){std::cerr << e.a << "\n"; succeeded = false; }});
        if (succeeded)
        {
            std::cout << "OK\n\n";
        }
        else
        {
            std::cout << "NG\n\n";
        }
    }

    {
        std::cout << "destroy : \n";
        start = std::chrono::high_resolution_clock::now();
        for (const auto& entity : entities)
        {
            registry.destroy(entity);
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()); //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    entities.clear();
    entities.resize(kTestEntityNum);
    std::cout << "\n\nsecond time-----------------\n\n";

    {
        std::cout << "create and emplace : \n";
        start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < kTestEntityNum; ++i)
        {
            entities[i] = registry.create();
            registry.add<A>(entities[i], 1);
            if (i % 2)
            {
                registry.add<B>(entities[i], 0.3);
            }
            else
            {
                registry.add<C>(entities[i], 'a');
            }
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()); //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n\n";
    }

    // composition of component(進次郎)
    // Entity 0 : A, C
    // Entity 1 : A, B
    // Entity 2 : A, C
    // Entity 3 : A, B
    // ...

    {
        std::cout << "each single component : \n";
        start = std::chrono::high_resolution_clock::now();

        //std::cout << "Component A each : \n";
        registry.each<A>([](A& e) {e.a += 1; });
        //std::cout << "Component B each : \n";
        registry.each<B>([](B& e) {e.b += 2.; });
        //std::cout << "Component C each : \n";
        registry.each<C>([](C& e) {e.c += 1; });

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.; //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    {
        std::cout << "check : ";
        bool succeeded = true;

        registry.each<A>([&](A e) {if (e.a != 2)   succeeded = false; });
        registry.each<B>([&](B e) {if (e.b != 2.3) succeeded = false; });
        registry.each<C>([&](C e) {if (e.c != 'b') succeeded = false; });
        if (succeeded)
        {
            std::cout << "OK\n\n";
        }
        else
        {
            std::cout << "NG\n\n";
        }
    }

    {
        std::cout << "get Component A from all entities : \n";
        start = std::chrono::high_resolution_clock::now();

        for (auto&& entity : entities)
        {
            registry.get<A>(entity).a -= 1;
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()); //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    {
        std::cout << "check : ";
        bool succeeded = true;

        registry.each<A>([&](A e) {if (e.a != 1) { std::cerr << "missed val : " << e.a << "\n";  succeeded = false; } });
        if (succeeded)
        {
            std::cout << "OK\n\n";
        }
        else
        {
            std::cout << "NG\n\n";
        }
    }

    {
        std::cout << "get view (A, C) and each : \n";
        start = std::chrono::high_resolution_clock::now();
        auto view = registry.view<A, C>();

        //std::cout << "Component A, C (by view) : \n";
        view.each([](A& e1, C& e2) {e1.a += static_cast<int>(e2.c); });

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.; //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    {
        std::cout << "check : ";
        bool succeeded = true;
        auto view = registry.view<A, C>();

        view.each<A>([&](A e) {if (e.a != 1 + static_cast<int>('b')) { std::cerr << e.a << "\n"; succeeded = false; }});
        if (succeeded)
        {
            std::cout << "OK\n\n";
        }
        else
        {
            std::cout << "NG\n\n";
        }
    }

    {
        std::cout << "destroy : \n";
        start = std::chrono::high_resolution_clock::now();
        for (const auto& entity : entities)
        {
            registry.destroy(entity);
        }

        end = std::chrono::high_resolution_clock::now();
        elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()); //処理に要した時間をミリ秒に変換
        std::cout << "time : " << elapsed << "[ms]\n";
    }

    std::cout << "all test end successfully----------------\n";
}
