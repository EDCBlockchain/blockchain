file(GLOB HEADERS "include/graphene/history/*.hpp")

add_library( graphene_history 
             history_plugin.cpp
           )

target_link_libraries( graphene_history graphene_chain graphene_app )
target_include_directories( graphene_history
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( history_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_history

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
