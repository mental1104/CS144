#include "byte_stream.hh"

#include <algorithm>
#include <utility>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

namespace {
constexpr size_t CHUNK_TARGET_SIZE = 4096;
constexpr size_t DIRECT_CHUNK_THRESHOLD = CHUNK_TARGET_SIZE * 16;
constexpr size_t FRONT_COMPACT_PREFIX_THRESHOLD = CHUNK_TARGET_SIZE * 4;
constexpr size_t FRONT_COMPACT_REMAINING_THRESHOLD = CHUNK_TARGET_SIZE;
}

ByteStream::ByteStream(const size_t capacity):_capacity(capacity) {}

void ByteStream::append_copy(const char *data, const size_t len) {
    size_t copied = 0;

    while (copied < len) {
        if (_chunks.empty() || _chunks.back().size() >= CHUNK_TARGET_SIZE) {
            _chunks.emplace_back();
            _chunks.back().reserve(min(CHUNK_TARGET_SIZE, max(_capacity, size_t{1})));
        }

        string &tail = _chunks.back();
        const size_t room = CHUNK_TARGET_SIZE - tail.size();
        const size_t n = min(room, len - copied);
        tail.append(data + copied, n);
        copied += n;
    }
}

void ByteStream::compact_front_if_sparse() {
    if (_chunks.empty() || _front_offset < FRONT_COMPACT_PREFIX_THRESHOLD) {
        return;
    }

    string &front = _chunks.front();
    const size_t remaining = front.size() - _front_offset;
    if (remaining <= FRONT_COMPACT_REMAINING_THRESHOLD) {
        string compacted{front.data() + _front_offset, remaining};
        front.swap(compacted);
        _front_offset = 0;
    }
}

size_t ByteStream::write(const string &data) {
    const size_t len = min(data.length(), remaining_capacity());

    append_copy(data.data(), len);

    _write_count += len;
    _buffer_size += len;
    return len;
}

size_t ByteStream::write(string &&data) {
    const size_t len = min(data.length(), remaining_capacity());

    if (len >= DIRECT_CHUNK_THRESHOLD) {
        data.resize(len);
        _chunks.emplace_back(move(data));
    } else {
        append_copy(data.data(), len);
    }

    _write_count += len;
    _buffer_size += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    const size_t length = min(len, _buffer_size);
    string output;
    output.reserve(length);

    size_t remaining = length;
    for (auto it = _chunks.begin(); it != _chunks.end() && remaining > 0; ++it) {
        const size_t offset = (it == _chunks.begin()) ? _front_offset : 0;
        const size_t available = it->size() - offset;
        const size_t n = min(available, remaining);
        output.append(it->data() + offset, n);
        remaining -= n;
    }

    return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    const size_t length = min(len, _buffer_size);
    _read_count += length;
    _buffer_size -= length;

    size_t remaining = length;
    while (remaining > 0) {
        string &front = _chunks.front();
        const size_t available = front.size() - _front_offset;

        if (remaining < available) {
            _front_offset += remaining;
            remaining = 0;
            compact_front_if_sparse();
        } else {
            remaining -= available;
            _chunks.pop_front();
            _front_offset = 0;
        }
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const size_t length = min(len, _buffer_size);
    string output;
    output.reserve(length);

    size_t remaining = length;
    while (remaining > 0) {
        string &front = _chunks.front();
        const size_t available = front.size() - _front_offset;
        const size_t n = min(available, remaining);

        output.append(front.data() + _front_offset, n);
        remaining -= n;

        if (n < available) {
            _front_offset += n;
            compact_front_if_sparse();
        } else {
            _chunks.pop_front();
            _front_offset = 0;
        }
    }

    _read_count += length;
    _buffer_size -= length;
    return output;
}

void ByteStream::end_input() {  _input_ended_flag = true;  }

bool ByteStream::input_ended() const { return _input_ended_flag; }

size_t ByteStream::buffer_size() const { return _buffer_size; }

bool ByteStream::buffer_empty() const { return _buffer_size == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer_size; }
