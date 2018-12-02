#include <dark/blockchain.hpp>

#include <boost/filesystem.hpp>
#include <boost/range/irange.hpp>

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
        *records_storage_, 0, record_size);

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
    for (auto i: boost::irange(count()))
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
    return new_record_index;
}

bc::ec_compressed blockchain::get(const output_index_type index) const
{
    auto memory = records_->get(index);
    const auto* buffer = memory->buffer();
    bc::ec_compressed result;
    std::copy(buffer, buffer + bc::ec_compressed_size, result.begin());
    return result;
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

