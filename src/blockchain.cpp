#include <dark/blockchain.hpp>

#include <memory>
#include <boost/filesystem.hpp>

namespace dark {

namespace fs = boost::filesystem;

void touch_file(const fs::path& filename)
{
    std::ofstream file(filename.native());
    file.put(0);
}

std::string filepath(fs::path prefix, const char* filename)
{
    prefix /= filename;
    return prefix.native();
}

blockchain::blockchain(const char* prefix)
{
    bool create_new = false;
    if (fs::create_directories(prefix))
    {
        touch_file(filepath(prefix, "outputs"));
        create_new = true;
    }

    records_storage_ = std::make_unique<bc::database::file_storage>(
        filepath(prefix, "outputs"));
    records_ = std::make_unique<records_type>(
        *records_storage_, 0, blockchain_record_size);

    records_storage_->open();
    if(create_new)
    {
        records_->create();
    }
    records_->start();
}

blockchain::~blockchain()
{
}

output_index_type blockchain::next_available_record()
{
    const auto chain_count = count();
    for (size_t i = 0; i < chain_count; ++i)
        if (!exists(i))
            return i;
    const auto index = records_->allocate(1);
    records_->commit();
    return index;
}

output_index_type blockchain::put(const bc::ec_compressed& point)
{
    const auto new_record_index = next_available_record();
    auto memory = records_->get(new_record_index);
    auto* buffer = memory->buffer();
    std::copy(point.begin(), point.end(), buffer);
    // Write time
    auto serial = bc::make_unsafe_serializer(buffer + bc::ec_compressed_size);
    const auto time = std::time(nullptr);
    serial.write_4_bytes_little_endian(time);
    return new_record_index;
}

const uint8_t* blockchain::get(const output_index_type index) const
{
    auto memory = records_->get(index);
    const auto* buffer = memory->buffer();
    return buffer;
}

void blockchain::remove(const output_index_type index)
{
    auto memory = records_->get(index);
    auto* buffer = memory->buffer();
    BITCOIN_ASSERT(buffer[0] == 2 || buffer[0] == 3);
    buffer[0] = 0;
}
bool blockchain::exists(const output_index_type index)
{
    auto memory = records_->get(index);
    const auto* buffer = memory->buffer();
    if (buffer[0] != 0)
    {
        BITCOIN_ASSERT(buffer[0] == 2 || buffer[0] == 3);
        return true;
    }
    return false;
}

output_index_type blockchain::count() const
{
    return records_->count();
}

} // namespace

