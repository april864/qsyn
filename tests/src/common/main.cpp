#include <catch2/catch_session.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::off);
    return Catch::Session().run(argc, argv);
}
