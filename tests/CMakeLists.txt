


set(DCKP_TESTS
    test_parser_compact
    test_parser_ampl
    test_index_normalization
    test_validator_invalid
    test_solution_comparison
    test_greedy_max_profit
    test_vnd
    test_ils
    test_vns
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

add_test(NAME test_cli_help COMMAND dckp_sbpo --help)
set_tests_properties(test_cli_help PROPERTIES
    PASS_REGULAR_EXPRESSION "Usage: dckp_sbpo"
    TIMEOUT 5
    LABELS "dckp;cli"
)

add_test(NAME test_cli_rejects_zero_time
    COMMAND dckp_sbpo unused-instance --time-limit 0)
add_test(NAME test_cli_rejects_trailing_time
    COMMAND dckp_sbpo unused-instance --time-limit 10ms)
add_test(NAME test_cli_rejects_negative_seed
    COMMAND dckp_sbpo unused-instance --seed -1)
add_test(NAME test_cli_rejects_unknown_algorithm
    COMMAND dckp_sbpo unused-instance --algo unknown)
add_test(NAME test_cli_rejects_missing_value
    COMMAND dckp_sbpo unused-instance --seed)
add_test(NAME test_cli_rejects_extra_argument
    COMMAND dckp_sbpo first-instance second-instance)
add_test(NAME test_script_rejects_zero_time
    COMMAND bash ${PROJECT_SOURCE_DIR}/scripts/run_experiments.sh --time-limit 0)
add_test(NAME test_script_rejects_missing_value
    COMMAND bash ${PROJECT_SOURCE_DIR}/scripts/run_experiments.sh --algo)
add_test(NAME test_script_rejects_huge_runs
    COMMAND bash ${PROJECT_SOURCE_DIR}/scripts/run_experiments.sh --runs 1000000000)

set_tests_properties(
    test_cli_rejects_zero_time
    test_cli_rejects_trailing_time
    test_cli_rejects_negative_seed
    test_cli_rejects_unknown_algorithm
    test_cli_rejects_missing_value
    test_cli_rejects_extra_argument
    test_script_rejects_zero_time
    test_script_rejects_missing_value
    test_script_rejects_huge_runs
    PROPERTIES
        WILL_FAIL TRUE
        TIMEOUT 5
        LABELS "dckp;cli"
)
