add_executable(TestExecutable main.cpp test.cpp)

target_link_libraries(TestExecutable
    PRIVATE
    ${EC2S_LIB_NAME}
)
