function(mini_redis_enable_sanitizers target_name)
  if(NOT MINI_REDIS_ENABLE_SANITIZERS)
    return()
  endif()

  if(MSVC)
    message(WARNING "The sanitizer preset is intended for GCC or Clang on Linux")
    return()
  endif()

  target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
endfunction()
