#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

size_t 
StreamReassembler::merge_substring(std::string& data, uint64_t &index, std::set<TypeUnassembled>::iterator iter){
    string s = iter->data;
    size_t l1 = index, r1 = index + data.size()-1;
    size_t l2 = iter->index, r2 = l2 + iter->data.size()-1;
    if(l2 > r1 + 1 || l1 > r2 + 1)//第二个左大于第一个右，代表分离，无法合并。
        return 0;
    index = min(l1, l2);
    size_t deleteNum = s.size();
    if(l1 <= l2){
        if(r1 < r2){//如果data块包含s，那么这条语句不会执行。
            data += std::string(s.begin() + r1 - l2 + 1, s.end());
        }
    } else {//l2 < l1
        if(r2 < r1){
            s += std::string(data.begin() + r2 - l1 + 1, data.end());
        }
        data.assign(s);
    }
    return deleteNum;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(data.empty() || index + data.size() <= _firstUnassembled){
        _eof |= eof;
        if (_eof)
            _output.end_input();
        return;
    }


    size_t firstUnacceptable = _firstUnassembled + _output.remaining_capacity();

    if (index + data.size() <= firstUnacceptable){
        _eof |= eof;
    }//大于数据量的时候就不要设EOF了

    std::set<TypeUnassembled>::iterator iter;
    size_t resIndex = index;
    string resData = std::string(data); 

    if (resIndex < _firstUnassembled) {//数据大小超过了head_index
        resData = resData.substr(_firstUnassembled - resIndex);//截掉重合包，并将数据和索引都更新为
        resIndex = _firstUnassembled;//非重合部分
    }

    if (resIndex + resData.size() > firstUnacceptable)//如果已经大于ByteStream界限，那么截取
        resData = resData.substr(0, firstUnacceptable - resIndex);//至流的最大值

    iter = _Unassembled.lower_bound(TypeUnassembled(resIndex,resData));//不小于resIndex的值
    while(iter != _Unassembled.begin()){
        if(iter == _Unassembled.end())
            iter--;

        if(size_t deleteNum = merge_substring(resData, resIndex, iter)){
            _nUnassembled -= deleteNum;
            if(iter != _Unassembled.begin())
                _Unassembled.erase(iter--);
            else{
                _Unassembled.erase(iter);
                break;
            }
        } else 
            break;
    }//向前合并。 //necessary, (bcd, 1) (c, 3) 

    iter = _Unassembled.lower_bound(TypeUnassembled(resIndex,resData));//不小于resIndex的值
    while(iter != _Unassembled.end()){
        if(size_t deleteNum = merge_substring(resData, resIndex, iter)){
            _nUnassembled-=deleteNum;
            _Unassembled.erase(iter++);
        } else 
            break;
    }

    if(resIndex <= _firstUnassembled){
        size_t written_size = _output.write(string(resData.begin() + _firstUnassembled - resIndex, resData.end()));
        _firstUnassembled += written_size;
    } else {
        _Unassembled.insert(TypeUnassembled(resIndex,resData));
        _nUnassembled += resData.size();
    }

    if (_eof && empty()) {
        _output.end_input();
    }
    return;
}


size_t StreamReassembler::unassembled_bytes() const { return _nUnassembled; }

bool StreamReassembler::empty() const { return _nUnassembled == 0; }
