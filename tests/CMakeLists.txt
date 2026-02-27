add_executable(bible_tests
    test_strongs_parser.cpp
)

target_link_libraries(bible_tests PRIVATE bible::bible)

add_test(NAME bible_tests COMMAND bible_tests)
