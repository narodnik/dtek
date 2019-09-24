#ifndef DARK_BLOCKCHAIN_HPP
#define DARK_BLOCKCHAIN_HPP

#include <ctime>
#include <bitcoin/system.hpp>
#include <bitcoin/database/primitives/record_manager.hpp>
#include <bitcoin/database/memory/file_storage.hpp>

namespace dark {

namespace bcs = bc::system;

typedef uint32_t output_index_type;

constexpr size_t blockchain_record_size = bcs::ec_compressed_size + 4;

class blockchain
{
public:
    blockchain(const char* prefix = "blockchain");
    ~blockchain();

    // non-copyable
    blockchain(const blockchain&) = delete;

    output_index_type put(const bcs::ec_compressed& point);
    const uint8_t* get(const output_index_type index) const;

    void remove(const output_index_type index);
    bool exists(const output_index_type index);

    output_index_type count() const;

private:
    typedef std::unique_ptr<bc::database::file_storage> storage_uniq;

    typedef bc::database::record_manager<output_index_type> records_type;
    typedef std::unique_ptr<records_type> records_uniq;

    output_index_type next_available_record();

    storage_uniq records_storage_;
    records_uniq records_;
};

} // namespace dark

#endif

