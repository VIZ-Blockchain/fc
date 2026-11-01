

add_executable( api api.cpp )
target_link_libraries( api fc )

if( ECC_IMPL STREQUAL secp256k1 )
    add_executable( blind all_tests.cpp crypto/blind.cpp )
    target_link_libraries( blind fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )
endif()

add_executable( ntp_test all_tests.cpp network/ntp_test.cpp )
target_link_libraries( ntp_test fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )

add_executable( task_cancel_test all_tests.cpp thread/task_cancel.cpp )
target_link_libraries( task_cancel_test fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )


add_executable( bloom_test all_tests.cpp bloom_test.cpp )
target_link_libraries( bloom_test fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )


add_executable( hmac_test hmac_test.cpp )
target_link_libraries( hmac_test fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )

add_executable( ecc_test crypto/ecc_test.cpp )
target_link_libraries( ecc_test fc )

add_executable( log_test crypto/log_test.cpp )
target_link_libraries( log_test fc )

#add_executable( test_aes aes_test.cpp )
#target_link_libraries( test_aes fc ${rt_library} ${pthread_library} )
#add_executable( test_sleep sleep.cpp )
#target_link_libraries( test_sleep fc )

add_executable( all_tests all_tests.cpp
                          compress/compress.cpp
                          crypto/aes_test.cpp
                          crypto/base_n_tests.cpp
                          crypto/bigint_test.cpp
                          crypto/blind.cpp
                          crypto/sha_tests.cpp
                          network/ntp_test.cpp
                          network/http/websocket_test.cpp
                          thread/task_cancel.cpp
                          bloom_test.cpp
                          utf8_test.cpp
                          )
target_link_libraries( all_tests fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )
