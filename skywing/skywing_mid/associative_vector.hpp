#ifndef ASSOCIATIVE_VECTOR_HPP
#define ASSOCIATIVE_VECTOR_HPP

#include <fstream>
#include <iostream>
#include <sstream>

#include "skywing_mid/pubsub_converter.hpp"

namespace skywing
{

// Typecasting helpers
template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept Stringy = std::is_same_v<T, std::string>;

// string to integral
template <std::integral out_T>
out_T type_cast_helper(std::string const& input)
{
    return std::stoll(input);
}

// string to floating point
template <std::floating_point out_T>
out_T type_cast_helper(std::string const& input)
{
    return std::stold(input);
}

// string to string
template <Stringy out_T>
out_T type_cast_helper(std::string const& input)
{
    return input;
}

// arithmetic to string
template <Stringy out_T>
out_T type_cast_helper(Arithmetic auto const& input)
{
    return std::to_string(input);
}

// arithmetic to arithmetic
template <Arithmetic out_T>
out_T type_cast_helper(Arithmetic auto const& input)
{
    return input;
}

// helper function for checking the delimiter
// WM: todo - should really improve this...
// This goes down a list checking for possible
// delimiters in the order '\t', '|', ';', ',', ':',
// and defaults to spaces if none of the above are found.
inline char check_delimiter(std::string const& line)
{
    std::vector<char> possible_delimiters = {'\t', '|', ';', ',', ':'};
    for (const auto& d : possible_delimiters) {
        if (line.find(d) != std::string::npos) {
            return d;
        }
    }
    return ' ';
}

/**
* @class AssociativeVector
* @brief A data structure that combines properties of a vector and a map, allowing for associative access and mathematical operations.
*
* @tparam index_t The type of the keys used for indexing the vector.
* @tparam val_t The type of the values stored in the vector.
* @tparam isOpen Whether the vector is open (allows dynamic key insertion) or closed (fixed keys).
*
* This class provides a vector-like interface with associative access, allowing elements to be accessed and modified using keys.
* It supports various mathematical operations such as addition, subtraction, scalar multiplication, and dot product.
*/
template <typename index_t = std::uint32_t,
          typename val_t = double,
          bool isOpen = true>
class AssociativeVector
{
public:
    /**
    * @brief Default constructor.
    * Initializes an empty AssociativeVector with a default value.
    */
    AssociativeVector(val_t default_value = 0) : default_value_(default_value)
    {}

    /**
    * @brief Constructor for closed AssociativeVector.
    * @param keys A vector of keys to initialize the AssociativeVector.
    * @param default_value The default value for uninitialized keys.
    */
    AssociativeVector(std::vector<index_t>&& keys, val_t default_value = 0)
        : default_value_(default_value)
    {
        for (auto it : keys)
            data_[it] = default_value_;
    }
    /**
    * @brief Constructs an AssociativeVector from an unordered map.
    * 
    * @param data An unordered map containing key-value pairs to initialize the AssociativeVector.
    * 
    * This constructor allows the AssociativeVector to be initialized with a set of key-value pairs
    * provided in an unordered map. This is useful for scenarios where you have existing data in a map
    * and want to leverage the mathematical and associative capabilities of the AssociativeVector.
    */
    AssociativeVector(std::unordered_map<index_t, val_t> data,
                      val_t default_value = 0)
        : default_value_(default_value), data_(std::move(data))
    {}

    /**
    * @brief Conversion constructor for AssociativeVector.
    * 
    * @param other An AssociativeVector with the same key and value types but opposite openness state.
    * 
    * This constructor allows for the conversion between an open and a closed AssociativeVector.
    * It initializes the new AssociativeVector with the default value and data from the provided
    * AssociativeVector. This is useful when you need to switch between open and closed states
    * while preserving the data and default value.
    */
    AssociativeVector(AssociativeVector<index_t, val_t, !isOpen> other)
        : default_value_(other.default_value_), data_(other.data_)
    {}

