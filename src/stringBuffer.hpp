#include "buffer.hpp"



template <u32 Capacity> class StringBuffer {
public:
    using Buffer = ::Buffer<char, Capacity + 1>;

    StringBuffer(const char* init)
    {
        mem_.push_back('\0');
        (*this) += init;
    }

    StringBuffer()
    {
        mem_.push_back('\0');
    }

    StringBuffer(const StringBuffer& other)
    {
        clear();

        for (auto it = other.begin(); it not_eq other.end(); ++it) {
            push_back(*it);
        }
    }

    template <u32 OtherCapacity>
    StringBuffer(const StringBuffer<OtherCapacity>& other)
    {
        static_assert(OtherCapacity <= Capacity, "");

        clear();

        for (auto it = other.begin(); it not_eq other.end(); ++it) {
            push_back(*it);
        }
    }

    const StringBuffer& operator=(const StringBuffer& other)
    {
        clear();

        for (auto it = other.begin(); it not_eq other.end(); ++it) {
            push_back(*it);
        }
        return *this;
    }

    template <u32 OtherCapacity>
    const StringBuffer& operator=(const StringBuffer<OtherCapacity>& other)
    {
        clear();

        for (auto it = other.begin(); it not_eq other.end(); ++it) {
            push_back(*it);
        }
        return *this;
    }

    const StringBuffer& operator=(StringBuffer&&) = delete;

    char& operator[](int pos)
    {
        return mem_[pos];
    }

    void push_back(char c)
    {
        if (not mem_.full()) {
            mem_[mem_.size() - 1] = c;
            mem_.push_back('\0');
        }
    }

    void pop_back()
    {
        mem_.pop_back();
        mem_.pop_back();
        mem_.push_back('\0');
    }

    typename Buffer::Iterator begin() const
    {
        return mem_.begin();
    }

    typename Buffer::Iterator end() const
    {
        return mem_.end() - 1;
    }

    typename Buffer::Iterator insert(typename Buffer::Iterator pos, char val)
    {
        return mem_.insert(pos, val);
    }

    StringBuffer& operator+=(const char* str)
    {
        while (*str not_eq '\0') {
            push_back(*(str++));
        }
        return *this;
    }

    template <u32 OtherCapacity>
    StringBuffer& operator+=(const StringBuffer<OtherCapacity>& other)
    {
        (*this) += other.c_str();
        return *this;
    }

    StringBuffer& operator=(const char* str)
    {
        this->clear();

        *this += str;

        return *this;
    }

    bool operator==(const char* str)
    {
        return str_cmp(str, this->c_str()) == 0;
    }

    bool full() const
    {
        return mem_.full();
    }

    u32 length() const
    {
        return mem_.size() - 1;
    }

    bool empty() const
    {
        return mem_.size() == 1;
    }

    void clear()
    {
        mem_.clear();
        mem_.push_back('\0');
    }

    const char* c_str() const
    {
        return mem_.data();
    }

    u32 remaining() const
    {
        return (mem_.capacity() - 1) - mem_.size();
    }

private:
    Buffer mem_;
};
