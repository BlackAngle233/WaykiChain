#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#include "config/errorcode.h"
#include "wasm/wasm_native_contract.hpp"
#include "wasm/exceptions.hpp"
#include "wasm/datastream.hpp"
#include "wasm/types/asset.hpp"
#include "persistence/cachewrapper.h"
#include "wasm/wasm_log.hpp"
#include "wasm/wasm_native_contract_abi.hpp"
#include "wasm/abi_def.hpp"
#include "wasm/abi_serializer.hpp"

using namespace std;
using namespace wasm;

namespace wasm {

    void wasmio_native_setcode(wasm_context &context) {

         WASM_ASSERT( context._receiver == wasmio, 
                      wasm_assert_exception, 
                      "wasmio_native_setcode.setcode, Except contract wasmio, But get '%s'", 
                      wasm::name(context._receiver).to_string());

        auto &database_account         = context.database.accountCache;
        auto &database_contract        = context.database.contractCache;
        auto &control_trx              = context.control_trx;

        set_code_data_type set_code_data = wasm::unpack<std::tuple<uint64_t, string, string, string>>(context.trx.data);
        auto contract_name               = std::get<0>(set_code_data);
        auto code                        = std::get<1>(set_code_data);
        auto abi                         = std::get<2>(set_code_data);
        auto memo                        = std::get<3>(set_code_data);

        context.require_auth(contract_name); 

        CAccount contract;
        WASM_ASSERT(database_account.GetAccount(nick_name(contract_name), contract),
                    account_operation_exception,
                    "wasmio_native_setcode.setcode, Contract '%s' does not exist",
                    wasm::name(contract_name).to_string()) 

        CUniversalContract contract_store;
        // ban code reset
        // WASM_ASSERT(!cw.contractCache.GetContract(contract.regid, contract_store),
        //             account_operation_exception,
        //             "wasmnativecontract.Setcode, can not reset code, contract = %s",
        //             contract_name.c_str()) 
        contract_store.vm_type = VMType::WASM_VM;
        contract_store.code    = code;
        contract_store.abi     = abi;
        contract_store.memo    = memo;

        WASM_ASSERT(database_contract.SaveContract(contract.regid, contract_store), 
                    account_operation_exception,
                    "wasmio_native_setcode.setcode, Save account error")
    }
    
    void wasmio_bank_native_transfer(wasm_context &context) {

        WASM_ASSERT( context._receiver == wasmio_bank,
                     wasm_assert_exception, 
                     "except contract wasmio.bank, but get '%s'", 
                     wasm::name(context._receiver).to_string());

        auto &database            = context.database.accountCache;
        context.control_trx.fuel += context.trx.GetSerializeSize(SER_DISK, CLIENT_VERSION) * store_fuel_fee_per_byte;

        transfer_data_type transfer_data = wasm::unpack<std::tuple<uint64_t, uint64_t, wasm::asset, string>>(context.trx.data);
        auto from                        = std::get<0>(transfer_data);
        auto to                          = std::get<1>(transfer_data);
        auto quantity                    = std::get<2>(transfer_data);
        auto memo                        = std::get<3>(transfer_data);

        context.require_auth(from); //from auth
        WASM_ASSERT(from != to,             wasm_assert_exception, "cannot transfer to self");

        WASM_ASSERT(context.is_account(to), wasm_assert_exception, "to account '%s' does not exist", wasm::name(to).to_string() );
        WASM_ASSERT(quantity.is_valid(),    wasm_assert_exception, "invalid quantity");
        WASM_ASSERT(quantity.amount > 0,    wasm_assert_exception, "must transfer positive quantity");
        WASM_ASSERT(memo.size()  <= 256,    wasm_assert_exception, "memo has more than 256 bytes");

        CAccount from_account;
        WASM_ASSERT(database.GetAccount(nick_name(from), from_account),
                    account_operation_exception,
                    "from account '%s' does not exist",
                    wasm::name(from).to_string())
        sub_balance( from_account, quantity, database );

        CAccount to_account;
        WASM_ASSERT(database.GetAccount(nick_name(to), to_account),
                    account_operation_exception,
                    "to account '%s' does not exist",
                    wasm::name(to).to_string())
        add_balance( to_account, quantity, database );

        context.require_recipient(from);
        context.require_recipient(to);

    }

}