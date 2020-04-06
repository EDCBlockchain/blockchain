file(GLOB HEADERS "include/graphene/protocol/*.hpp")

list(APPEND SOURCES account.cpp
                        types.cpp
                        address.cpp
                        authority.cpp
                        asset.cpp
                        assert.cpp
                        account.cpp
                        transfer.cpp
                        committee_member.cpp
                        witness.cpp
                        market.cpp
                        proposal.cpp
                        withdraw_permission.cpp
                        asset_ops.cpp
                        fund_ops.cpp
                        cheque_ops.cpp
                        memo.cpp
                        worker.cpp
                        custom.cpp
                        operations.cpp
                        transaction.cpp
                        block.cpp
                        chain_parameters.cpp
                        fee_schedule.cpp
                        confidential.cpp
                        vote.cpp
                        small_ops.cpp
                        pts_address.cpp
                        special_authority.cpp)

add_library( graphene_protocol ${SOURCES} ${HEADERS} )
target_link_libraries( graphene_protocol fc )
target_include_directories( graphene_protocol PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   graphene_protocol

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/graphene/protocol" )
