


set(DCKP_TESTS
    test_parser_compact
    test_parser_ampl
    test_index_normalization
    test_validator_invalid
    test_solution_comparison
    test_greedy_max_profit
    test_vnd
    test_ils
)

foreach(test_name IN LISTS DCKP_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE dckp_core)
    dckp_apply_project_flags(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 30
        LABELS "dckp"
    )
endforeach()
