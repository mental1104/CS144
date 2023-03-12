#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):_capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t len = data.length();     //取得数据的长度
    if(len > remaining_capacity())  //如果长度大于剩余量，那么
        len = remaining_capacity(); //将数据的长度设置为等于剩余量
    _write_count += len;            //更新写入的数量
    _buffer.append(BufferList(move(string().assign(data.begin(),data.begin()+len))));
    //将数据写入buffer，剩余的可能被切断
    return len; //返回写入的量
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len; //读取长度
    if(length > _buffer.size()) //吐出bufferlist里现有的内容，故不能大于bufferlist的总大小。
        length = _buffer.size();
    string s = _buffer.concatenate();
    return string().assign(s.begin(), s.begin()+length); //返回该流中前length个数据
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = len;
    if(length > _buffer.size())
        length = _buffer.size();
    _read_count += length; 
    //更新已读的量，和write不同，read不会把读完的数据清除, 所以需要在这一步更新。
    _buffer.remove_prefix(length);
    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string readStr = peek_output(len);//先读取
    pop_output(len);//再弹出
    return readStr;
}

void ByteStream::end_input() {  _input_ended_flag = true;  }

bool ByteStream::input_ended() const { return _input_ended_flag; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
