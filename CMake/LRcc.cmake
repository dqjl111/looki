cmake_minimum_required(VERSION 3.5)

function(lrcc_add_resources target_name)
  set(options ADD_INTO_IDE)
  set(oneValueArgs "")
  set(multiValueArgs "")
  cmake_parse_arguments(ARGUMENTS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
  #
  set(inputs_lrcc_filepath ${ARGUMENTS_UNPARSED_ARGUMENTS})
  ##
  if(NOT TARGET ${target_name})    
    message(AUTHOR_WARNING "Cannot find compile target: ${target_name}\n")
    return()
  endif()

  #string(MD5 lrcc_inventory ${target_name})

  set(warn_msg)
  if(NOT EXISTS ${LRCC_GENER})
    if(NOT TARGET lrcc)
      set(warn_msg "${warn_msg} Cannot find resource generator: ${LRCC_GENER}\n")
    endif()
  endif()

  if(NOT inputs_lrcc_filepath OR inputs_lrcc_filepath EQUAL "")
    set(warn_msg "${warn_msg}  Need resource of target: ${target_name} \n")
  endif()

  if(warn_msg)
    message(AUTHOR_WARNING "${warn_msg}\n")
    return()
  endif()

  #
  set(warn_msg_not_founds_inputs_lrccs)
  foreach(input_file_name ${inputs_lrcc_filepath})
    if(NOT EXISTS ${input_file_name})
      set(warn_msg_not_founds_inputs_lrccs ${warn_msg_not_founds_inputs_lrccs} ${input_file_name})
    endif()
  endforeach()
  if(warn_msg_not_founds_inputs_lrccs)
    set(warn_msg "${warn_msg} Cannot find <inputs_lrcc_filepath> : ${warn_msg_not_founds_inputs_lrccs}\n")
  endif()
  #
  if(warn_msg)
    message(AUTHOR_WARNING "${warn_msg}\n")
    return()
  endif()

  foreach(input_file_name ${inputs_lrcc_filepath})
    get_filename_component(file_base_name ${input_file_name} NAME_WE)
    set(output_file_name "${CMAKE_CURRENT_BINARY_DIR}/lrcc_files/${file_base_name}.cpp")

    file(GENERATE OUTPUT ${output_file_name} CONTENT "")

    add_custom_target(
      "${target_name}_${file_base_name}" ALL
      COMMAND "${LRCC_GENER}" "-i" "${input_file_name}" "-o" "${output_file_name}"
      COMMENT "Create resource cpp: ${output_file_name}"
      VERBATIM
    )

    if(TARGET lrcc)
      add_dependencies("${target_name}_${file_base_name}" lrcc)
    endif()

    add_dependencies(${target_name} "${target_name}_${file_base_name}")
    target_sources(${target_name} PRIVATE ${output_file_name})
  endforeach()

endfunction()
