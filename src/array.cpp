#include "array.hpp"
#include "class.hpp"
#include "memory.hpp"



namespace java {



namespace jvm {

extern Class primitive_array_class;
extern Class reference_array_class;

} // namespace jvm



Array* allocate_array(int data_bytes)
{
    static_assert(alignof(Array) == alignof(Object), "unsupported alignment");

    auto mem = (Array*)jvm::heap::allocate(sizeof(Array) + data_bytes);

    if (mem == nullptr) {
        return mem;
    }

    memset((void*)mem, 0, sizeof(Array) + data_bytes);

    return mem;
}



Array* Array::create(int size, u8 element_size, Type primitive_type)
{
    auto array = allocate_array(element_size * size);

    if (array == nullptr) {
        return array;
    }

    new (array) Array(size, element_size, primitive_type);
    array->object_.class_ = &jvm::primitive_array_class;
    return array;
}



Array* Array::create(int size, Class* class_type)
{
    auto array = allocate_array(size * sizeof(Object*));

    if (array == nullptr) {
        return array;
    }

    new (array) Array(size, class_type);
    array->object_.class_ = &jvm::reference_array_class;
    return array;
}



} // namespace java