    /**
    * @brief Accesses or modifies the value associated with a given key.
    * @param ind The key to access.
    * @return A reference to the value associated with the key.
    */
    val_t& operator[](const index_t& ind)
    {
        if constexpr (isOpen) {
            if (!contains(ind))
                data_[ind] = default_value_;
            return data_[ind];
        }
        else {
            // if not, this vector is closed, so throw an error if it's not
            // available.
            if (!contains(ind))
                throw std::runtime_error(
                    "AssociativeVector::operator[] Attempted to access a "
                    "nonexistent index in a closed vector.");
            return data_[ind];
        }
    }

    /**
    * @brief Modifies the value associated with a given key.
    * @param ind The key to access.
    * @param value The value to set the key to.
    */
    void set(index_t ind, val_t value)
    {
      if constexpr (!isOpen) {
	if (!contains(ind))
        throw std::runtime_error("AssociativeVector::set Attempted to set a "
                                 "nonexistet index in a closed vector.");
      }
      data_[ind] = value;
    }

    /**
    * @brief Accesses the value associated with the specified index.
    * 
    * @param ind The index (key) whose associated value is to be accessed.
    * @return A constant reference to the value associated with the specified index.
    * 
    * This method provides read-only access to the value associated with a given index in the
    * AssociativeVector. If the index does not exist in the vector, the method throws an
    * `std::out_of_range` exception. This behavior is consistent with the `at` method in
    * standard associative containers like `std::map` and `std::unordered_map`.
    * 
    * @throws std::out_of_range if the index is not found in the AssociativeVector.
    */
    const val_t& at(const index_t& ind) const { return data_.at(ind); }

    /**
    * @brief Checks if the specified index exists in the AssociativeVector.
    * 
    * @param ind The index (key) to check for existence in the AssociativeVector.
    * @return `true` if the index exists in the AssociativeVector, `false` otherwise.
    * 
    * This method determines whether a given index is present in the AssociativeVector.
    * It returns `true` if the index is found, indicating that there is an associated
    * value stored in the vector. Otherwise, it returns `false`.
    * 
    * The method utilizes the `count` function of the underlying data structure, which
    * checks for the presence of the index. This is typically efficient and provides
    * a quick way to verify the existence of a key.
    */
    bool contains(const index_t& ind) const { return data_.count(ind) == 1; }


    /**
    * @brief Gets the keys of the AssociativeVector.
    * @return A vector of keys.
    */
    std::vector<index_t> get_keys() const
    {
        std::vector<index_t> keys;
        std::transform(
            data_.begin(),
            data_.end(),
            std::back_inserter(keys),
            [](const typename std::unordered_map<index_t, val_t>::value_type&
                   pair) { return pair.first; });
        return keys;
    }
    /**
    * @brief Computes the dot product with another AssociativeVector.
    * @param other The AssociativeVector to compute the dot product with.
    * @return The dot product result.
    */
    val_t dot(const AssociativeVector<index_t, val_t, isOpen>& b) const
    {
        val_t result = 0;
        for (auto&& iter : b.data_) {
            if (contains(iter.first))
                result += data_.at(iter.first) * iter.second;
        }
        return result;
    }

    /**
    * @brief Sets all values in the AssociativeVector equal to the given value.
    * @param x The value to set equal to.
    * @return A reference to this AssociativeVector.
    */
    AssociativeVector&
    operator=(const val_t& x)
    {
        for (auto iter : data_) {
            const index_t& ind = iter.first;
            data_[ind] = x;
        }
        return *this;
    }
    /**
    * @brief Adds another AssociativeVector to this one.
    * @param other The AssociativeVector to add.
    * @return A reference to this AssociativeVector.
    */
    AssociativeVector&
    operator+=(const AssociativeVector<index_t, val_t, isOpen>& b)
    {
        for (auto iter : b.data_) {
            const index_t& ind = iter.first;
            if constexpr (isOpen)
            { // add key k to data_ if it isn't there already
                if (!contains(ind))
                    data_[ind] = default_value_;
                data_[ind] += b.at(ind);
            }
            else {
                // is !isOpen, only add on this index if it's already in data_
                if (contains(ind))
                    data_[ind] += b.at(ind);
            }
        }
        return *this;
    }
    /**
    * @brief Subtracts another AssociativeVector from this one.
    * @param other The AssociativeVector to subtract.
    * @return A reference to this AssociativeVector.
    */
    AssociativeVector&
    operator-=(const AssociativeVector<index_t, val_t, isOpen>& b)
    {
        for (auto iter : b.data_) {
            const index_t& ind = iter.first;
            if constexpr (isOpen)
            { // add key k to data_ if it isn't there already
                if (!contains(ind))
                    data_[ind] = default_value_;
                data_[ind] -= b.at(ind);
            }
            else {
                // is !isOpen, only subtract on this index if it's already in
                // data_
                if (contains(ind))
                    data_[ind] -= b.at(ind);
            }
        }
        return *this;
    }

