# EC2S
my tutorial implementation of header-only Sparse set-based Entity Component System(ECS)

thanks to "ECS back and forth" by skypjack
https://skypjack.github.io/2019-02-14-ecs-baf-part-1/

# Header-only usage

Add `include` to your compiler's include paths. Running EC2S's CMake is not required.

moodycamel ConcurrentQueue v1.0.5 is bundled in `include/third_party/concurrentqueue`
(commit `9afb99746f0f5fc94ac8aef737053ae0481ba8d1`).
Its license is included in the same directory.

