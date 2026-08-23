


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
add_test(NAME test_cli_rejects_invalid_ils_perturbation
    COMMAND dckp_sbpo unused-instance --ils-perturbation-strength 9)
add_test(NAME test_cli_rejects_invalid_vnd_flag
    COMMAND dckp_sbpo unused-instance --vnd-add 2)
add_test(NAME test_cli_rejects_empty_vnd
    COMMAND dckp_sbpo unused-instance --vnd-add 0 --vnd-swap-1-1 0 --vnd-swap-2-1 0 --vnd-swap-1-2 0)
add_test(NAME test_cli_rejects_extra_argument
    COMMAND dckp_sbpo first-instance second-instance)
add_test(NAME test_script_rejects_zero_time
    COMMAND bash ${PROJECT_SOURCE_DIR}/scripts/run_experiments.sh --time-limit 0)
add_test(NAME test_script_rejects_missing_value
    COMMAND bash ${PROJECT_SOURCE_DIR}/scripts/run_experiments.sh --algo)
add_test(NAME test_script_rejects_huge_runs
    COMMAND bash ${PROJECT_SOURCE_DIR}/scripts/run_experiments.sh --runs 1000000000)
add_test(NAME test_ils_cli_tuning_defaults
    COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/test_ils_cli_tuning_defaults.sh
            $<TARGET_FILE:dckp_sbpo>
            ${PROJECT_SOURCE_DIR}/DCKP-instances/DCKP-instances-set-I-100/I1-I10/1I1)
add_test(NAME test_irace_target_runner
    COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/test_irace_target_runner.sh
            ${PROJECT_SOURCE_DIR}
            ${PROJECT_SOURCE_DIR}/DCKP-instances/DCKP-instances-set-I-100/I1-I10/1I1)
add_test(NAME test_irace_scenario
    COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/test_irace_scenario.sh
            ${PROJECT_SOURCE_DIR})
add_test(NAME test_irace_analysis
    COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/test_irace_analysis.sh
            ${PROJECT_SOURCE_DIR})

set_tests_properties(
    test_cli_rejects_zero_time
    test_cli_rejects_trailing_time
    test_cli_rejects_negative_seed
    test_cli_rejects_unknown_algorithm
    test_cli_rejects_missing_value
    test_cli_rejects_invalid_ils_perturbation
    test_cli_rejects_invalid_vnd_flag
    test_cli_rejects_empty_vnd
    test_cli_rejects_extra_argument
    test_script_rejects_zero_time
    test_script_rejects_missing_value
    test_script_rejects_huge_runs
    PROPERTIES
        WILL_FAIL TRUE
        TIMEOUT 5
        LABELS "dckp;cli"
)

set_tests_properties(
    test_ils_cli_tuning_defaults
    test_irace_target_runner
    test_irace_scenario
    test_irace_analysis
    PROPERTIES
        TIMEOUT 30
        LABELS "dckp;cli;irace"
)
set_tests_properties(test_irace_scenario PROPERTIES SKIP_RETURN_CODE 77)
