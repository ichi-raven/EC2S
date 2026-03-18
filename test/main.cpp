#include <EC2S.hpp>

#include <iostream>
#include <chrono>
#include <random>

using namespace ec2s;

void test(std::pmr::memory_resource* const pMemoryResource = nullptr);

void heavyTask()
{
    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e5);
    ec2s::Registry registry;
    std::vector<ec2s::Entity> entities(kTestEntityNum);

    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        entities[i] = registry.create();
        registry.add<int>(entities[i], 1);
        if (i % 2)
        {
            registry.add<double>(entities[i], 0.3);
        }
        else
        {
            registry.add<char>(entities[i], 'a');
        }
    }

    registry.each<int>([](int& e) { e += 1; });
    registry.each<double>([](double& e) { e += 2.; });
    registry.each<char>([](char& e) { e += 1; });

    registry.each<int, double>(
        [](int& e, double& e2)
        {
            e += 1;
            e2 += 0.5;
        });

    registry.each<int, Exclude<double>>([]([[maybe_unused]] Entity entity, int& e) { e += 1; });

    // validation
    bool succeeded = true;

    registry.each<int, double>([&](int& e, double& e2) { succeeded &= (e == 3); });
    registry.each<int, char>([&](int& e, char& e2) { succeeded &= (e == 3); });

    registry.each<double>([&](double& e) { succeeded &= (e == 2.8); });
    registry.each<char>([&](char& e) { succeeded &= (e == 'b'); });

    // deletion test
    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        registry.destroy(entities[i]);
    }

    if (!succeeded)
    {
        std::cout << "failed at thread " << std::this_thread::get_id() << "\n";
    }
}

void loadTest()
{
    constexpr int kTestTime = static_cast<std::size_t>(1e3);
    ThreadPool threadPool;

    std::cout << "load test with " << threadPool.size() << " threads-----------------------\n";

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kTestTime; ++i)
    {
        threadPool.submit(heavyTask);
    }
    threadPool.wait();
    auto end = std::chrono::steady_clock::now();
    std::cout << "parallel time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < kTestTime; ++i)
    {
        heavyTask();
    }
    end = std::chrono::steady_clock::now();
    std::cout << "serial time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";
}

void parallelTest()
{
    ThreadPool threadPool;

    std::cout << "parallel test with " << threadPool.size() << " threads-----------------------\n";

    auto sleepRandomMs = []()
    {
        using namespace std::chrono_literals;

        static thread_local std::mt19937_64 rng{ std::random_device{}() };
        static thread_local std::uniform_int_distribution<int> dist(1, 30);

        std::this_thread::sleep_for(dist(rng) * 1ms);
    };

    // ordered parallel tasks with dependencies

    const auto checkTask1 = [&sleepRandomMs]()
    {
        std::cout << "called 1\n";
        sleepRandomMs();
    };
    const auto checkTask1a = [&sleepRandomMs]()
    {
        std::cout << "called 1a\n";
        sleepRandomMs();
    };
    const auto checkTask2 = [&sleepRandomMs]()
    {
        std::cout << "called 2\n";
        sleepRandomMs();
    };
    const auto checkTask2a = [&sleepRandomMs]()
    {
        std::cout << "called 2a\n";
        sleepRandomMs();
    };
    const auto checkTask2b = [&sleepRandomMs]()
    {
        std::cout << "called 2b\n";
        sleepRandomMs();
    };
    const auto checkTask3 = [&sleepRandomMs]()
    {
        std::cout << "called 3\n";
        sleepRandomMs();
    };
    const auto checkTask4 = [&sleepRandomMs]()
    {
        std::cout << "called 4\n";
        sleepRandomMs();
    };
    const auto checkTask5 = [&sleepRandomMs]()
    {
        std::cout << "called 5\n";
        sleepRandomMs();
    };
    const auto independentTask = []() { std::cout << "independent\n"; };

    auto& job1  = threadPool.createJob(checkTask1);
    auto& job1a = threadPool.createJob(checkTask1a);
    auto& job2  = threadPool.createJob(checkTask2);
    auto& job2a = threadPool.createJob(checkTask2a);
    auto& job2b = threadPool.createJob(checkTask2b);
    auto& job3  = threadPool.createJob(checkTask3);
    auto& job4  = threadPool.createJob(checkTask4);
    auto& job5  = threadPool.createJob(checkTask5);

    job1.addChild(std::move(job2));
    job1a.addChild(std::move(job2));
    job1.addChild(std::move(job2a));
    job1.addChild(std::move(job2b));
    job2.addChild(std::move(job3));
    job2a.addChild(std::move(job3));
    job3.addChild(std::move(job4));
    job4.addChild(std::move(job5));

    std::cout << "start submitting all jobs...\n";

    // submit in random order with independent tasks
    threadPool.submit(independentTask);
    threadPool.submit(std::move(job1));
    threadPool.submit(independentTask);
    sleepRandomMs();
    threadPool.submit(std::move(job1a));
    threadPool.submit(independentTask);
    threadPool.submit(independentTask);

    threadPool.wait();
    std::cout << "all jobs done\n";
}

