add_executable(example1 example1)
target_link_libraries(example1 icon6)

add_executable(example6 example6)
target_link_libraries(example6 icon6)


add_executable(echo_server echo_server)
target_link_libraries(echo_server icon6)

add_executable(echo_client echo_client)
target_link_libraries(echo_client icon6)


add_executable(chat_server chat_server)
target_link_libraries(chat_server icon6)

add_executable(chat_client chat_client)
target_link_libraries(chat_client icon6)
