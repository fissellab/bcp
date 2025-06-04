#include "chunker.hpp"
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

Chunker::Chunker(std::vector<uint8_t>&& data,
                 size_t max_chunk_size)
    : data_(std::move(data)), normal_chunk_size_(max_chunk_size)
{
    if(data_.size() == 0) {
        throw std::invalid_argument("Data cannot be empty");
    }
    num_chunks_ = static_cast<unsigned int>(
        (data_.size() + max_chunk_size - 1) / max_chunk_size);
}

Chunk Chunker::get_chunk(SeqNum seq_num)
{
    if(seq_num >= num_chunks_) {
        throw std::out_of_range("Sequence number out of range");
    }

    size_t offset = get_chunk_offset(seq_num);
    size_t size = get_chunk_size(seq_num);

    auto chunk_data = std::make_unique<std::vector<uint8_t>>(
        data_.begin() + offset, data_.begin() + offset + size);

    return Chunk{seq_num, offset, std::move(chunk_data)};
}

unsigned int Chunker::get_num_chunks() { return num_chunks_; }

size_t Chunker::get_chunk_offset(SeqNum seq_num)
{
    return seq_num * normal_chunk_size_;
}

size_t Chunker::get_chunk_size(SeqNum seq_num)
{
    if(seq_num == num_chunks_ - 1) {
        return data_.size() - get_chunk_offset(seq_num);
    }
    return normal_chunk_size_;
}