    /**
    * @brief Multiplies this AssociativeVector by a scalar.
    * @param scalar The scalar to multiply by.
    * @return A reference to this AssociativeVector.
    */
    template <typename float_t>
    AssociativeVector& operator*=(float_t f)
    {
        for (auto&& iter : data_)
            iter.second *= f;
        return *this;
    }
    /**
    * @brief Divides each value in the AssociativeVector by a scalar.
    * 
    * @param f The scalar by which each value in the AssociativeVector will be divided.
    * @return A reference to the modified AssociativeVector.
    */
    template <typename float_t>
    AssociativeVector& operator/=(float_t f)
    {
        for (auto&& iter : data_)
            iter.second /= f;
        return *this;
    }

    /**
    * @brief Compares two AssociativeVectors for equality.
    * 
    * @param other The AssociativeVector to compare against.
    * @return `true` if both AssociativeVectors have the same keys and values, `false` otherwise.
    * 
    * This operator checks if the current AssociativeVector is equal to another by comparing
    * both the keys and their corresponding values. Two AssociativeVectors are considered equal
    * if they contain the same key-value pairs.
    */
    bool operator==(const AssociativeVector& other) const {
        // Check if sizes are equal
        if (this->size() != other.size()) {
            return false;
        }

        // Check if all keys and corresponding values are equal
        for (const auto& key : this->get_keys()) {
            if (this->at(key) != other.at(key)) {
                return false;
            }
        }

        return true;
    }

    /**
    * @brief Compares two AssociativeVectors for inequality.
    * 
    * @param other The AssociativeVector to compare against.
    * @return `true` if the AssociativeVectors differ in keys or values, `false` if they are identical.
    */
    bool operator!=(const AssociativeVector& other) const {
        return !(*this == other);
    }

    /**
    * @brief Retrieves the default value for the AssociativeVector.
    * 
    * @return The default value (`val_t`) used for uninitialized keys.
    * 
    * This method returns the default value that is assigned to keys in the
    * AssociativeVector when they are accessed but not explicitly set.
    */
    val_t get_default_value() const { return default_value_; }

    /**
    * @brief Returns the number of key-value pairs in the AssociativeVector.
    * 
    * @return The number of elements (`size_t`) currently stored in the AssociativeVector.
    * 
    * This method provides the current count of key-value pairs stored in the
    * AssociativeVector, reflecting its size.
    */
    size_t size() const { return data_.size(); }

    // Iterator types
    using iterator = typename std::unordered_map<index_t, val_t>::iterator;
    using const_iterator = typename std::unordered_map<index_t, val_t>::const_iterator;

    // Begin and end functions for iteration
    iterator begin() {
        return data_.begin();
    }

    const_iterator begin() const {
        return data_.begin();
    }

    iterator end() {
        return data_.end();
    }

    const_iterator end() const {
        return data_.end();
    }

private:
    val_t default_value_; //Stores the default value assigned to keys that are accessed but not explicitly set.
    std::unordered_map<index_t, val_t> data_; //The underlying data structure that holds the key-value pairs in the AssociativeVector.

