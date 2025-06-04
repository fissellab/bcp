#pragma once

#include "../sample.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

typedef uint32_t SeqNum;

// Represents a segment of a greater piece of data
struct Chunk {
    SeqNum seq_num; // Unique to each chunk for a given piece of data
    size_t offset;  // byte offset of the chunk in the data
    std::unique_ptr<std::vector<uint8_t>> data; // The data segment of the chunk
};

// Represents a piece of data that has been segmented
class Chunker
{
  public:
    Chunker(std::vector<uint8_t>&& data, size_t max_chunk_size);

    // get chunk by its unique sequence number
    // seq_num must be in the range [0, num_chunks)
    Chunk get_chunk(SeqNum seq_num);

    // get the total number of chunks
    unsigned int get_num_chunks();

  private:
    // get the byte offset of the chunk in the data
    size_t get_chunk_offset(SeqNum seq_num);

    // get the size of the chunk in bytes
    size_t get_chunk_size(SeqNum seq_num);

    // the piece of data that we get the segments from
    std::vector<uint8_t> data_;

    // the size of each chunk possibly excluding the last one
    size_t normal_chunk_size_;

    // the total number of chunks
    unsigned int num_chunks_;
};
