#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory
{
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity)
    {
    }

    RawMemory(const RawMemory& other) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(std::exchange(other.capacity_, 0))
    {
    }

    ~RawMemory()
    {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept
    {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept
    {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept
    {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept
    {
        assert(index <= capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept
    {
        return buffer_;
    }

    T* GetAddress() noexcept
    {
        return buffer_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n)
    {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept
    {
        operator delete(buf);
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};




template <typename T>
class Vector
{
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() noexcept = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0))
    {
    }

    Vector& operator=(const Vector& rhs)
    {
        if (this == &rhs)
            return *this;

        if (rhs.size_ > data_.Capacity())
        {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        }
        else
        {
            size_t i = 0;
            for (; i < size_ && i < rhs.Size(); i++)
                data_[i] = rhs[i];

            if (size_ < rhs.Size())
                std::uninitialized_copy_n(rhs.data_ + i, rhs.Size() - i, data_ + i);
            else // если у другого вектора было больше инициализированных элементов
                std::destroy_n(data_ + rhs.Size(), size_ - rhs.Size());

            size_ = rhs.Size();
        }

        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept
    {
        if (this != &rhs)
            Swap(rhs);

        return *this;
    }

    const T& operator[](size_t index) const noexcept
    {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept { return data_.GetAddress(); }
    iterator end() noexcept {return (size_ != 0) ? &data_[size_] : begin(); }

    const_iterator begin() const noexcept { return data_.GetAddress(); }
    const_iterator end() const noexcept { return (size_) ? &data_[size_] : begin(); }

    const_iterator cbegin() const noexcept { return data_.GetAddress(); }
    const_iterator cend() const noexcept { return (size_) ? &data_[size_] : begin(); }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    bool Empty() const noexcept
    {
        return size_ == 0;
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity())
            return;

        RawMemory<T> new_data(new_capacity);

        CopyDataRange(data_.GetAddress(), size_, new_data.GetAddress());

        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), new_data.Capacity());
    }

    void Resize(size_t new_size)
    {
        if (new_size >= Capacity())
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        else
        {
            for (size_t i = new_size; i < size_; i++)
                data_[i].~T();
        }
        size_ = new_size;
    }

    template <class Value>
    void PushBack(Value&& value)
    {
        EmplaceBack(std::forward<Value>(value));
    }

    void PopBack() noexcept
    {
        if (!Empty())
            data_[--size_].~T();
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        if (size_ == Capacity())
        {
            RawMemory<T> new_data((Capacity() == 0) ? 1 : 2 * Capacity());
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);

            // constexpr оператор if будет вычислен во время компиляции
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            else
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), new_data.Capacity());
            ++size_;
        }
        else
        {
            new (data_.GetAddress() + size_)  T(std::forward<Args>(args)...);
            ++size_;
        }

        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        size_t value_pos = pos - begin();

        if (size_ == Capacity())
        {
            // Создаём новую область памяти
            RawMemory<T> new_data((Capacity() == 0) ? 1 : 2 * Capacity());

            // Вставляем элемент в новую область
            new (new_data.GetAddress() + value_pos) T(std::forward<Args>(args)...);

            // Добавляем значения до pos в новую область памяти
            CopyDataRange(data_.GetAddress(), value_pos, new_data.GetAddress());

            // Добавляем значения от pos до end в новую область памяти
            CopyDataRange(data_.GetAddress() + value_pos, size_ - value_pos, new_data.GetAddress() + value_pos + 1);

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), new_data.Capacity());
        }
        else if (pos == end())
        {
            new(end()) T(std::forward<Args>(args)...);
        }
        else
        {
            new(end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(&data_[value_pos], end() - 1, end());
            data_[value_pos] = T(std::forward<Args>(args)...);
        }

        ++size_;
        return &data_[value_pos];
    }

    template <class Value>
    iterator Insert(const_iterator pos, Value&& value)
    {
        return Emplace(pos, std::forward<Value>(value));
    }

    iterator Erase(const_iterator pos) noexcept
    {
        assert(!Empty());
        size_t value_pos = pos - begin();
        std::move(&data_[value_pos + 1], end(), &data_[value_pos]);
        PopBack();
        return &data_[value_pos];
    }

    void Swap(Vector& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

private:
    constexpr void CopyDataRange(T* source, size_t count, T* dest)
    {
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            std::uninitialized_move_n(source, count, dest);
        else
            std::uninitialized_copy_n(source, count, dest);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};