void sortTest()
{
    std::cout << "sort test-----------------------\n";

    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e2);
    std::vector<Entity> entities(kTestEntityNum);

    std::vector<int> groundTruth(kTestEntityNum);
    std::iota(groundTruth.begin(), groundTruth.end(), 0);
    std::vector<int> shuffled = groundTruth;
    std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937_64{ std::random_device{}() });

    Registry registry;
    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        entities[i] = registry.create();
        registry.add<int>(entities[i], shuffled[i]);
    }

    registry.sort<int>([](const int& a, const int& b) { return a < b; });

    bool succeeded = true;

    // dense access check
    registry.each<int>(
        [&](const int& e)
        {
            static std::size_t index = 0;
            succeeded &= (e == groundTruth[index]);
            ++index;
        });

    // sparse access check
    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        const int& e = registry.get<int>(entities[i]);
        succeeded &= (e == static_cast<int>(i));

        if (!succeeded)
        {
            std::cout << "failed at entity " << entities[i] << "\n";
            std::cout << "expected " << static_cast<int>(i) << ", got " << e << "\n";
            break;
        }
    }

    if (succeeded)
    {
        std::cout << "sort test succeeded\n";
    }
    else
    {
        std::cout << "sort test failed\n";
    }
}

void groupTest()
{
    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e1);
    ec2s::Registry registry;
    std::vector<ec2s::Entity> entities(kTestEntityNum);

    std::cout << "group test-----------------------\n";

    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        entities[i] = registry.create();
        registry.add<int>(entities[i], 1);
        if (i % 2)
        {
            registry.add<double>(entities[i], 0.3);
        }
        else
        {
            registry.add<char>(entities[i], 'a');
        }
    }

    auto group = registry.group<int, double>();
    if (!group)
    {
        std::cout << "group creation failed!\n";
        return;
    }

    const auto added = registry.create<int, double>();
    registry.get<int>(added) += 1;
    registry.get<double>(added) += 0.3;

    group->each(
        [](int& e, double& e2)
        {
            e += 1;
            e2 += 2.;
        });

    // validation
    int index = 0;
    registry.each<int>([](const Entity entity, int& e) { std::cout << entity << " : " << e << "\n"; });
    std::cout << "\n";
    registry.each<double>([](const Entity entity, double& e) { std::cout << entity << " : " << e << "\n"; });
    std::cout << "\n";
    registry.each<int, char>([](const Entity entity, int& e, char& e2) { std::cout << entity << " : " << e << ", " << e2 << "\n"; });

    auto group2 = registry.group<int, char>();
    if (group2)
    {
        std::cout << "invalid! group creation succeeded\n";
    }
    else
    {
        std::cout << "valid! group creation failed\n";
    }
}

void groupPerformanceTest()
{
    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e5);
    constexpr int kTestTime              = static_cast<int>(1e3);
    ec2s::Registry registry;
    std::vector<ec2s::Entity> entities(kTestEntityNum);

    std::cout << "group performance test-----------------------\n";

    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        entities[i] = registry.create();
        registry.add<int>(entities[i], 1);
        if (i % 2)
        {
            registry.add<double>(entities[i], 0.3);
        }
        else
        {
            registry.add<char>(entities[i], 'a');
        }
    }

    // view
    {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kTestTime; ++i)
        {
            registry.each<int, double>(
                [](int& i, double& d)
                {
                    i += 1;
                    d += 0.3;
                });
        }
        const auto end = std::chrono::steady_clock::now();
        std::cout << "view time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() * 1e-3 << " ms\n";
    }

    // group
    {
        const auto start = std::chrono::steady_clock::now();
        auto group       = registry.group<int, double>();
        for (int i = 0; i < kTestTime; ++i)
        {
            group->each(
                [](int& i, double& d)
                {
                    i += 1;
                    d += 0.3;
                });
        }
        const auto end = std::chrono::steady_clock::now();
        std::cout << "group time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() * 1e-3 << " ms\n";
    }
}

