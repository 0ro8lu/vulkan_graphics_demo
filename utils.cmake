function(target_treat_all_warnings_as_errors Target)
	# Treat warnings as errors
	set_target_properties(${Target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

	# Turn all warnings on
	if (MSVC)
		target_compile_options(${Target} PRIVATE /W4)
	else()
		target_compile_options(${Target} PRIVATE -Wall -Wextra -pedantic)
	endif()

	if(XCODE)
    set_target_properties(App PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
	endif()
	
endfunction()
