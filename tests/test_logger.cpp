#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "core/logger.hpp"

TEST_CASE("Logger captures stderr") {
    Logger logger;

    logger.start();

    fprintf(stderr, "hello stderr\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto view = logger.getBufferView();

    REQUIRE(view.find("hello stderr") != std::string_view::npos);

    logger.stop();
}