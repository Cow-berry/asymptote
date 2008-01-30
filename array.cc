/*****
 * array.cc
 * Andy Hammerlindl  2008/01/26
 * 
 * Array type used by virtual machine.
 *****/

#include "array.h"
#include "mod.h"

namespace vm {

inline size_t sliceIndex(Int in, size_t len) {
  if (in < 0)
    in += len;
  if (in < 0)
    return 0;
  size_t index = (size_t)in;
  return index < len ? index : len;
}

array *array::slice(Int left, Int right)
{
  size_t length=size();
  if (length == 0)
    return new array();

  if (cycle) {
    if (right <= left)
      return new array();

    size_t resultLength = (size_t)(right - left);
    array *result = new array(resultLength);

    size_t i = (size_t)imod(left, length), ri = 0;
    while (ri < resultLength) {
      (*result)[ri] = (*this)[i];

      ++ri;
      ++i;
      if (i >= length)
        i -= length;
    }

    return result;
  }
  else { // Non-cyclic
    size_t l = sliceIndex(left, length);
    size_t r = sliceIndex(right, length);

    if (r <= l)
      return new array();

    size_t resultLength = r - l;
    array *result = new array(resultLength);

    std::copy(this->begin()+l, this->begin()+r, result->begin());

    return result;
  }
}

void array::setNonBridgingSlice(size_t l, size_t r, mem::vector<item> *a)
{
  assert(0 <= l);
  assert(l <= r);

  size_t asize=a->size();
  if (asize == r-l) {
    // In place
    std::copy(a->begin(), a->end(), this->begin()+l);
  }
  else if (asize < r-l) {
    // Shrinking
    std::copy(a->begin(), a->end(), this->begin()+l);
    this->erase(this->begin()+l+a->size(), this->begin()+r);
  }
  else {
    // Expanding
    // NOTE: As a speed optimization, we could check capacity() to see if the
    // array can fit the new entries, and build the new array from scratch
    // (using swap()) if a new allocation is necessary.
    std::copy(a->begin(), a->begin()+r-l, this->begin()+l);
    this->insert(this->begin()+r, a->begin()+r-l, a->end());
  }
}

void array::setBridgingSlice(size_t l, size_t r, mem::vector<item> *a)
{
  size_t len=this->size();

  assert(r<=l);
  assert(r+len-l == a->size());

  std::copy(a->begin(), a->begin()+(len-l), this->begin()+l);
  std::copy(a->begin()+(len-l), a->end(), this->begin());
}

void array::setSlice(Int left, Int right, array *a)
{
  // If we are slicing an array into itself, slice in a copy instead, to ensure
  // the proper result.
  mem::vector<item> *v = (a == this) ? new mem::vector<item>(*a) : a;

  size_t length=size();
  if (cycle) {
    if (right < left)
      right = left;

    if (right == left) {
      // Notice that assigning to the slice A[A.length:A.length] is the same as
      // assigning to the slice A[0:0] for a cyclic array.
      size_t l = (size_t)imod(left, length);
      setNonBridgingSlice(l, l, v);
    }
    else {
      if (left + length < right)
        vm::error("assigning to cyclic slice with repeated entries");

      size_t l = (size_t)imod(left, length);

      // Set r to length instead of zero, so that slices that go to the end of
      // the array are properly treated as non-bridging.
      size_t r = (size_t)imod(right, length);
      if (r == 0)
        r = length;

      if (l < r)
        setNonBridgingSlice(l, r, v);
      else {
        if (r + length - l == v->size())
          setBridgingSlice(l, r, v);
        else
          vm::error("assignment to cyclic slice is not well defined");
      }
    }
  }
  else {
    size_t l=sliceIndex(left, length);
    size_t r=sliceIndex(right, length);

    if (r < l)
      r = l;

    setNonBridgingSlice(l, r, v);
  }
}

} // namespace vm