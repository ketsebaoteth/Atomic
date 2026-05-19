add_executable(sandbox main.cpp)


add_dependencies(sandbox AtomicShaders)

target_link_libraries(sandbox PRIVATE atomic_lib)