    template <typename I, typename V, bool O>
    friend AssociativeVector<I, V, O>
    operator-(const AssociativeVector<I, V, O>& a);// A friend function that allows subtraction operations on AssociativeVector instances.
    // A friend function that enables output streaming of an AssociativeVector to an output stream, typically for debugging or logging purposes.
    template <typename I, typename V, bool O>
    friend std::ostream& operator<<(std::ostream& out,
                                    const AssociativeVector<I, V, O>& a);
    // A friend class that allows access to private members of an AssociativeVector with the opposite isOpen template parameter.
    friend class AssociativeVector<index_t, val_t, !isOpen>;
    // A friend struct that facilitates conversion between AssociativeVector and a pub-sub compatible format.
    friend struct PubSubConverter<AssociativeVector<index_t, val_t, isOpen>>;
}; // class AssociativeVector


/**
* @brief Adds two AssociativeVectors element-wise.
* 
* @param a The first AssociativeVector.
* @param b The second AssociativeVector.
* @return A new AssociativeVector containing the element-wise sum of `a` and `b`.
* 
* This operator creates a new AssociativeVector by adding corresponding elements
* from two input AssociativeVectors. If a key exists in one vector but not the other,
* the missing value is considered as the default value.
*/
template <typename index_t, typename val_t, bool isOpen>
AssociativeVector<index_t, val_t, isOpen>
operator+(const AssociativeVector<index_t, val_t, isOpen>& a,
          const AssociativeVector<index_t, val_t, isOpen>& b)
{
    AssociativeVector<index_t, val_t, true> c(a);
    c += b;
    return AssociativeVector<index_t, val_t, isOpen>(c);
}

/**
* @brief Subtracts one AssociativeVector from another element-wise.
* 
* @param a The AssociativeVector to subtract from.
* @param b The AssociativeVector to subtract.
* @return A new AssociativeVector containing the element-wise difference of `a` and `b`.
* 
* This operator creates a new AssociativeVector by subtracting corresponding elements
* of `b` from `a`. If a key exists in one vector but not the other, the missing value
* is considered as the default value.
*/
template <typename index_t, typename val_t, bool isOpen>
AssociativeVector<index_t, val_t, isOpen>
operator-(const AssociativeVector<index_t, val_t, isOpen>& a,
          const AssociativeVector<index_t, val_t, isOpen>& b)
{
    AssociativeVector<index_t, val_t, true> c(a);
    c -= b;
    return AssociativeVector<index_t, val_t, isOpen>(c);
}

/**
* @brief Negates all values in the AssociativeVector.
* 
* @param a The AssociativeVector to negate.
* @return A new AssociativeVector with all values negated.
* 
* This operator creates a new AssociativeVector by negating each value in the input
* AssociativeVector `a`.
*/
template <typename index_t, typename val_t, bool isOpen>
AssociativeVector<index_t, val_t, isOpen>
operator-(const AssociativeVector<index_t, val_t, isOpen>& a)
{
    AssociativeVector<index_t, val_t, isOpen> new_vec(a.data_);
    for (auto&& iter : new_vec.data_)
        iter.second = -iter.second;
    return new_vec;
}

/**
* @brief Multiplies all values in the AssociativeVector by a scalar.
* 
* @param f The scalar value to multiply with.
* @param b The AssociativeVector to be scaled.
* @return A new AssociativeVector with all values multiplied by `f`.
* 
* This operator scales each value in the AssociativeVector `b` by the scalar `f`.
*/
template <typename index_t, typename val_t, bool isOpen, typename float_t>
AssociativeVector<index_t, val_t, isOpen>
operator*(float_t f, const AssociativeVector<index_t, val_t, isOpen>& b)
{
    AssociativeVector<index_t, val_t, isOpen> c = b;
    return c *= f;
}

/**
* @brief Divides all values in the AssociativeVector by a scalar.
* 
* @param b The AssociativeVector to be scaled.
* @param f The scalar value to divide by.
* @return A new AssociativeVector with all values divided by `f`.
* 
* This operator scales each value in the AssociativeVector `b` by dividing it by the scalar `f`.
*/
template <typename index_t, typename val_t, bool isOpen, typename float_t>
AssociativeVector<index_t, val_t, isOpen>
operator/(const AssociativeVector<index_t, val_t, isOpen>& b, float_t f)
{
    AssociativeVector<index_t, val_t, isOpen> c = b;
    return c /= f;
}

template <typename index_t, typename val_t, bool isOpen>
std::ostream& operator<<(std::ostream& out,
                         const AssociativeVector<index_t, val_t, isOpen>& a)
{
    out << "[ ";
    for (const auto& iter : a.data_) {
        out << "(";
        out << iter.first;
        out << ", ";
        out << iter.second;
        out << ") ";
    }
    out << "]";
    return out;
}

/**
 * @brief Read data from file into an AssociativeVector.
 *
 * @param filename The filename to read from.
 * @param rowList Optional list of row keys to include (if none is passed, all
 * rows are included).
 * @param read_keys Flag indicating whether to read keys (if false, index by
 * integers from 0).
 * @return The AssociativeVector
 *
 * This method reads in data from file with optional keys.
 */
template <typename index_t, typename val_t, bool isOpen>
AssociativeVector<index_t, val_t, isOpen> ReadAssocitiveVector(
    std::string filename,
    const std::vector<index_t> rowList = std::vector<index_t>(),
    bool read_keys = false)
{
    // Open file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::stringstream ss;
        ss << filename << " file not found!";
        throw std::invalid_argument(ss.str());
    }

