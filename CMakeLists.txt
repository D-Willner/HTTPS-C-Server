CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

PROJECT(server)

FIND_PACKAGE(Threads REQUIRED)

ADD_EXECUTABLE(server)

TARGET_LINK_LIBRARIES(server PRIVATE Threads::Threads)

TARGET_SOURCES(server
PRIVATE
    main.c 
    http.c
    server.c 
    test/test_http.c
)

INCLUDE(CTest)
IF(BUILD_TESTING)
    ADD_EXECUTABLE(test_http test/test_main.c test/test_http.c http.c)
    TARGET_COMPILE_FEATURES(test_http PRIVATE c_std_99)

    FOREACH(case IN ITEMS
        get_defaults get_uri post_json host user_agent keep_alive close
        numeric_cookie post_text extra_fields no_space_after_colon
        unrecognized_values unrecognized_connection body_is_not_headers
    )
        ADD_TEST(NAME http_parse_request.${case} COMMAND test_http ${case})
        SET_TESTS_PROPERTIES(http_parse_request.${case} PROPERTIES TIMEOUT 5)
    ENDFOREACH()

    FOREACH(case IN ITEMS no_content ok_fields repeated_fields version_last)
        ADD_TEST(NAME http_parse_response.${case} COMMAND test_http response ${case})
        SET_TESTS_PROPERTIES(http_parse_response.${case} PROPERTIES TIMEOUT 5)
    ENDFOREACH()

    FOREACH(case IN ITEMS get_without_fields post_with_fields)
        ADD_TEST(NAME http_create_request.${case} COMMAND test_http create ${case})
        SET_TESTS_PROPERTIES(http_create_request.${case} PROPERTIES TIMEOUT 5)
    ENDFOREACH()

    FOREACH(case IN ITEMS no_content_without_fields not_found_with_empty_value created_with_repeated_fields)
        ADD_TEST(NAME http_create_response.${case} COMMAND test_http create ${case})
        SET_TESTS_PROPERTIES(http_create_response.${case} PROPERTIES TIMEOUT 5)
    ENDFOREACH()
ENDIF()
