#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    if(!_syn_recved && !header.syn) //如果先前没有接收到syn报文而且当前报文也不含syn，那么直接忽略
        return;

    string data = seg.payload().copy(); //实际数据
    bool eof = false;

    if(!_syn_recved && header.syn){ //如果没有收到过syn报文且当前报文含syn
        _syn_recved = true; //_syn标志位设置为true
        _isn = header.seqno; //起始序列号设置为该段的序列号

        if(header.fin){ // 一个段同时 syn && fin的情形
            _fin_recved = true; //_fin标志位设置为true
            eof = true; //开启eof
        }
        _reassembler.push_substring(data, 0, eof); //第一个包，直接插入0的位置即可
        //注意syn也可以携带数据，所以仍然需要传入数据，而不是传入空串。
        return;
    }

    if(_syn_recved && header.fin){ //该段为 fin
        _fin_recved = true; //_fin标志位设置为true
        eof = true; //开启eof
    }

    //之后收到的syn报文，该位被直接忽略，当做普通报文处理
    uint64_t checkpoint = stream_out().bytes_written();
    uint64_t abs_seqno = unwrap(header.seqno, _isn, checkpoint); //序列号转为包含syn和fin在内的绝对序列号
    uint64_t stream_index = abs_seqno - 1; //将序列号转换为流中的的索引
    _reassembler.push_substring(data, stream_index, eof); //只有fin报文能够设置eof位
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    //即使是只收到了syn报文包，也要回应一次ack+1
    if(!_syn_recved){
        return nullopt;
    }

    uint64_t abs_seqno = stream_out().bytes_written();

    if(_fin_recved && stream_out().input_ended())
        abs_seqno++; //stream_index + 1 -> FIN of absolute seqno


    return optional<WrappingInt32>(wrap(abs_seqno+1, _isn));
 }

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