void externalAllocatorTest()
{
    std::cout << "external allocator test-----------------------\n";

    constexpr size_t memSize = 1024 * 1024;  // 1 MB

    {
        std::cout << "new_delete_resource test-----------------------\n\n";
        test(std::pmr::new_delete_resource());
    }

    {
        std::cout << "monotonic_buffer_resource test-----------------------\n\n";
        std::pmr::monotonic_buffer_resource pool(memSize);  // 1 MB buffer
        test(&pool);
    }

    {
        std::cout << "TLSFAllocator test-----------------------\n\n";
        std::byte* pMainMemory = new std::byte[memSize];
        TLSFAllocator<> tlsfAllocator(pMainMemory, memSize);  // 1 MB pool
        TLSFMemoryResource<> tlsfPool(tlsfAllocator);
        test(&tlsfPool);
    }

    std::cout << "external allocator test completed\n";
}

void taggedPointerTest()
{
    std::cout << "tagged pointer test-----------------------\n";
    TaggedPointer taggedPtr;
    int value = 42;
    taggedPtr.setPointer(&value);
    taggedPtr.setTag(std::byte{ 0x1F });
    int* pValue   = taggedPtr.getPointer<int>();
    std::byte tag = taggedPtr.getTag();
    std::cout << "pointer value: " << *pValue << "\n";
    std::cout << "tag value: " << std::to_integer<int>(tag) << "\n";
}

void lockFreeQueueTest()
{
    std::cout << "lock-free queue test-----------------------\n";

    LockFreeQueue<int> queue;
    constexpr int kTestUnit    = 10;
    constexpr int kProducerNum = 10;
    constexpr int kConsumerNum = 10;
    std::vector<int> consumedValues;
    std::mutex consumedValuesMutex;

    // Producer threads
    std::vector<std::thread> producers;
    producers.resize(kProducerNum);
    for (int i = 0; i < kProducerNum; ++i)
    {
        producers[i] = std::thread(
            [&queue, i]()
            {
                for (int j = i * kTestUnit; j < (i + 1) * kTestUnit; ++j)
                {
                    queue.push(j);
                }
            });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    consumers.resize(kProducerNum);
    for (int i = 0; i < kConsumerNum; ++i)
    {
        consumers[i] = std::thread(
            [&queue, &consumedValues, &consumedValuesMutex]()
            {
                for (int j = 0; j < kTestUnit; ++j)
                {
                    int value;
                    while (!queue.pop(value))
                    {
                        // Queue is empty, wait and retry
                        std::this_thread::yield();
                    }

                    {
                        std::lock_guard<std::mutex> lock(consumedValuesMutex);
                        consumedValues.push_back(value);
                    }
                }
            });
    }

    // Wait for all producers and consumers to finish
    for (auto& producer : producers)
    {
        producer.join();
    }
    for (auto& consumer : consumers)
    {
        consumer.join();
    }

    // validation
    std::sort(consumedValues.begin(), consumedValues.end());
    for (int i = 0; i < kProducerNum * kTestUnit; ++i)
    {
        std::cout << consumedValues[i] << " ";
    }
    std::cout << "\n";

    bool succeeded = true;
    for (int i = 0; i < kProducerNum * kTestUnit; ++i)
    {
        if (consumedValues[i] != i)
        {
            succeeded = false;
            std::cout << "failed at index " << i << ": expected " << i << ", got " << consumedValues[i] << "\n";
            return;
        }
    }

    std::cout << "lock-free queue test completed\n";
}

void lockFreeQueueTest2()
{
    LockFreeQueue<int> queue;
    constexpr int producerCount = 4;
    constexpr int consumerCount = 4;
    constexpr int perProducer   = 100;

    std::atomic<int> popCount{ 0 };

    std::mt19937_64 eng{ std::random_device{}() };
    std::uniform_int_distribution<> dist{ 10, 100 };

    std::vector<std::thread> threads;

    for (int p = 0; p < producerCount; p++)
    {
        threads.emplace_back(
            [&queue, &dist, &eng]()
            {
                for (int i = 0; i < perProducer; i++)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds{ dist(eng) });
                    queue.push(i);
                }
            });
    }

    for (int c = 0; c < consumerCount; c++)
    {
        threads.emplace_back(
            [&queue, &popCount]()
            {
                int v;
                int local = 0;

                while (local < perProducer)
                {
                    if (queue.pop(v))
                    {
                        local++;
                        popCount++;
                    }
                }
            });
    }

    for (auto& th : threads)
    {
        th.join();
    }

    if (popCount.load() != producerCount * perProducer)
    {
        std::cout << "failed: expected pop count " << producerCount * perProducer << ", got " << popCount.load() << "\n";
    }
    else
    {
        std::cout << "lock-free queue multi-thread push/pop test succeeded\n";
    }
}

int main()
{
    //test();
    //loadTest();
    //parallelTest();
    //sortTest();
    //groupTest();
    //groupPerformanceTest();
    //externalAllocatorTest();
    //taggedPointerTest();
    lockFreeQueueTest2();

    return 0;
}