    std::string line;
    size_t i = 0;
    index_t key;
    std::string val;
    std::unordered_map<index_t, val_t> map;
    size_t expected_row_size = read_keys ? 2 : 1;
    size_t val_idx = read_keys ? 1 : 0;
    char delimiter = '\0';

    // Read file and parse values
    while (std::getline(file, line)) {
        // Check for the delimiter on the first line
        if (delimiter == '\0') {
            delimiter = check_delimiter(line);
        }
        // Read items in the line
        std::stringstream ss(line);
        std::vector<std::string> line_vals;
        while (std::getline(ss, val, delimiter)) {
            line_vals.push_back(val);
        }
        // Expect keys to be in the first column (if present), values in the
        // second column
        if (line_vals.size() != expected_row_size) {
            std::stringstream ss;
            ss << "ERROR: wrong number of columns in file " << filename
               << " when reading AssociativeVector";
            throw std::runtime_error(ss.str());
        }
        key = read_keys ? type_cast_helper<index_t>(line_vals[0])
                        : type_cast_helper<index_t>(i++);
        // If the row list is empty, include all rows, otherwise
        // check to see if the row key was in the requested list.
        if (rowList.empty()
            || std::find(rowList.begin(), rowList.end(), key) != rowList.end())
        {
            map[key] = type_cast_helper<val_t>(line_vals[val_idx]);
        }
    }

    return AssociativeVector<index_t, val_t, isOpen>(map);
}

/**************************************************
 * Pubsub conversion
 *************************************************/

template <typename index_t, typename val_t, bool isOpen>
struct PubSubConverter<AssociativeVector<index_t, val_t, isOpen>>
{
    using input_type = AssociativeVector<index_t, val_t, isOpen>;
    using map_type = std::unordered_map<index_t, val_t>;
    using data_pubsub_t = PubSub_t<map_type>;
    using before_final_t = std::tuple<PubSub_t<val_t>, data_pubsub_t>;
    using pubsub_type = PubSub_t<before_final_t>;

    /**
    * @brief Converts an AssociativeVector to its pub-sub representation.
    * 
    * @param input The AssociativeVector to convert.
    * @return The pub-sub representation of the AssociativeVector.
    * 
    * This method serializes the `AssociativeVector` into a format suitable for
    * pub-sub systems, encapsulating both the default value and the data map.
    */
    static pubsub_type convert(input_type input)
    {
        before_final_t bf(
            PubSubConverter<val_t>::convert(input.default_value_),
            PubSubConverter<std::unordered_map<index_t, val_t>>::convert(
                input.data_));
        return PubSubConverter<before_final_t>::convert(bf);
    }

    /**
    * @brief Converts a pub-sub representation back to an AssociativeVector.
    * 
    * @param ps_input The pub-sub representation to convert.
    * @return The reconstructed AssociativeVector.
    * 
    * This method deserializes the pub-sub format back into an `AssociativeVector`,
    * reconstructing both the default value and the data map.
    */
    static input_type deconvert(pubsub_type ps_input)
    {
        before_final_t bf =
            PubSubConverter<before_final_t>::deconvert(ps_input);
        val_t default_value =
            PubSubConverter<val_t>::deconvert(std::get<0>(bf));
        map_type data = PubSubConverter<map_type>::deconvert(std::get<1>(bf));
        return AssociativeVector<index_t, val_t, isOpen>(data, default_value);
    }
}; // struct

} // namespace skywing

#endif // ASSOCIATIVE_VECTOR_HPP
