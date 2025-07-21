/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#include <algorithm>
#include <vector>

// std::priority_queue does not allow non-const access to the "top" element and therefore is impossible to use with
// unique_ptr elements...
template <typename T, typename Compare = std::less<T>>
struct max_heap
{
    using value_type = T;

    void push(const T& value)
    {
        m_data.push_back(value);
        std::push_heap(m_data.begin(), m_data.end(), m_compare);
    }

    void push(T&& value)
    {
        m_data.push_back(std::move(value));
        std::push_heap(m_data.begin(), m_data.end(), m_compare);
    }

    T pop()
    {
        std::pop_heap(m_data.begin(), m_data.end(), m_compare);
        auto value = std::move(m_data.back());
        m_data.pop_back();
        return value;
    }

    bool empty() const noexcept
    {
        return m_data.empty();
    }

    std::size_t size() const noexcept
    {
        return m_data.size();
    }

private:
    std::vector<T> m_data;
    Compare m_compare;
};
