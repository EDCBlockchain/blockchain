#pragma once 

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/memo.hpp>
#include <iostream>

namespace graphene { namespace chain {

using namespace protocol;

class child_balance {
public:
    account_id_type account_id;
    uint64_t balance;
    uint32_t level;
    child_balance(): account_id(account_id_type()),balance(0), level(0) {};
    child_balance(account_id_type acc_id, uint64_t bal, uint32_t lvl): account_id(acc_id), balance(bal), level(lvl) {};
};

class leaf_info
{
public:
    typedef account_id_type acc_id_type;
    account_id_type account_id = account_id_type();
    int64_t balance = 0;
    uint32_t level_1_partners = 0;
    uint64_t level_1_sum = 0;
    uint32_t level_2_partners = 0;
    uint32_t all_partners = 0;
    uint64_t all_sum = 0; // sum of all maturing child balances
    double bonus_percent = 0;
    int64_t mature_balance = 0;
    string rank;
    std::vector<child_balance> child_balances;
    leaf_info( account_id_type acc_id, int64_t bal, int64_t m_bal = 0 ): account_id(acc_id), balance(bal), mature_balance(m_bal) {};
    leaf_info( account_id_type acc_id, int64_t bal,
               uint32_t level_1_partners, uint64_t level_1_sum, 
               uint32_t level_2_partners, 
               uint32_t all_partners, uint64_t all_sum, 
               double bonus_percent, int64_t m_bal = 0) :
        account_id(acc_id), balance(bal),
        level_1_partners(level_1_partners), level_1_sum(level_1_sum), 
        level_2_partners(level_2_partners), 
        all_partners(all_partners), all_sum(all_sum), 
        bonus_percent(bonus_percent), mature_balance(m_bal) {};
    leaf_info() {};
    int64_t get_bonus_value();
    void add_child_balance_old(account_id_type account_id, int64_t balance, uint32_t level);
    void add_child_balance(account_id_type account_id, int64_t balance, int64_t mat_balance, uint32_t level);
    std::vector<child_balance> get_child_balances_old();
    std::vector<child_balance> get_child_balances();
    bool operator ==(leaf_info& other)
    {
        return this->account_id == other.account_id &&
            this->all_partners == other.all_partners &&
            this->all_sum == other.all_sum &&
            this->balance == other.balance &&
            this->bonus_percent == other.bonus_percent &&
            this->level_1_partners == other.level_1_partners &&
            this->level_1_sum == other.level_1_sum &&
            this->level_2_partners == other.level_2_partners &&
            this->mature_balance == other.mature_balance;
    }
};

struct leaf_info2
{
   account_id_type account_id;
   bool referral_payments_enabled = true;
   std::string name;
   uint16_t level = 0;
   share_type daily_deposits;
   share_type active_deposits;
   share_type active_deposits_sum;
   uint32_t active_deposits_count = 0;
   uint32_t active_deposits_count_sum = 0;
   share_type level1_payment;
   share_type level2_payment;
   share_type level3_payment;
   uint32_t level1_valid_referrals_count = 0;
   uint32_t level2_valid_referrals_count = 0;
   uint32_t level3_valid_referrals_count = 0;
   fc::time_point_sec nearest_return_datetime;
};


} } // namespace graphene::chain

FC_REFLECT( graphene::chain::child_balance,
            (account_id)(balance)(level) )

FC_REFLECT( graphene::chain::leaf_info,
            (account_id)(rank)(balance)(level_1_partners)(level_1_sum)(level_2_partners)(all_partners)(all_sum)(bonus_percent) )