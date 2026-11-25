function(check_fills sample latency lifetime size expected_row)
    execute_process(
        COMMAND "${REPLAY}" "${CMAKE_CURRENT_LIST_DIR}/${sample}"
                --simulate "${latency}" "${lifetime}" "${size}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE actual
        ERROR_VARIABLE error
    )
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "${sample} failed to replay: ${result} ${error}")
    endif()

    set(header "arrival_us,price,size,observed_us,status,same_price_shares,same_price_first_wait_us,fifo_shares,fifo_first_wait_us")
    set(expected "${header}\n${expected_row}\n")
    string(REPLACE "\r\n" "\n" actual "${actual}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${sample} at latency ${latency}\nExpected:\n${expected}Actual:\n${actual}")
    endif()
endfunction()

# The first 200-share execution and the following cancellation clear the
# queue ahead. Only the two later executions contribute to the FIFO fill.
check_fills(fill_sample.txt 0 100 100 "0,100,100,100,expired,100,10,70,30")

# Arrival uses the changed best bid. The execution at expiry is excluded.
check_fills(fill_delayed_sample.txt 10 20 200 "10,101,200,20,expired,150,2,0,")
