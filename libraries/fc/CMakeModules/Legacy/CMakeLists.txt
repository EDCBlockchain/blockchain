# If you are running recent enough version of CMake you shall consider updating/removing these lines ...
MESSAGE(STATUS "Configuring build scripts for older CMake versions")

# This patches your current boost searching routine so that `pthread` library is included if needed
#  - this is here to fix some older CMake + Boost configurations
set(Boost_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Boost" PARENT_SCOPE)

if (APPLE OR USE_LEGACY_FIND_BOOST)
    MESSAGE(STATUS "Configuring for custom FindBoost.cmake")
    # Custom FindBoost.cmake
    #  - overrides `find_package( Boost )` functionality
    #  - it's a modified https://github.com/Kitware/CMake/blob/363825cd55595b7de62fcf610836f6156a9f1a31/Modules/FindBoost.cmake
    #    - adds additional Boost search path
    #    - adds pthread as an dependency if needed
    ######
    # This is using an old version of FindBoost.cmake irregardless of your CMake version
    #  - this will be removed in future release
    #  - improves Boost library search in some Apple OS configrations
    #    - this can be fixed with properly setting BOOST_LIBRARYDIR
    #####

    MESSAGE(DEPRECATION "Custom FindBoost.cmake will be deprecated in some future release. If you are using some of the affected configurations try setting the BOOST_LIBRARYDIR variable.")

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Boost")

    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
endif ()
