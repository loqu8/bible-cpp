add_executable(bible_tests
    test_strongs_parser.cpp
)
target_link_libraries(bible_tests PRIVATE bible::bible)
add_test(NAME bible_tests COMMAND bible_tests)

add_executable(engine_tests
    test_engine.cpp
)
target_link_libraries(engine_tests PRIVATE bible::bible ${BIBLE_SQLITE_TARGET})
add_test(NAME engine_tests COMMAND engine_tests)
